#pragma once

#include <cstddef>
#include <string>

#include <juce_audio_formats/juce_audio_formats.h>

namespace famalamajam::audio
{
class OggVorbisCodec final
{
public:
    static bool encode(const juce::AudioBuffer<float>& input,
                       double sampleRate,
                       juce::MemoryBlock& encodedPayload,
                       std::string* error = nullptr);

    static bool decode(const void* encodedData,
                       std::size_t encodedSize,
                       juce::AudioBuffer<float>& decodedAudio,
                       double& sampleRate,
                       std::string* error = nullptr);
};
} // namespace famalamajam::audio
