#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "audio/AudioBufferResampler.h"

namespace
{
juce::AudioBuffer<float> makeSineBuffer(double frequencyHz, double sampleRate, double durationSeconds)
{
    const auto samples = static_cast<int>(std::llround(sampleRate * durationSeconds));
    juce::AudioBuffer<float> buffer(1, samples);

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * frequencyHz * static_cast<double>(sample) / sampleRate;
        buffer.setSample(0, sample, static_cast<float>(std::sin(phase)));
    }

    return buffer;
}

double estimateFrequencyHz(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() < 3)
        return 0.0;

    const auto* samples = buffer.getReadPointer(0);
    int positiveGoingCrossings = 0;

    for (int sample = 1; sample < buffer.getNumSamples(); ++sample)
    {
        if (samples[sample - 1] <= 0.0f && samples[sample] > 0.0f)
            ++positiveGoingCrossings;
    }

    return static_cast<double>(positiveGoingCrossings) * sampleRate / static_cast<double>(buffer.getNumSamples());
}
} // namespace

TEST_CASE("audio buffer resampler preserves perceived pitch while matching target rate", "[audio_buffer_resampler]")
{
    constexpr double sourceSampleRate = 44100.0;
    constexpr double targetSampleRate = 48000.0;
    constexpr double toneFrequencyHz = 440.0;
    constexpr double durationSeconds = 0.25;

    const auto source = makeSineBuffer(toneFrequencyHz, sourceSampleRate, durationSeconds);
    const auto resampled = famalamajam::audio::resampleAudioBuffer(source, sourceSampleRate, targetSampleRate);

    REQUIRE(resampled.getNumChannels() == 1);
    CHECK(resampled.getNumSamples()
          == static_cast<int>(std::llround(static_cast<double>(source.getNumSamples()) * targetSampleRate / sourceSampleRate)));
    const auto estimatedFrequencyHz = estimateFrequencyHz(resampled, targetSampleRate);
    CHECK(std::abs(estimatedFrequencyHz - toneFrequencyHz) <= 6.0);
}
