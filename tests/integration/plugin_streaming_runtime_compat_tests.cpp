#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server, double sampleRate, int blockSize)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(sampleRate, blockSize);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool()>& predicate,
                  int attempts = 6000)
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

TEST_CASE("plugin streaming runtime compatibility recovers send and receive after supported sample-rate changes",
          "[plugin_streaming_runtime_compat]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, 48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportSentFramesForTesting() > 0
            && processor.getTransportReceivedFramesForTesting() > 0
            && processor.isRemoteSourceActiveForTesting("alice#0");
    }));

    processor.prepareToPlay(44100.0, 256);
    CHECK_FALSE(processor.getTransportUiState().hasServerTiming);

    juce::AudioBuffer<float> reconfiguredBuffer(2, 256);
    REQUIRE(processUntil(processor, reconfiguredBuffer, midi, [&]() {
        const auto state = processor.getTransportUiState();
        return state.hasServerTiming
            && processor.getLastCodecPayloadBytesForTesting() > 0
            && processor.getLastDecodedSamplesForTesting() > 0
            && processor.isRemoteSourceActiveForTesting("alice#0");
    }));

    processor.prepareToPlay(48000.0, 1024);
    juce::AudioBuffer<float> secondBuffer(2, 1024);
    REQUIRE(processUntil(processor, secondBuffer, midi, [&]() {
        const auto state = processor.getTransportUiState();
        return state.hasServerTiming
            && processor.getTransportSentFramesForTesting() > 1
            && processor.getTransportReceivedFramesForTesting() > 1
            && processor.isRemoteSourceActiveForTesting("alice#0");
    }));
}
