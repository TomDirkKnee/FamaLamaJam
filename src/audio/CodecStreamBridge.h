#pragma once

#include <cstddef>
#include <deque>
#include <string>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace famalamajam::audio
{
class CodecStreamBridge final : private juce::Thread
{
public:
    struct PendingInputFrame
    {
        juce::AudioBuffer<float> audio;
        double sampleRate { 0.0 };
    };

    struct InboundEncodedFrame
    {
        juce::MemoryBlock payload;
        std::string sourceId;
    };

    struct DecodedFrame
    {
        juce::AudioBuffer<float> audio;
        std::string sourceId;
        double sampleRate { 0.0 };
    };

    CodecStreamBridge();
    ~CodecStreamBridge() override;

    void start();
    void stop();

    void submitInput(const juce::AudioBuffer<float>& input, double sampleRate);
    void submitInboundEncoded(const void* encodedData, std::size_t encodedSize);
    void submitInboundEncoded(const std::string& sourceId, const void* encodedData, std::size_t encodedSize);

    bool popEncoded(juce::MemoryBlock& payload);
    bool popDecoded(juce::AudioBuffer<float>& output);
    bool popDecodedFrame(DecodedFrame& output);

    [[nodiscard]] std::size_t getLastEncodedPayloadBytes() const;

private:
    void run() override;

    mutable juce::CriticalSection stateLock_;
    std::deque<PendingInputFrame> pendingInputQueue_;

    std::deque<InboundEncodedFrame> pendingInboundEncodedQueue_;

    std::deque<juce::MemoryBlock> encodedOutputQueue_;

    std::deque<DecodedFrame> decodedOutputQueue_;

    std::size_t lastEncodedPayloadBytes_ { 0 };
    juce::WaitableEvent workEvent_;
};
} // namespace famalamajam::audio
