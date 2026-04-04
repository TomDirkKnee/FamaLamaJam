#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::plugin::FamaLamaJamAudioProcessorEditor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

class ScopedTempStemDirectory
{
public:
    ScopedTempStemDirectory()
        : directory_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getNonexistentChildFile("famalamajam-plugin-stems", {}, false))
    {
        REQUIRE(directory_.createDirectory());
    }

    ~ScopedTempStemDirectory()
    {
        directory_.deleteRecursively();
    }

    [[nodiscard]] const juce::File& directory() const noexcept { return directory_; }

private:
    juce::File directory_;
};

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server, std::string username = "guest")
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = std::move(username);

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool()>& predicate,
                  int attempts = 4000)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (predicate())
            return true;

        juce::Thread::sleep(1);
    }

    return false;
}

juce::AudioBuffer<float> makePulseBuffer(int channels, int samples, float amplitude)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel)
    {
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, (sample < (samples / 2)) ? amplitude : -amplitude);
    }

    return buffer;
}

std::unique_ptr<juce::AudioFormatReader> createReader(const juce::File& file)
{
    juce::WavAudioFormat format;
    auto input = std::make_unique<juce::FileInputStream>(file);
    REQUIRE(input->openedOk());
    auto* rawReader = format.createReaderFor(input.release(), true);
    REQUIRE(rawReader != nullptr);
    return std::unique_ptr<juce::AudioFormatReader>(rawReader);
}
} // namespace

TEST_CASE("plugin stem capture excludes local voice mode and resumes interval export with a fresh file",
          "[plugin_stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, "jim");

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), true));

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return ! processor.getWrittenStemFilesForTesting().empty();
    }));

    const auto initialFiles = processor.getWrittenStemFilesForTesting();
    REQUIRE(initialFiles.size() == 1);
    CHECK_FALSE(initialFiles.front().getFileName().contains("voice"));

    if (! processor.isLocalVoiceModeEnabled())
        (void) processor.toggleLocalVoiceMode();
    CHECK(processor.isLocalVoiceModeEnabled());
    const auto filesBeforeVoice = processor.getWrittenStemFilesForTesting().size();
    const auto voiceOnlyStable = processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex >= 4;
    });
    REQUIRE(voiceOnlyStable);
    CHECK(processor.getWrittenStemFilesForTesting().size() == filesBeforeVoice);

    (void) processor.toggleLocalVoiceMode();
    CHECK_FALSE(processor.isLocalVoiceModeEnabled());
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getWrittenStemFilesForTesting().size() > filesBeforeVoice;
    }));

    const auto stopInterval = processor.getTransportUiState().intervalIndex;
    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), false));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex > stopInterval;
    }));
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    const auto files = processor.getWrittenStemFilesForTesting();
    REQUIRE(files.size() == 2);
    CHECK_FALSE(files[0].getFileName().contains("voice"));
    CHECK_FALSE(files[1].getFileName().contains("voice"));
    for (const auto& file : files)
    {
        auto reader = createReader(file);
        CHECK(reader->numChannels == 2);
        CHECK(reader->lengthInSamples > 0);
    }

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin stem capture ui requires a folder before arming and persists the chosen path",
          "[plugin_stem_capture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTempStemDirectory tempDirectory;

    FamaLamaJamAudioProcessor processor(true, true);
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);

    CHECK_FALSE(editor->isStemCaptureEnabledForTesting());

    editor->clickStemCaptureToggleForTesting();
    CHECK_FALSE(editor->isStemCaptureEnabledForTesting());
    CHECK(editor->getStemCaptureStatusTextForTesting().contains("Choose a stem folder"));

    editor->setStemCaptureDirectoryForTesting(tempDirectory.directory().getFullPathName());
    editor->clickStemCaptureToggleForTesting();

    CHECK(editor->isStemCaptureEnabledForTesting());
    CHECK(editor->getStemCaptureStatusTextForTesting().isEmpty());

    const auto settings = processor.getStemCaptureSettings();
    CHECK(settings.enabled);
    CHECK(settings.outputDirectory == tempDirectory.directory().getFullPathName().toStdString());
}

