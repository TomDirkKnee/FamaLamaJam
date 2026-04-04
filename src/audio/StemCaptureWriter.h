#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace famalamajam::audio
{
class StemCaptureWriter final : private juce::Thread
{
public:
    enum class Mode
    {
        Interval,
        Voice,
    };

    struct SessionConfig
    {
        juce::File baseDirectory;
        int beatsPerMinute { 0 };
        int beatsPerInterval { 0 };
        juce::Time startedAt;
    };

    struct SourceConfig
    {
        std::string sourceKey;
        std::string userName;
        std::string channelName;
        Mode mode { Mode::Interval };
        double sampleRate { 0.0 };
    };

    struct Frame
    {
        SourceConfig source;
        juce::AudioBuffer<float> audio;
    };

    StemCaptureWriter();
    ~StemCaptureWriter() override;

    void start();
    void stop();

    void beginSession(const SessionConfig& config);
    void pauseSession();
    void endSession();
    void retireSource(const std::string& sourceKey);
    void submitFrame(const Frame& frame);

    [[nodiscard]] bool flushForTesting(int timeoutMs);
    [[nodiscard]] juce::File getCurrentSessionDirectoryForTesting() const;
    [[nodiscard]] std::vector<juce::File> getWrittenFilesForTesting() const;

private:
    struct Command;
    struct WriterState
    {
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter> writer;
    };

    void run() override;
    void enqueue(Command command);
    void processCommand(Command& command);
    void closeAllWriters();
    void handleRetireSource(const std::string& sourceKey);
    void handleBeginSession(const SessionConfig& config);
    void handleSubmitFrame(const Frame& frame);
    [[nodiscard]] static std::string sanitizePathToken(std::string_view text);
    [[nodiscard]] static std::string makeSessionFolderName(const SessionConfig& config);
    [[nodiscard]] std::string makeBaseFilename(const SourceConfig& source) const;
    [[nodiscard]] juce::File resolveFileForSource(const SourceConfig& source);

    struct Command
    {
        enum class Kind
        {
            BeginSession,
            EndSession,
            RetireSource,
            SubmitFrame,
            Flush,
        };

        Kind kind { Kind::EndSession };
        SessionConfig sessionConfig;
        std::string sourceKey;
        Frame frame;
        std::shared_ptr<juce::WaitableEvent> flushEvent;
    };

    mutable std::mutex stateMutex_;
    std::deque<Command> commandQueue_;
    juce::WaitableEvent workEvent_;
    juce::File currentSessionDirectory_;
    std::unordered_map<std::string, WriterState> writersBySourceKey_;
    std::vector<juce::File> writtenFiles_;
    std::unordered_map<std::string, int> baseNameUseCounts_;
};
} // namespace famalamajam::audio
