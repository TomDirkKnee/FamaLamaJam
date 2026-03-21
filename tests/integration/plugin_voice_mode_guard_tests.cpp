#include <functional>
#include <type_traits>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

template <typename T, typename = void>
constexpr bool hasChannelFlagsField = false;

template <typename T>
constexpr bool hasChannelFlagsField<T, std::void_t<decltype(std::declval<T&>().channelFlags)>> = true;

template <typename T, typename = void>
constexpr bool hasUnsupportedVoiceModeField = false;

template <typename T>
constexpr bool hasUnsupportedVoiceModeField<T,
                                            std::void_t<decltype(std::declval<T&>().unsupportedVoiceMode)>> = true;

bool waitUntil(FamaLamaJamAudioProcessor& processor,
               juce::AudioBuffer<float>& buffer,
               juce::MidiBuffer& midi,
               const std::function<bool()>& predicate,
               int attempts = 600)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (predicate())
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

float captureRemoteAudibleRms(FamaLamaJamAudioProcessor& processor,
                              juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midi,
                              int attempts = 120)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);

        const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto right = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        const auto rms = juce::jmax(left, right);
        if (rms > 1.0e-6f)
            return rms;

        juce::Thread::sleep(5);
    }

    return 0.0f;
}
} // namespace

TEST_CASE("plugin voice mode guard harness can advertise per-peer channel flags", "[plugin_voice_mode_guard]")
{
    CHECK(hasChannelFlagsField<MiniNinjamServer::RemotePeer>);

    MiniNinjamServer::RemotePeer peer;
    peer.channelFlags = 2;
    CHECK(peer.channelFlags == 2);
}

TEST_CASE("plugin voice mode guard does not mix remote voice-mode channels as interval audio",
          "[plugin_voice_mode_guard]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "voice-user", 0, 0, 2 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitUntil(processor, buffer, midi, [&]() {
        return processor.getTransportReceivedFramesForTesting() > 0
            && processor.getRemoteReceiveDiagnosticsText().find("mode=voice") != std::string::npos;
    }));

    CHECK(captureRemoteAudibleRms(processor, buffer, midi) == 0.0f);

    FamaLamaJamAudioProcessor::MixerStripSnapshot snapshot;
    REQUIRE(processor.getMixerStripSnapshot("voice-user#0", snapshot));
    CHECK(snapshot.unsupportedVoiceMode);
    CHECK(snapshot.statusText == "Voice chat mode is not supported yet.");
    CHECK_FALSE(processor.isRemoteSourceActiveForTesting("voice-user#0"));

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin voice mode guard reserves explicit unsupported state in normal mixer snapshots",
          "[plugin_voice_mode_guard]")
{
    CHECK(hasUnsupportedVoiceModeField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);

    FamaLamaJamAudioProcessor processor;
    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(hasUnsupportedVoiceModeField<decltype(localSnapshot)>);
}
