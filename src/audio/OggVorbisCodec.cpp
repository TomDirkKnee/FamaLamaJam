#include "audio/OggVorbisCodec.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <juce_audio_formats/codecs/oggvorbis/vorbisfile.h>

namespace famalamajam::audio
{
namespace
{
void setError(std::string* error, const std::string& message)
{
    if (error != nullptr)
        *error = message;
}

struct MemoryVorbisSource
{
    const std::uint8_t* data { nullptr };
    std::size_t size { 0 };
    std::size_t position { 0 };
};

size_t readFromMemory(void* dest, size_t size, size_t nmemb, void* datasource)
{
    if (dest == nullptr || datasource == nullptr || size == 0 || nmemb == 0)
        return 0;

    auto& source = *static_cast<MemoryVorbisSource*>(datasource);
    const auto requestedBytes = size * nmemb;
    const auto availableBytes = source.size - juce::jmin(source.position, source.size);
    const auto bytesToCopy = juce::jmin(requestedBytes, availableBytes);

    if (bytesToCopy == 0)
        return 0;

    std::memcpy(dest, source.data + source.position, bytesToCopy);
    source.position += bytesToCopy;
    return bytesToCopy / size;
}

int seekInMemory(void* datasource, ogg_int64_t offset, int whence)
{
    if (datasource == nullptr)
        return -1;

    auto& source = *static_cast<MemoryVorbisSource*>(datasource);

    std::int64_t base = 0;
    switch (whence)
    {
        case SEEK_SET: base = 0; break;
        case SEEK_CUR: base = static_cast<std::int64_t>(source.position); break;
        case SEEK_END: base = static_cast<std::int64_t>(source.size); break;
        default: return -1;
    }

    const auto nextPosition = base + static_cast<std::int64_t>(offset);
    if (nextPosition < 0 || nextPosition > static_cast<std::int64_t>(source.size))
        return -1;

    source.position = static_cast<std::size_t>(nextPosition);
    return 0;
}

long tellInMemory(void* datasource)
{
    if (datasource == nullptr)
        return -1;

    const auto& source = *static_cast<MemoryVorbisSource*>(datasource);
    if (source.position > static_cast<std::size_t>(std::numeric_limits<long>::max()))
        return -1;

    return static_cast<long>(source.position);
}
} // namespace

bool OggVorbisCodec::encode(const juce::AudioBuffer<float>& input,
                            double sampleRate,
                            juce::MemoryBlock& encodedPayload,
                            std::string* error)
{
    if (sampleRate <= 0.0)
    {
        setError(error, "sample rate must be positive");
        return false;
    }

    const auto numChannels = input.getNumChannels();
    const auto numSamples = input.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
    {
        setError(error, "input audio buffer is empty");
        return false;
    }

    encodedPayload.reset();

    juce::OggVorbisAudioFormat format;
    std::unique_ptr<juce::OutputStream> outputStream = std::make_unique<juce::MemoryOutputStream>(encodedPayload, false);

    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate(sampleRate)
                             .withNumChannels(numChannels)
                             .withBitsPerSample(16)
                             .withQualityOptionIndex(0);

    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(outputStream, options));

    if (writer == nullptr)
    {
        setError(error, "failed to create Ogg Vorbis writer");
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer(input, 0, numSamples))
    {
        setError(error, "failed to encode audio buffer");
        return false;
    }

    writer.reset();

    if (encodedPayload.getSize() == 0)
    {
        setError(error, "encoded payload is empty");
        return false;
    }

    return true;
}

bool OggVorbisCodec::decode(const void* encodedData,
                            std::size_t encodedSize,
                            juce::AudioBuffer<float>& decodedAudio,
                            double& sampleRate,
                            std::string* error)
{
    if (encodedData == nullptr || encodedSize == 0)
    {
        setError(error, "encoded payload is empty");
        return false;
    }

    MemoryVorbisSource source {
        .data = static_cast<const std::uint8_t*>(encodedData),
        .size = encodedSize,
        .position = 0,
    };

    ov_callbacks callbacks {};
    callbacks.read_func = &readFromMemory;
    callbacks.seek_func = &seekInMemory;
    callbacks.close_func = nullptr;
    callbacks.tell_func = &tellInMemory;

    OggVorbis_File vorbisFile {};
    if (ov_open_callbacks(&source, &vorbisFile, nullptr, 0, callbacks) != 0)
    {
        setError(error, "failed to open Ogg Vorbis payload");
        return false;
    }

    const auto closeVorbis = juce::ScopeGuard([&vorbisFile]() { ov_clear(&vorbisFile); });
    const auto* initialInfo = ov_info(&vorbisFile, -1);
    if (initialInfo == nullptr || initialInfo->channels <= 0 || initialInfo->rate <= 0)
    {
        setError(error, "decoded stream has invalid Vorbis metadata");
        return false;
    }

    const auto decodedChannels = static_cast<int>(initialInfo->channels);
    sampleRate = static_cast<double>(initialInfo->rate);

    std::vector<std::vector<float>> channelData(static_cast<std::size_t>(decodedChannels));
    int currentSection = 0;

    for (;;)
    {
        float** outputChannels = nullptr;
        const auto samplesDecoded = ov_read_float(&vorbisFile, &outputChannels, 4096, &currentSection);

        if (samplesDecoded == 0)
            break;

        if (samplesDecoded < 0)
        {
            setError(error, "failed to decode Ogg Vorbis payload");
            return false;
        }

        const auto* sectionInfo = ov_info(&vorbisFile, currentSection);
        if (sectionInfo == nullptr || sectionInfo->channels <= 0 || sectionInfo->rate <= 0)
        {
            setError(error, "decoded stream has invalid Vorbis section metadata");
            return false;
        }

        if (sectionInfo->channels != decodedChannels)
        {
            setError(error, "decoded payload changed channel count mid-stream");
            return false;
        }

        if (! juce::approximatelyEqual(static_cast<double>(sectionInfo->rate), sampleRate))
        {
            setError(error, "decoded payload changed sample rate mid-stream");
            return false;
        }

        for (int channel = 0; channel < decodedChannels; ++channel)
        {
            auto& destination = channelData[static_cast<std::size_t>(channel)];
            destination.insert(destination.end(),
                               outputChannels[channel],
                               outputChannels[channel] + samplesDecoded);
        }
    }

    const auto decodedSamples = static_cast<int>(channelData.front().size());
    if (decodedSamples <= 0)
    {
        setError(error, "decoded stream has no audio frames");
        return false;
    }

    decodedAudio.setSize(decodedChannels, decodedSamples, false, true, true);
    for (int channel = 0; channel < decodedChannels; ++channel)
    {
        auto* dest = decodedAudio.getWritePointer(channel);
        const auto& sourceChannel = channelData[static_cast<std::size_t>(channel)];
        std::copy(sourceChannel.begin(), sourceChannel.end(), dest);
    }

    return true;
}
} // namespace famalamajam::audio
