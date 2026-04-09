#include "audio/StemCaptureWriter.h"

#include <algorithm>

namespace famalamajam::audio
{
namespace
{
constexpr int kWriterStopTimeoutMs = 2000;
}

StemCaptureWriter::StemCaptureWriter()
    : juce::Thread("StemCaptureWriter")
{
}

StemCaptureWriter::~StemCaptureWriter()
{
    stop();
}

void StemCaptureWriter::start()
{
    if (! isThreadRunning())
        startThread();
}

void StemCaptureWriter::stop()
{
    signalThreadShouldExit();
    workEvent_.signal();
    stopThread(kWriterStopTimeoutMs);

    std::scoped_lock lock(stateMutex_);
    commandQueue_.clear();
    closeAllWriters();
    writtenFiles_.clear();
    baseNameUseCounts_.clear();
    currentSessionDirectory_ = juce::File {};
}

void StemCaptureWriter::beginSession(const SessionConfig& config)
{
    start();
    Command command;
    command.kind = Command::Kind::BeginSession;
    command.sessionConfig = config;
    enqueue(std::move(command));
}

void StemCaptureWriter::endSession()
{
    Command command;
    command.kind = Command::Kind::EndSession;
    enqueue(std::move(command));
}

void StemCaptureWriter::retireSource(const std::string& sourceKey)
{
    if (sourceKey.empty())
        return;

    start();
    Command command;
    command.kind = Command::Kind::RetireSource;
    command.sourceKey = sourceKey;
    enqueue(std::move(command));
}

void StemCaptureWriter::pauseSession()
{
    start();
    auto flushEvent = std::make_shared<juce::WaitableEvent>();
    Command command;
    command.kind = Command::Kind::Flush;
    command.flushEvent = flushEvent;
    enqueue(std::move(command));
    (void) flushEvent->wait(kWriterStopTimeoutMs);

    std::scoped_lock lock(stateMutex_);
    closeAllWriters();
}

void StemCaptureWriter::submitFrame(const Frame& frame)
{
    if (frame.audio.getNumChannels() <= 0 || frame.audio.getNumSamples() <= 0 || frame.source.sampleRate <= 0.0)
        return;

    start();
    Command command;
    command.kind = Command::Kind::SubmitFrame;
    command.frame.source = frame.source;
    command.frame.audio.makeCopyOf(frame.audio, true);
    enqueue(std::move(command));
}

bool StemCaptureWriter::flushForTesting(int timeoutMs)
{
    start();
    auto flushEvent = std::make_shared<juce::WaitableEvent>();
    Command command;
    command.kind = Command::Kind::Flush;
    command.flushEvent = flushEvent;
    enqueue(std::move(command));
    return flushEvent->wait(timeoutMs);
}

juce::File StemCaptureWriter::getCurrentSessionDirectoryForTesting() const
{
    std::scoped_lock lock(stateMutex_);
    return currentSessionDirectory_;
}

std::vector<juce::File> StemCaptureWriter::getWrittenFilesForTesting() const
{
    std::scoped_lock lock(stateMutex_);
    std::vector<juce::File> files = writtenFiles_;

    std::sort(files.begin(),
              files.end(),
              [](const juce::File& lhs, const juce::File& rhs) { return lhs.getFullPathName() < rhs.getFullPathName(); });
    return files;
}

void StemCaptureWriter::run()
{
    while (! threadShouldExit())
    {
        workEvent_.wait(25);

        while (! threadShouldExit())
        {
            Command command;
            {
                std::scoped_lock lock(stateMutex_);
                if (commandQueue_.empty())
                    break;

                command = std::move(commandQueue_.front());
                commandQueue_.pop_front();
            }

            processCommand(command);
        }
    }
}

void StemCaptureWriter::enqueue(Command command)
{
    {
        std::scoped_lock lock(stateMutex_);
        commandQueue_.push_back(std::move(command));
    }

    workEvent_.signal();
}

void StemCaptureWriter::processCommand(Command& command)
{
    switch (command.kind)
    {
        case Command::Kind::BeginSession:
            handleBeginSession(command.sessionConfig);
            return;

        case Command::Kind::EndSession:
            closeAllWriters();
            return;

        case Command::Kind::RetireSource:
            handleRetireSource(command.sourceKey);
            return;

        case Command::Kind::SubmitFrame:
            handleSubmitFrame(command.frame);
            return;

        case Command::Kind::Flush:
            if (command.flushEvent != nullptr)
                command.flushEvent->signal();
            return;
    }
}

void StemCaptureWriter::closeAllWriters()
{
    for (auto& [sourceKey, state] : writersBySourceKey_)
    {
        (void) sourceKey;
        state.writer.reset();
    }

    writersBySourceKey_.clear();
}

void StemCaptureWriter::handleRetireSource(const std::string& sourceKey)
{
    if (const auto writerIt = writersBySourceKey_.find(sourceKey); writerIt != writersBySourceKey_.end())
    {
        writerIt->second.writer.reset();
        writersBySourceKey_.erase(writerIt);
    }
}

void StemCaptureWriter::handleBeginSession(const SessionConfig& config)
{
    closeAllWriters();
    writtenFiles_.clear();
    baseNameUseCounts_.clear();

    if (config.baseDirectory == juce::File())
    {
        currentSessionDirectory_ = juce::File {};
        return;
    }

    currentSessionDirectory_ = config.baseDirectory.getChildFile(makeSessionFolderName(config));
    currentSessionDirectory_.createDirectory();
}

void StemCaptureWriter::handleSubmitFrame(const Frame& frame)
{
    if (currentSessionDirectory_ == juce::File())
        return;

    auto writerIt = writersBySourceKey_.find(frame.source.sourceKey);
    if (writerIt == writersBySourceKey_.end())
    {
        auto file = resolveFileForSource(frame.source);
        file.getParentDirectory().createDirectory();

        auto stream = std::make_unique<juce::FileOutputStream>(file);
        if (! stream->openedOk())
            return;

        juce::WavAudioFormat format;
        const auto options = juce::AudioFormatWriterOptions {}
                                 .withSampleRate(frame.source.sampleRate)
                                 .withNumChannels(frame.audio.getNumChannels())
                                 .withBitsPerSample(24);
        std::unique_ptr<juce::OutputStream> outputStream(stream.release());
        auto writer = format.createWriterFor(outputStream, options);
        if (writer == nullptr)
            return;

        WriterState state;
        state.file = file;
        state.writer = std::move(writer);
        writtenFiles_.push_back(file);
        writerIt = writersBySourceKey_.emplace(frame.source.sourceKey, std::move(state)).first;
    }

    if (writerIt->second.writer != nullptr)
        (void) writerIt->second.writer->writeFromAudioSampleBuffer(frame.audio, 0, frame.audio.getNumSamples());
}

std::string StemCaptureWriter::sanitizePathToken(std::string_view text)
{
    juce::String sanitized;
    const juce::String source(text.data());
    for (const auto character : source)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character))
            sanitized << character;
        else if (character == ' ' || character == '-' || character == '_')
            sanitized << character;
    }

    sanitized = sanitized.trim();
    while (sanitized.contains("  "))
        sanitized = sanitized.replace("  ", " ");

    if (sanitized.isEmpty())
        sanitized = "untitled";

    return sanitized.toStdString();
}

