#include <functional>

#include <catch2/catch_test_macros.hpp>

#include "audio/OggVorbisCodec.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::audio::OggVorbisCodec;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;

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

juce::AudioBuffer<float> makePulseBuffer(int channels, int samples, float amplitude)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    buffer.clear();

    for (int channel = 0; channel < channels; ++channel)
    {
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto value = sample < (samples / 2) ? amplitude : -amplitude;
            buffer.setSample(channel, sample, value);
        }
    }

    return buffer;
}

juce::MemoryBlock encodeAudio(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    juce::MemoryBlock encoded;
    std::string codecError;
    REQUIRE(OggVorbisCodec::encode(audio, sampleRate, encoded, &codecError));
    REQUIRE(encoded.getSize() > 0);
    return encoded;
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool(const juce::AudioBuffer<float>&)>& predicate,
                  int attempts = 400,
                  int sleepMs = 2)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);

        if (predicate(buffer))
            return true;

        juce::Thread::sleep(sleepMs);
    }

    return false;
}

float maxAudibleRms(const juce::AudioBuffer<float>& buffer)
{
    const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    const auto right = buffer.getNumChannels() > 1 ? buffer.getRMSLevel(1, 0, buffer.getNumSamples()) : left;
    return juce::jmax(left, right);
}
} // namespace

TEST_CASE("plugin voice mode transport plays remote voice chunks before the next interval boundary in mixed rooms",
          "[plugin_voice_mode_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(120, 8);
    server.setRemotePeers({ { "interval-user", 0, 0, 0 }, { "voice-user", 1, 0, 2 } });

    const auto encodedVoice = encodeAudio(makePulseBuffer(2, 2048, 0.18f), 48000.0);
    const auto encodedInterval = encodeAudio(makePulseBuffer(2, 2048, 0.07f), 48000.0);
    server.enqueueRemoteTransfer({ "voice-user", 1, 0, 2 }, encodedVoice, 192);
    server.enqueueRemoteTransfer({ "interval-user", 0, 0, 0 }, encodedInterval, 192);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const auto heardVoiceBeforeBoundary = processUntil(processor, buffer, midi, [&](const juce::AudioBuffer<float>& output) {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming
            && transport.intervalIndex == 0
            && processor.getTransportReceivedFramesForTesting() >= 2
            && maxAudibleRms(output) > 1.0e-4f;
    });

    INFO(processor.getRemoteReceiveDiagnosticsText());
    REQUIRE(heardVoiceBeforeBoundary);
    CHECK_FALSE(processor.isRemoteSourceActiveForTesting("interval-user#0"));

    FamaLamaJamAudioProcessor::MixerStripSnapshot voiceSnapshot;
    REQUIRE(processor.getMixerStripSnapshot("voice-user#1", voiceSnapshot));
    CHECK_FALSE(voiceSnapshot.unsupportedVoiceMode);
    CHECK(voiceSnapshot.descriptor.visible);
    CHECK(voiceSnapshot.statusText != "Voice chat mode is not supported yet.");

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin voice mode transport keeps interval peers boundary-quantized while mixed-room voice is already audible",
          "[plugin_voice_mode_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "interval-user", 0, 0, 0 }, { "voice-user", 1, 0, 2 } });

    const auto encodedVoice = encodeAudio(makePulseBuffer(2, 2048, 0.21f), 48000.0);
    const auto encodedInterval = encodeAudio(makePulseBuffer(2, 2048, 0.09f), 48000.0);
    server.enqueueRemoteTransfer({ "voice-user", 1, 0, 2 }, encodedVoice, 128);
    server.enqueueRemoteTransfer({ "interval-user", 0, 0, 0 }, encodedInterval, 128);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const auto heardVoiceFirst = processUntil(processor, buffer, midi, [&](const juce::AudioBuffer<float>& output) {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming
            && transport.intervalIndex == 0
            && processor.getTransportReceivedFramesForTesting() >= 2
            && maxAudibleRms(output) > 1.0e-4f;
    });

    INFO(processor.getRemoteReceiveDiagnosticsText());
    REQUIRE(heardVoiceFirst);
    CHECK_FALSE(processor.isRemoteSourceActiveForTesting("interval-user#0"));

    const auto intervalPeerActivated = processUntil(processor, buffer, midi, [&](const juce::AudioBuffer<float>&) {
        const auto transport = processor.getTransportUiState();
        return transport.intervalIndex >= 1 && processor.isRemoteSourceActiveForTesting("interval-user#0");
    });

    INFO(processor.getRemoteReceiveDiagnosticsText());
    REQUIRE(intervalPeerActivated);
    CHECK(processor.getRemoteReceiveDiagnosticsText().find("voice-user#1 mode=voice") != std::string::npos);
    CHECK(processor.getRemoteReceiveDiagnosticsText().find("interval-user#0 mode=interval") != std::string::npos);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}
