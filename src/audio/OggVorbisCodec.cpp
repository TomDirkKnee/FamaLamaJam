#include "audio/OggVorbisCodec.h"

#include <limits>
#include <memory>

namespace famalamajam::audio
{
namespace
{
void setError(std::string* error, const std::string& message)
{
    if (error != nullptr)
        *error = message;
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

    juce::OggVorbisAudioFormat format;
    auto* inputStream = new juce::MemoryInputStream(encodedData, encodedSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(inputStream, true));

    if (reader == nullptr)
    {
        setError(error, "failed to create Ogg Vorbis reader");
        return false;
    }

    if (reader->lengthInSamples <= 0 || reader->numChannels <= 0)
    {
        setError(error, "decoded stream has no audio frames");
        return false;
    }

    if (reader->lengthInSamples > static_cast<juce::int64>(std::numeric_limits<int>::max()))
    {
        setError(error, "decoded stream too large");
        return false;
    }

    const auto decodedSamples = static_cast<int>(reader->lengthInSamples);
    const auto decodedChannels = static_cast<int>(reader->numChannels);

    decodedAudio.setSize(decodedChannels, decodedSamples, false, true, true);

    const auto readOk = reader->read(&decodedAudio,
                                     0,
                                     decodedSamples,
                                     0,
                                     true,
                                     decodedChannels > 1);

    if (! readOk)
    {
        setError(error, "failed to decode Ogg Vorbis payload");
        return false;
    }

    sampleRate = reader->sampleRate;
    return true;
}
} // namespace famalamajam::audio
