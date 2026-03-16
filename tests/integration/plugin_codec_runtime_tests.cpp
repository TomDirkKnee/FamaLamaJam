#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

namespace
{
void fillBuffer(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, static_cast<float>(sample) / static_cast<float>(buffer.getNumSamples()));
    }
}
} // namespace

TEST_CASE("plugin codec runtime bridge processes audio after active connection", "[plugin_codec_runtime]")
{
    famalamajam::plugin::FamaLamaJamAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(famalamajam::app::session::ConnectionEvent {
        .type = famalamajam::app::session::ConnectionEventType::Connected,
    });

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int attempt = 0; attempt < 80; ++attempt)
    {
        fillBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (processor.getLastCodecPayloadBytesForTesting() > 0)
            break;

        juce::Thread::sleep(5);
    }

    CHECK(processor.getLastCodecPayloadBytesForTesting() > 0);

    REQUIRE(processor.requestDisconnect());
    CHECK(processor.getLastCodecPayloadBytesForTesting() == 0);

    processor.releaseResources();
}

TEST_CASE("plugin codec runtime bridge resets stale timing and queued state on host reconfiguration",
          "[plugin_codec_runtime]")
{
    famalamajam::plugin::FamaLamaJamAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(famalamajam::app::session::ConnectionEvent {
        .type = famalamajam::app::session::ConnectionEventType::Connected,
    });

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        fillBuffer(buffer);
        processor.processBlock(buffer, midi);
    }

    processor.prepareToPlay(44100.0, 256);

    CHECK(processor.getTransportUiState().intervalIndex == 0);
    CHECK(processor.getTransportUiState().intervalProgress == Catch::Approx(0.0f));
    CHECK(processor.getQueuedRemoteSourceCountForTesting() == 0);
    CHECK(processor.getPendingRemoteSourceCountForTesting() == 0);

    for (int attempt = 0; attempt < 80; ++attempt)
    {
        fillBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (processor.getLastCodecPayloadBytesForTesting() > 0)
            break;

        juce::Thread::sleep(5);
    }

    CHECK(processor.getLastCodecPayloadBytesForTesting() > 0);
}
