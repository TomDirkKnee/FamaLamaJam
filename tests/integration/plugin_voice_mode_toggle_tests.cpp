#include <functional>
#include <memory>

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

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

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
} // namespace

TEST_CASE("plugin voice mode toggle exposes a dedicated local-strip voice control beside transmit",
          "[plugin_voice_mode_toggle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor(true, true);
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));

    REQUIRE(editor != nullptr);
    CHECK(editor->getMixerStripTransmitButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId) == "TX");
    CHECK(editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId) == "INT");

    bool voiceEnabled = true;
    REQUIRE(editor->getMixerStripVoiceToggleStateForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId,
                                                            voiceEnabled));
    CHECK_FALSE(voiceEnabled);

    REQUIRE(editor->clickMixerStripVoiceToggleForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId));
    CHECK(processor.isLocalVoiceModeEnabled());
    CHECK(editor->getMixerStripStatusTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId)
          == "Voice mode ready");
    CHECK(editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId) == "VOX");
    REQUIRE(editor->getMixerStripVoiceToggleStateForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId,
                                                            voiceEnabled));
    CHECK(voiceEnabled);
}

TEST_CASE("plugin voice mode toggle keeps the local voice button compact beside the integrated strip spine",
          "[plugin_voice_mode_toggle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor(true, true);
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));

    REQUIRE(editor != nullptr);

    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting layout;
    REQUIRE(editor->getMixerStripLayoutSnapshotForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, layout));

    CHECK(layout.gainBounds.contains(layout.meterBounds.getCentre()));
    CHECK(layout.voiceBounds.getWidth() <= 28);
    CHECK(layout.voiceBounds.getX() - layout.gainBounds.getRight() <= 14);
}

TEST_CASE("plugin voice mode toggle switches live into voice mode and returns through interval warmup",
          "[plugin_voice_mode_toggle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransmitState() == FamaLamaJamAudioProcessor::TransmitState::Active;
    }));

    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);

    REQUIRE(editor->clickMixerStripVoiceToggleForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId));
    CHECK(processor.isLocalVoiceModeEnabled());
    CHECK(editor->getMixerStripStatusTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId)
          == "Switching to voice mode...");
    CHECK(processor.getTransmitState() == FamaLamaJamAudioProcessor::TransmitState::Active);

    fillRampBuffer(buffer);
    processor.processBlock(buffer, midi);
    editor->refreshForTesting();
    CHECK(editor->getMixerStripStatusTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId)
          == "Voice chat: low quality, near realtime");

    REQUIRE(editor->clickMixerStripVoiceToggleForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId));
    CHECK_FALSE(processor.isLocalVoiceModeEnabled());
    CHECK(editor->getMixerStripStatusTextForTesting(FamaLamaJamAudioProcessor::kLocalMonitorSourceId)
          == "Getting ready to transmit");
    CHECK(processor.getTransmitState() == FamaLamaJamAudioProcessor::TransmitState::WarmingUp);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}
