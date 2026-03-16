#include "audio/AudioBufferResampler.h"

#include <cmath>

namespace famalamajam::audio
{
juce::AudioBuffer<float> resampleAudioBuffer(const juce::AudioBuffer<float>& input,
                                             double sourceSampleRate,
                                             double targetSampleRate)
{
    if (input.getNumChannels() <= 0 || input.getNumSamples() <= 0)
        return {};

    if (sourceSampleRate <= 0.0 || targetSampleRate <= 0.0
        || std::abs(sourceSampleRate - targetSampleRate) < 0.5)
    {
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf(input, true);
        return copy;
    }

    const auto outputSamples = juce::jmax(
        1,
        static_cast<int>(std::llround(static_cast<double>(input.getNumSamples()) * targetSampleRate / sourceSampleRate)));

    juce::AudioBuffer<float> output(input.getNumChannels(), outputSamples);

    const auto inputSamples = input.getNumSamples();
    const auto sourceAdvance = sourceSampleRate / targetSampleRate;

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        const auto* inputData = input.getReadPointer(channel);
        auto* outputData = output.getWritePointer(channel);

        for (int sample = 0; sample < outputSamples; ++sample)
        {
            const auto sourcePosition = static_cast<double>(sample) * sourceAdvance;
            const auto lowerIndex = juce::jlimit(0, inputSamples - 1, static_cast<int>(std::floor(sourcePosition)));
            const auto upperIndex = juce::jmin(lowerIndex + 1, inputSamples - 1);
            const auto fraction = static_cast<float>(sourcePosition - static_cast<double>(lowerIndex));
            const auto lowerSample = inputData[lowerIndex];
            const auto upperSample = inputData[upperIndex];
            outputData[sample] = lowerSample + ((upperSample - lowerSample) * fraction);
        }
    }

    return output;
}
} // namespace famalamajam::audio
