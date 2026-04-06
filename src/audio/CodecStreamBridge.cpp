#include "audio/CodecStreamBridge.h"

#include "audio/OggVorbisCodec.h"

namespace famalamajam::audio
{
namespace
{
constexpr std::size_t kMaxPendingInputQueueDepth = 128;
constexpr std::size_t kMaxInboundQueueDepth = 64;
constexpr std::size_t kMaxEncodedQueueDepth = 128;
constexpr std::size_t kMaxDecodedQueueDepth = 64;
constexpr std::size_t kMaxInboundDecodePerCycle = 8;
} // namespace

CodecStreamBridge::CodecStreamBridge()
    : juce::Thread("CodecStreamBridge")
{
}

CodecStreamBridge::~CodecStreamBridge()
{
    stop();
}

void CodecStreamBridge::start()
{
    if (isThreadRunning())
        return;

    startThread();
}

void CodecStreamBridge::stop()
{
    signalThreadShouldExit();
    workEvent_.signal();
    stopThread(2000);

    const juce::ScopedLock lock(stateLock_);
    pendingInputQueue_.clear();

    pendingInboundEncodedQueue_.clear();

    encodedOutputQueue_.clear();

    decodedOutputQueue_.clear();
    lastEncodedPayloadBytes_ = 0;
}

void CodecStreamBridge::submitInput(const juce::AudioBuffer<float>& input, double sampleRate)
{
    submitInput(input, sampleRate, {});
}

void CodecStreamBridge::submitInput(const juce::AudioBuffer<float>& input,
                                    double sampleRate,
                                    LocalChannelMetadata metadata)
{
    if (sampleRate <= 0.0 || input.getNumChannels() <= 0 || input.getNumSamples() <= 0)
        return;

    {
        const juce::ScopedLock lock(stateLock_);

        PendingInputFrame frame;
        frame.audio.makeCopyOf(input, true);
        frame.sampleRate = sampleRate;
        frame.metadata = std::move(metadata);
        pendingInputQueue_.push_back(std::move(frame));

        while (pendingInputQueue_.size() > kMaxPendingInputQueueDepth)
            pendingInputQueue_.pop_front();
    }

    workEvent_.signal();
}

void CodecStreamBridge::submitInboundEncoded(const void* encodedData, std::size_t encodedSize)
{
    submitInboundEncoded(std::string(), 0, encodedData, encodedSize);
}

void CodecStreamBridge::submitInboundEncoded(const std::string& sourceId,
                                             const void* encodedData,
                                             std::size_t encodedSize)
{
    submitInboundEncoded(sourceId, 0, encodedData, encodedSize);
}

void CodecStreamBridge::submitInboundEncoded(const std::string& sourceId,
                                             std::uint64_t boundaryGeneration,
                                             const void* encodedData,
                                             std::size_t encodedSize)
{
    if (encodedData == nullptr || encodedSize == 0)
        return;

    InboundEncodedFrame frame;
    frame.payload.replaceAll(encodedData, encodedSize);
    frame.sourceId = sourceId;
    frame.boundaryGeneration = boundaryGeneration;

    {
        const juce::ScopedLock lock(stateLock_);
        pendingInboundEncodedQueue_.push_back(std::move(frame));

        while (pendingInboundEncodedQueue_.size() > kMaxInboundQueueDepth)
            pendingInboundEncodedQueue_.pop_front();
    }

    workEvent_.signal();
}

bool CodecStreamBridge::popEncoded(juce::MemoryBlock& payload)
{
    EncodedLocalFrame frame;
    if (! popEncoded(frame))
        return false;

    payload = std::move(frame.payload);
    return true;
}

bool CodecStreamBridge::popEncoded(EncodedLocalFrame& frame)
{
    const juce::ScopedLock lock(stateLock_);

    if (encodedOutputQueue_.empty())
        return false;

    frame = std::move(encodedOutputQueue_.front());
    encodedOutputQueue_.pop_front();
    return true;
}

bool CodecStreamBridge::popDecoded(juce::AudioBuffer<float>& output)
{
    DecodedFrame frame;
    if (! popDecodedFrame(frame))
        return false;

    output.makeCopyOf(frame.audio, true);
    return true;
}

bool CodecStreamBridge::popDecodedFrame(DecodedFrame& output)
{
    const juce::ScopedLock lock(stateLock_);

    if (decodedOutputQueue_.empty())
        return false;

    output.audio.makeCopyOf(decodedOutputQueue_.front().audio, true);
    output.sourceId = decodedOutputQueue_.front().sourceId;
    output.sampleRate = decodedOutputQueue_.front().sampleRate;
    output.boundaryGeneration = decodedOutputQueue_.front().boundaryGeneration;
    decodedOutputQueue_.pop_front();
    return true;
}

std::size_t CodecStreamBridge::getLastEncodedPayloadBytes() const
{
    const juce::ScopedLock lock(stateLock_);
    return lastEncodedPayloadBytes_;
}

void CodecStreamBridge::run()
{
    while (! threadShouldExit())
    {
        workEvent_.wait(25);

        while (! threadShouldExit())
        {
            PendingInputFrame inputSnapshot;
            std::deque<InboundEncodedFrame> inboundSnapshots;

            {
                const juce::ScopedLock lock(stateLock_);

                if (! pendingInputQueue_.empty())
                {
                    inputSnapshot.audio.makeCopyOf(pendingInputQueue_.front().audio, true);
                    inputSnapshot.sampleRate = pendingInputQueue_.front().sampleRate;
                    inputSnapshot.metadata = pendingInputQueue_.front().metadata;
                    pendingInputQueue_.pop_front();
                }

                while (! pendingInboundEncodedQueue_.empty() && inboundSnapshots.size() < kMaxInboundDecodePerCycle)
                {
                    inboundSnapshots.push_back(std::move(pendingInboundEncodedQueue_.front()));
                    pendingInboundEncodedQueue_.pop_front();
                }
            }

            if (inputSnapshot.audio.getNumChannels() <= 0
                && (inboundSnapshots.empty() || inboundSnapshots.front().payload.getSize() == 0))
            {
                break;
            }

            if (inputSnapshot.audio.getNumChannels() > 0
                && inputSnapshot.audio.getNumSamples() > 0
                && inputSnapshot.sampleRate > 0.0)
            {
                juce::MemoryBlock encoded;
                std::string codecError;

                if (OggVorbisCodec::encode(inputSnapshot.audio, inputSnapshot.sampleRate, encoded, &codecError))
                {
                    const juce::ScopedLock lock(stateLock_);
                    encodedOutputQueue_.push_back(EncodedLocalFrame {
                        .payload = std::move(encoded),
                        .metadata = inputSnapshot.metadata,
                    });

                    while (encodedOutputQueue_.size() > kMaxEncodedQueueDepth)
                        encodedOutputQueue_.pop_front();

                    lastEncodedPayloadBytes_ = encodedOutputQueue_.back().payload.getSize();
                }
            }

            for (auto& inboundSnapshot : inboundSnapshots)
            {
                if (inboundSnapshot.payload.getSize() == 0)
                    continue;

                juce::AudioBuffer<float> decoded;
                double decodedSampleRate = 0.0;
                std::string codecError;

                if (OggVorbisCodec::decode(inboundSnapshot.payload.getData(),
                                           inboundSnapshot.payload.getSize(),
                                           decoded,
                                           decodedSampleRate,
                                           &codecError))
                {
                    DecodedFrame frame;
                    frame.audio.makeCopyOf(decoded, true);
                    frame.sourceId = inboundSnapshot.sourceId;
                    frame.sampleRate = decodedSampleRate;
                    frame.boundaryGeneration = inboundSnapshot.boundaryGeneration;

                    const juce::ScopedLock lock(stateLock_);
                    decodedOutputQueue_.push_back(std::move(frame));

                    while (decodedOutputQueue_.size() > kMaxDecodedQueueDepth)
                        decodedOutputQueue_.pop_front();
                }
            }
        }
    }
}
} // namespace famalamajam::audio
