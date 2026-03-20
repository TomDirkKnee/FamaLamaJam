#include <array>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "audio/OggVorbisCodec.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kChannels = 2;
constexpr int kSamples = 4800;

juce::AudioBuffer<float> makeTestTone()
{
    juce::AudioBuffer<float> buffer(kChannels, kSamples);

    for (int sample = 0; sample < kSamples; ++sample)
    {
        const auto t = static_cast<float>(sample) / static_cast<float>(kSamples);
        const auto value = std::sin(2.0f * juce::MathConstants<float>::pi * 7.0f * t);

        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value * 0.7f);
    }

    return buffer;
}
} // namespace

TEST_CASE("ogg vorbis codec encodes and decodes audio", "[ogg_vorbis_codec]")
{
    auto input = makeTestTone();
    juce::MemoryBlock encoded;

    REQUIRE(famalamajam::audio::OggVorbisCodec::encode(input, kSampleRate, encoded));
    CHECK(encoded.getSize() > 0);

    juce::AudioBuffer<float> decoded;
    double decodedSampleRate = 0.0;
    std::string decodeError;

    REQUIRE(famalamajam::audio::OggVorbisCodec::decode(encoded.getData(),
                                                       encoded.getSize(),
                                                       decoded,
                                                       decodedSampleRate,
                                                       &decodeError));

    CHECK(decodeError.empty());
    CHECK(decodedSampleRate == Catch::Approx(kSampleRate).epsilon(0.001));
    CHECK(decoded.getNumChannels() == kChannels);
    CHECK(decoded.getNumSamples() == kSamples);

    const auto window = juce::jmin(decoded.getNumSamples(), input.getNumSamples());
    CHECK(decoded.getRMSLevel(0, 0, window) > 0.01f);
    CHECK(decoded.getRMSLevel(1, 0, window) > 0.01f);
}

TEST_CASE("ogg vorbis codec rejects invalid payload", "[ogg_vorbis_codec]")
{
    constexpr std::array<unsigned char, 6> invalidPayload { 'n', 'o', 't', 'o', 'g', 'g' };

    juce::AudioBuffer<float> decoded;
    double sampleRate = 0.0;
    std::string error;

    CHECK_FALSE(famalamajam::audio::OggVorbisCodec::decode(invalidPayload.data(),
                                                            invalidPayload.size(),
                                                            decoded,
                                                            sampleRate,
                                                            &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("ogg vorbis codec decodes concatenated payloads as full audio", "[ogg_vorbis_codec]")
{
    auto first = makeTestTone();
    auto second = makeTestTone();
    second.applyGain(0.35f);

    juce::MemoryBlock firstEncoded;
    juce::MemoryBlock secondEncoded;
    REQUIRE(famalamajam::audio::OggVorbisCodec::encode(first, kSampleRate, firstEncoded));
    REQUIRE(famalamajam::audio::OggVorbisCodec::encode(second, kSampleRate, secondEncoded));

    juce::MemoryBlock concatenated;
    concatenated.append(firstEncoded.getData(), firstEncoded.getSize());
    concatenated.append(secondEncoded.getData(), secondEncoded.getSize());

    juce::AudioBuffer<float> decoded;
    double decodedSampleRate = 0.0;
    std::string decodeError;

    REQUIRE(famalamajam::audio::OggVorbisCodec::decode(concatenated.getData(),
                                                       concatenated.getSize(),
                                                       decoded,
                                                       decodedSampleRate,
                                                       &decodeError));

    CHECK(decodeError.empty());
    CHECK(decodedSampleRate == Catch::Approx(kSampleRate).epsilon(0.001));
    CHECK(decoded.getNumChannels() == kChannels);
    CHECK(decoded.getNumSamples() == (kSamples * 2));
}
