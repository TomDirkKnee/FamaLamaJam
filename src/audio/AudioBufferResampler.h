#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace famalamajam::audio
{
juce::AudioBuffer<float> resampleAudioBuffer(const juce::AudioBuffer<float>& input,
                                             double sourceSampleRate,
                                             double targetSampleRate);
} // namespace famalamajam::audio