std::string StemCaptureWriter::makeSessionFolderName(const SessionConfig& config)
{
    const auto timestamp = config.startedAt.toString(true, true, false, true).replace(":", "-").replace(" ", "_");
    const auto milliseconds = juce::String(static_cast<int>(config.startedAt.toMilliseconds() % 1000)).paddedLeft('0', 3);
    return std::to_string(juce::jmax(0, config.beatsPerMinute))
        + "bpm-"
        + std::to_string(juce::jmax(0, config.beatsPerInterval))
        + "bpi-"
        + timestamp.toStdString()
        + "-"
        + milliseconds.toStdString();
}

std::string StemCaptureWriter::makeBaseFilename(const SourceConfig& source) const
{
    auto baseName = sanitizePathToken(source.userName) + " - " + sanitizePathToken(source.channelName);
    if (source.mode == Mode::Voice)
        baseName += " - voice";

    return baseName;
}

juce::File StemCaptureWriter::resolveFileForSource(const SourceConfig& source)
{
    auto baseName = makeBaseFilename(source);
    auto useCountIt = baseNameUseCounts_.find(baseName);
    if (useCountIt == baseNameUseCounts_.end())
    {
        baseNameUseCounts_[baseName] = 1;
        return currentSessionDirectory_.getChildFile(baseName + ".wav");
    }

    ++useCountIt->second;
    return currentSessionDirectory_.getChildFile(baseName + " (" + std::to_string(useCountIt->second) + ").wav");
}
} // namespace famalamajam::audio
