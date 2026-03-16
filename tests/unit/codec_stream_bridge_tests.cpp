#include <set>

#include <catch2/catch_test_macros.hpp>

#include "audio/CodecStreamBridge.h"

namespace
{
juce::AudioBuffer<float> makeInputBuffer()
{
    constexpr int channels = 2;
    constexpr int samples = 512;

    juce::AudioBuffer<float> buffer(channels, samples);

    for (int channel = 0; channel < channels; ++channel)
    {
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, static_cast<float>(sample) / static_cast<float>(samples));
    }

    return buffer;
}
} // namespace

TEST_CASE("codec stream bridge emits encoded payload and decodes inbound payload", "[codec_stream_bridge]")
{
    famalamajam::audio::CodecStreamBridge bridge;
    bridge.start();

    const auto input = makeInputBuffer();
    bridge.submitInput(input, 48000.0);

    juce::MemoryBlock encoded;
    bool hasEncoded = false;

    for (int attempt = 0; attempt < 80; ++attempt)
    {
        if (bridge.popEncoded(encoded))
        {
            hasEncoded = true;
            break;
        }

        juce::Thread::sleep(10);
    }

    REQUIRE(hasEncoded);
    CHECK(encoded.getSize() > 0);

    bridge.submitInboundEncoded(encoded.getData(), encoded.getSize());

    juce::AudioBuffer<float> decoded;
    bool hasDecoded = false;

    for (int attempt = 0; attempt < 80; ++attempt)
    {
        if (bridge.popDecoded(decoded))
        {
            hasDecoded = true;
            break;
        }

        juce::Thread::sleep(10);
    }

    REQUIRE(hasDecoded);
    CHECK(decoded.getNumChannels() == input.getNumChannels());
    CHECK(decoded.getNumSamples() > 0);

    bridge.stop();
}

TEST_CASE("codec stream bridge preserves source identity for queued inbound payloads", "[codec_stream_bridge]")
{
    famalamajam::audio::CodecStreamBridge bridge;
    bridge.start();

    const auto input = makeInputBuffer();
    bridge.submitInput(input, 48000.0);

    juce::MemoryBlock encoded;
    bool hasEncoded = false;

    for (int attempt = 0; attempt < 80; ++attempt)
    {
        if (bridge.popEncoded(encoded))
        {
            hasEncoded = true;
            break;
        }

        juce::Thread::sleep(10);
    }

    REQUIRE(hasEncoded);
    REQUIRE(encoded.getSize() > 0);

    bridge.submitInboundEncoded("peer-a#0", encoded.getData(), encoded.getSize());
    bridge.submitInboundEncoded("peer-b#1", encoded.getData(), encoded.getSize());

    std::set<std::string> decodedSources;
    int decodedFrames = 0;

    for (int attempt = 0; attempt < 120 && decodedFrames < 2; ++attempt)
    {
        famalamajam::audio::CodecStreamBridge::DecodedFrame frame;

        if (bridge.popDecodedFrame(frame))
        {
            ++decodedFrames;
            decodedSources.insert(frame.sourceId);
            CHECK(std::abs(frame.sampleRate - 48000.0) <= 48.0);
        }
        else
        {
            juce::Thread::sleep(10);
        }
    }

    CHECK(decodedFrames == 2);
    CHECK(decodedSources.count("peer-a#0") == 1);
    CHECK(decodedSources.count("peer-b#1") == 1);

    bridge.stop();
}