TEST_CASE("plugin stem capture writes late-joining remote interval stems without silence backfill", "[plugin_stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, "dirk");

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), true));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex >= 2;
    }));

    constexpr int kIntervalSamples = 7200;
    processor.registerRemoteSourceForTesting("late-user#0", "late-user", "Keys", 0u);
    processor.injectDecodedRemoteIntervalForTesting("late-user#0", makePulseBuffer(2, kIntervalSamples, 0.25f), 48000.0);
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    const auto stopInterval = processor.getTransportUiState().intervalIndex;
    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), false));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex > stopInterval;
    }));
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    const auto sessionDirectory = processor.getStemCaptureSessionDirectoryForTesting();
    REQUIRE(sessionDirectory.isDirectory());
    CHECK(sessionDirectory.getFileName().contains("400bpm-1bpi-"));

    const auto files = processor.getWrittenStemFilesForTesting();
    REQUIRE(files.size() >= 1);
    std::string fileNames;
    for (const auto& file : files)
    {
        if (! fileNames.empty())
            fileNames += ", ";
        fileNames += file.getFileName().toStdString();
    }
    INFO(fileNames);

    const auto remoteIt = std::find_if(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName().contains("late-user");
    });
    REQUIRE(remoteIt != files.end());
    CHECK_FALSE(remoteIt->getFileName().contains("voice"));

    auto reader = createReader(*remoteIt);
    CHECK(reader->numChannels == 2);
    CHECK(reader->lengthInSamples == kIntervalSamples);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin stem capture excludes remote voice chunks and resumes interval export into a fresh file",
          "[plugin_stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, "dirk");

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), true));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex >= 2;
    }));

    constexpr int kIntervalSamples = 7200;
    processor.registerRemoteSourceForTesting("remote-peer#0", "remote-peer", "Keys", 0u);
    processor.injectDecodedRemoteIntervalForTesting("remote-peer#0", makePulseBuffer(2, kIntervalSamples, 0.25f), 48000.0);
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    auto files = processor.getWrittenStemFilesForTesting();
    REQUIRE(files.size() == 1);
    CHECK_FALSE(files.front().getFileName().contains("voice"));

    processor.injectDecodedRemoteVoiceForTesting("remote-peer#0", makePulseBuffer(1, 1024, 0.35f), 48000.0);
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));
    CHECK(processor.getWrittenStemFilesForTesting().size() == 1);

    processor.injectDecodedRemoteIntervalForTesting("remote-peer#0", makePulseBuffer(2, kIntervalSamples, 0.15f), 48000.0);
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    files = processor.getWrittenStemFilesForTesting();
    REQUIRE(files.size() == 2);
    CHECK(std::any_of(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName().contains("(2)");
    }));
    CHECK_FALSE(files[1].getFileName().contains("voice"));

    auto resumedReader = createReader(files[1]);
    CHECK(resumedReader->numChannels == 2);
    CHECK(resumedReader->lengthInSamples == kIntervalSamples);

    const auto stopInterval = processor.getTransportUiState().intervalIndex;
    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), false));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex > stopInterval;
    }));
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin stem capture starts a fresh session folder on explicit new run without reconnect",
          "[plugin_stem_capture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTempStemDirectory tempDirectory;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, "dirk");
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), true));
    editor->refreshForTesting();
    CHECK(editor->canClickNewStemRunForTesting());

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return ! processor.getWrittenStemFilesForTesting().empty();
    }));

    const auto firstSessionDirectory = processor.getStemCaptureSessionDirectoryForTesting();
    REQUIRE(firstSessionDirectory.isDirectory());
    const auto firstSessionFiles = processor.getWrittenStemFilesForTesting();
    REQUIRE_FALSE(firstSessionFiles.empty());

    editor->clickNewStemRunForTesting();
    CHECK(editor->getStemCaptureStatusTextForTesting().contains("next interval"));

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getStemCaptureSessionDirectoryForTesting() != firstSessionDirectory;
    }));

    const auto secondSessionDirectory = processor.getStemCaptureSessionDirectoryForTesting();
    REQUIRE(secondSessionDirectory.isDirectory());
    CHECK(secondSessionDirectory != firstSessionDirectory);
    CHECK(firstSessionDirectory.findChildFiles(juce::File::findFiles, false, "*.wav").size()
          >= static_cast<int>(firstSessionFiles.size()));

    const auto stopInterval = processor.getTransportUiState().intervalIndex;
    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), false));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex > stopInterval;
    }));
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin stem capture keeps one session folder across disconnect and reconnect", "[plugin_stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, "dirk");

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), true));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return ! processor.getWrittenStemFilesForTesting().empty();
    }));

    const auto sessionDirectory = processor.getStemCaptureSessionDirectoryForTesting();
    REQUIRE(sessionDirectory.isDirectory());
    const auto initialFiles = processor.getWrittenStemFilesForTesting();
    REQUIRE_FALSE(initialFiles.empty());

    REQUIRE(processor.requestDisconnect());
    juce::Thread::sleep(50);

    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getWrittenStemFilesForTesting().size() > initialFiles.size();
    }));

    const auto postReconnectFiles = processor.getWrittenStemFilesForTesting();
    CHECK(processor.getStemCaptureSessionDirectoryForTesting() == sessionDirectory);
    CHECK(postReconnectFiles.size() > initialFiles.size());

    const auto stopInterval = processor.getTransportUiState().intervalIndex;
    REQUIRE(processor.setStemCaptureDirectoryForTesting(tempDirectory.directory(), false));
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().intervalIndex > stopInterval;
    }));
    REQUIRE(processor.waitForStemCaptureFlushForTesting(2000));

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}
