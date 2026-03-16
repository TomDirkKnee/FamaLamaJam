#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

#include <juce_core/juce_core.h>

namespace famalamajam::net
{
class FramedSocketTransport final : private juce::Thread
{
public:
    struct ServerTimingConfig
    {
        int beatsPerMinute { 0 };
        int beatsPerInterval { 0 };
    };

    struct RemoteSourceInfo
    {
        std::string sourceId;
        std::string groupId;
        std::string userName;
        std::string channelName;
        std::string displayName;
        std::uint8_t channelIndex { 0 };
    };

    struct InboundFrame
    {
        juce::MemoryBlock payload;
        std::string sourceId;
        RemoteSourceInfo sourceInfo;
    };

    struct RemoteSourceActivityUpdate
    {
        RemoteSourceInfo sourceInfo;
        bool active { false };
    };

    explicit FramedSocketTransport(std::size_t maxFrameBytes = 1u << 20);
    ~FramedSocketTransport() override;

    bool start(std::unique_ptr<juce::StreamingSocket> socket, std::string username, std::string password);
    bool waitForAuthentication(int timeoutMs,
                               std::string* failureReason = nullptr,
                               std::string* effectiveUsername = nullptr);
    void stop();
    [[nodiscard]] bool isRunning() const;

    void enqueueOutbound(const juce::MemoryBlock& payload);
    bool popInbound(juce::MemoryBlock& payload);
    bool popInboundFrame(InboundFrame& frame);
    bool popRemoteSourceActivityUpdate(RemoteSourceActivityUpdate& update);
    bool getServerTimingConfig(ServerTimingConfig& config) const;

    [[nodiscard]] std::size_t getSentFrameCount() const;
    [[nodiscard]] std::size_t getReceivedFrameCount() const;
    [[nodiscard]] int getKeepAliveSecondsForTesting() const;

private:
    void run() override;

    bool writeMessage(std::uint8_t type, const void* payload, std::size_t payloadBytes);
    bool readOneMessage(std::uint8_t& type, juce::MemoryBlock& payload);
    bool writeUploadInterval(const juce::MemoryBlock& payload);
    void processInboundMessage(std::uint8_t type, const juce::MemoryBlock& payload);
    void handleAuthChallenge(const juce::MemoryBlock& payload);
    void handleAuthReply(const juce::MemoryBlock& payload);
    void handleConfigChange(const juce::MemoryBlock& payload);
    void handleUserInfo(const juce::MemoryBlock& payload);
    void handleDownloadBegin(const juce::MemoryBlock& payload);
    void handleDownloadWrite(const juce::MemoryBlock& payload);
    void sendUserMask(const std::string& username, std::uint32_t channelMask);
    void sendDefaultChannelInfo();
    void failAuthentication(const std::string& reason);

    [[nodiscard]] std::array<std::uint8_t, 20> buildAuthPassHash(const std::array<std::uint8_t, 8>& challenge) const;
    [[nodiscard]] std::string makeGuidKey(const std::uint8_t* guidBytes) const;
    [[nodiscard]] std::string makeDownloadSourceId(const juce::MemoryBlock& payload) const;
    [[nodiscard]] RemoteSourceInfo makeRemoteSourceInfo(std::string username,
                                                        std::uint8_t channelIndex,
                                                        std::string channelName) const;
    [[nodiscard]] RemoteSourceInfo makeDownloadSourceInfo(const juce::MemoryBlock& payload) const;

    struct InboundTransferState
    {
        juce::MemoryBlock payload;
        std::string sourceId;
        RemoteSourceInfo sourceInfo;
    };

    std::unique_ptr<juce::StreamingSocket> socket_;
    mutable juce::CriticalSection stateLock_;
    std::deque<juce::MemoryBlock> outboundQueue_;
    std::deque<InboundFrame> inboundQueue_;
    std::deque<RemoteSourceActivityUpdate> remoteSourceActivityQueue_;

    std::array<char, 5> inboundHeader_ { 0, 0, 0, 0, 0 };
    int inboundHeaderBytes_ { 0 };
    std::uint8_t inboundType_ { 0 };
    juce::MemoryBlock inboundPayload_;
    int inboundPayloadBytes_ { 0 };
    std::uint32_t inboundExpectedBytes_ { 0 };
    bool socketFailed_ { false };

    std::string username_;
    std::string password_;
    std::string effectiveUsername_;
    std::string authFailureReason_;
    ServerTimingConfig serverTimingConfig_;
    bool hasServerTimingConfig_ { false };
    std::unordered_map<std::string, RemoteSourceInfo> knownRemoteSourcesById_;

    int keepAliveSeconds_ { 3 };
    juce::int64 lastSendMs_ { 0 };
    juce::int64 lastReceiveMs_ { 0 };

    bool awaitingAuthChallenge_ { true };
    bool awaitingAuthReply_ { false };
    bool authenticated_ { false };

    std::unordered_map<std::string, std::uint32_t> subscribedUserMasks_;
    std::unordered_map<std::string, InboundTransferState> inboundTransfers_;

    std::size_t sentFrameCount_ { 0 };
    std::size_t receivedFrameCount_ { 0 };
    const std::size_t maxFrameBytes_;
    juce::WaitableEvent wakeEvent_;
    juce::WaitableEvent authEvent_;
};
} // namespace famalamajam::net
