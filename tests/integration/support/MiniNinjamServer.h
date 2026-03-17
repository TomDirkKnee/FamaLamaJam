#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>

namespace famalamajam::tests::integration
{
constexpr std::uint8_t kMessageServerAuthChallenge = 0x00;
constexpr std::uint8_t kMessageServerAuthReply = 0x01;
constexpr std::uint8_t kMessageServerConfigChange = 0x02;
constexpr std::uint8_t kMessageServerUserInfoChange = 0x03;
constexpr std::uint8_t kMessageServerDownloadIntervalBegin = 0x04;
constexpr std::uint8_t kMessageServerDownloadIntervalWrite = 0x05;

constexpr std::uint8_t kMessageClientAuthUser = 0x80;
constexpr std::uint8_t kMessageClientSetUserMask = 0x81;
constexpr std::uint8_t kMessageClientSetChannelInfo = 0x82;
constexpr std::uint8_t kMessageClientUploadIntervalBegin = 0x83;
constexpr std::uint8_t kMessageClientUploadIntervalWrite = 0x84;
constexpr std::uint8_t MESSAGE_CHAT_MESSAGE = 0xC0;

constexpr std::uint32_t kProtoVerCur = 0x00020000u;
constexpr std::uint32_t kUploadFourCcOggv = static_cast<std::uint32_t>('O')
                                          | (static_cast<std::uint32_t>('G') << 8)
                                          | (static_cast<std::uint32_t>('G') << 16)
                                          | (static_cast<std::uint32_t>('v') << 24);

inline std::uint32_t decodeLe32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8)
         | (static_cast<std::uint32_t>(bytes[2]) << 16)
         | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

constexpr std::uint32_t kNetMessageMaxSize = 16384;

inline void encodeLe32(std::uint32_t value, std::uint8_t* dest)
{
    dest[0] = static_cast<std::uint8_t>(value & 0xffu);
    dest[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    dest[2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    dest[3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

inline std::size_t boundedStrnlen(const char* text, std::size_t maxLen)
{
    std::size_t length = 0;
    while (length < maxLen && text[length] != '\0')
        ++length;

    return length;
}

struct NetMessage
{
    std::uint8_t type { 0 };
    juce::MemoryBlock payload;
};

class MiniNinjamServer final : private juce::Thread
{
public:
    struct RoomMessage
    {
        std::array<std::string, 5> fields;
    };

    struct RemotePeer
    {
        std::string username { "peer" };
        std::uint8_t channelIndex { 0 };
        int startAfterUploadBegins { 0 };
    };

    struct UserInfoChange
    {
        std::string username;
        std::string channelName;
        std::uint8_t channelIndex { 0 };
        bool active { false };
    };

    MiniNinjamServer()
        : juce::Thread("MiniNinjamServer")
    {
    }

    ~MiniNinjamServer() override
    {
        stopServer();
    }

    bool startServer()
    {
        if (! listener_.createListener(0, "127.0.0.1"))
            return false;

        port_ = listener_.getBoundPort();
        authEvent_.reset();
        startThread();
        return port_ > 0;
    }

    void stopServer()
    {
        signalThreadShouldExit();
        listener_.close();

        {
            const juce::ScopedLock lock(clientLock_);
            if (client_ != nullptr)
                client_->close();
        }

        wakeEvent_.signal();
        stopThread(2000);

        const juce::ScopedLock lock(clientLock_);
        client_.reset();
    }

    int port() const noexcept { return port_; }

    void setInitialTiming(std::uint16_t bpm, std::uint16_t bpi)
    {
        const juce::ScopedLock lock(configLock_);
        initialBpm_ = bpm;
        initialBpi_ = bpi;
    }

    void setAdvertisedKeepAliveSeconds(std::uint8_t seconds)
    {
        const juce::ScopedLock lock(configLock_);
        keepAliveSeconds_ = seconds == 0 ? 3 : seconds;
    }

    void enqueueConfigChange(std::uint16_t bpm, std::uint16_t bpi)
    {
        const juce::ScopedLock lock(configLock_);
        pendingConfigChanges_.emplace_back(bpm, bpi);
        wakeEvent_.signal();
    }

    void setRemotePeers(std::vector<RemotePeer> peers)
    {
        const juce::ScopedLock lock(configLock_);
        remotePeers_ = std::move(peers);
        if (remotePeers_.empty())
            remotePeers_.push_back(RemotePeer {});
    }

    void enqueueUserInfoChange(std::string username, std::string channelName, std::uint8_t channelIndex, bool active)
    {
        const juce::ScopedLock lock(configLock_);
        pendingUserInfoChanges_.push_back(UserInfoChange {
            .username = std::move(username),
            .channelName = std::move(channelName),
            .channelIndex = channelIndex,
            .active = active,
        });
        wakeEvent_.signal();
    }

    static juce::MemoryBlock buildRoomMessagePayload(const RoomMessage& message)
    {
        juce::MemoryBlock payload;

        for (const auto& field : message.fields)
        {
            payload.append(field.data(), field.size());
            const char terminator = '\0';
            payload.append(&terminator, 1);
        }

        return payload;
    }

    void enqueueRoomMessage(RoomMessage message)
    {
        const juce::ScopedLock lock(configLock_);
        pendingRoomMessages_.push_back(std::move(message));
        wakeEvent_.signal();
    }

    void enqueueRoomChatMessage(std::string author, std::string text)
    {
        enqueueRoomMessage(RoomMessage {
            .fields = { "MSG", std::move(author), std::move(text), {}, {} },
        });
    }

    void enqueueRoomSystemMessage(std::string text)
    {
        enqueueRoomChatMessage({}, std::move(text));
    }

    void enqueueRoomTopic(std::string author, std::string topic)
    {
        enqueueRoomMessage(RoomMessage {
            .fields = { "TOPIC", std::move(author), std::move(topic), {}, {} },
        });
    }

    void enqueueRoomJoin(std::string username)
    {
        enqueueRoomMessage(RoomMessage {
            .fields = { "JOIN", std::move(username), {}, {}, {} },
        });
    }

    void enqueueRoomPart(std::string username)
    {
        enqueueRoomMessage(RoomMessage {
            .fields = { "PART", std::move(username), {}, {}, {} },
        });
    }

    bool waitForAuthentication(int timeoutMs) const
    {
        return authEvent_.wait(timeoutMs);
    }

    bool waitForCapturedRoomMessage(int timeoutMs, RoomMessage& out)
    {
        const auto boundedTimeout = juce::jmax(1, timeoutMs);

        if (! roomMessageEvent_.wait(boundedTimeout))
            return false;

        const juce::ScopedLock lock(clientLock_);
        if (capturedRoomMessages_.empty())
            return false;

        out = std::move(capturedRoomMessages_.front());
        capturedRoomMessages_.erase(capturedRoomMessages_.begin());

        if (! capturedRoomMessages_.empty())
            roomMessageEvent_.signal();

        return true;
    }

private:
    bool readExact(juce::StreamingSocket& socket, void* dest, int bytes)
    {
        int offset = 0;
        auto* data = static_cast<char*>(dest);

        while (offset < bytes && ! threadShouldExit())
        {
            if (socket.waitUntilReady(true, 50) <= 0)
                continue;

            const auto got = socket.read(data + offset, bytes - offset, false);
            if (got <= 0)
                return false;

            offset += got;
        }

        return offset == bytes;
    }

    bool writeExact(juce::StreamingSocket& socket, const void* src, int bytes)
    {
        int offset = 0;
        const auto* data = static_cast<const char*>(src);

        while (offset < bytes && ! threadShouldExit())
        {
            if (socket.waitUntilReady(false, 50) <= 0)
                continue;

            const auto wrote = socket.write(data + offset, bytes - offset);
            if (wrote <= 0)
                return false;

            offset += wrote;
        }

        return offset == bytes;
    }

    bool readMessage(juce::StreamingSocket& socket, NetMessage& out)
    {
        std::array<std::uint8_t, 5> header {};
        if (! readExact(socket, header.data(), static_cast<int>(header.size())))
            return false;

        out.type = header[0];
        const auto payloadBytes = decodeLe32(header.data() + 1);

        if (payloadBytes > kNetMessageMaxSize)
            return false;

        out.payload.setSize(payloadBytes);
        if (payloadBytes > 0)
            return readExact(socket, out.payload.getData(), static_cast<int>(payloadBytes));

        return true;
    }

    bool writeMessage(juce::StreamingSocket& socket, std::uint8_t type, const void* payload, std::size_t payloadBytes)
    {
        juce::MemoryBlock frame;
        frame.setSize(5 + payloadBytes);

        auto* bytes = static_cast<std::uint8_t*>(frame.getData());
        bytes[0] = type;
        encodeLe32(static_cast<std::uint32_t>(payloadBytes), bytes + 1);

        if (payloadBytes > 0 && payload != nullptr)
            std::memcpy(bytes + 5, payload, payloadBytes);

        return writeExact(socket, frame.getData(), static_cast<int>(frame.getSize()));
    }

    static RoomMessage parseRoomMessage(const juce::MemoryBlock& payload)
    {
        RoomMessage message;
        const auto* cursor = static_cast<const char*>(payload.getData());
        const auto* end = cursor + payload.getSize();

        for (auto& field : message.fields)
        {
            if (cursor >= end)
                break;

            const auto remaining = static_cast<std::size_t>(end - cursor);
            const auto fieldLength = boundedStrnlen(cursor, remaining);
            field.assign(cursor, fieldLength);
            cursor += fieldLength + 1;
        }

        return message;
    }

    bool sendChallenge(juce::StreamingSocket& socket)
    {
        std::array<std::uint8_t, 16> payload {};
        for (int i = 0; i < 8; ++i)
            payload[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i + 1);

        std::uint8_t keepAliveSeconds = 3;
        {
            const juce::ScopedLock lock(configLock_);
            keepAliveSeconds = keepAliveSeconds_;
        }

        const std::uint32_t serverCaps = static_cast<std::uint32_t>(keepAliveSeconds) << 8;
        encodeLe32(serverCaps, payload.data() + 8);
        encodeLe32(kProtoVerCur, payload.data() + 12);

        return writeMessage(socket, kMessageServerAuthChallenge, payload.data(), payload.size());
    }

    bool sendAuthReply(juce::StreamingSocket& socket, const std::string& effectiveUsername)
    {
        std::vector<std::uint8_t> payload(1 + effectiveUsername.size() + 1 + 1);
        payload[0] = 1;
        std::memcpy(payload.data() + 1, effectiveUsername.data(), effectiveUsername.size());
        payload[1 + effectiveUsername.size()] = 0;
        payload[1 + effectiveUsername.size() + 1] = 32;
        return writeMessage(socket, kMessageServerAuthReply, payload.data(), payload.size());
    }

    bool sendConfigChange(juce::StreamingSocket& socket, std::uint16_t bpm, std::uint16_t bpi)
    {
        std::array<std::uint8_t, 4> payload {};
        encodeLe32(static_cast<std::uint32_t>(bpm) | (static_cast<std::uint32_t>(bpi) << 16), payload.data());
        return writeMessage(socket, kMessageServerConfigChange, payload.data(), payload.size());
    }

    bool sendUserInfo(juce::StreamingSocket& socket,
                      const std::string& username,
                      const std::string& channelName,
                      std::uint8_t channelIndex,
                      bool active)
    {
        std::vector<std::uint8_t> payload;
        payload.resize(1 + 1 + 2 + 1 + 1 + username.size() + 1 + channelName.size() + 1);

        std::size_t offset = 0;
        payload[offset++] = active ? 1 : 0;
        payload[offset++] = channelIndex;
        payload[offset++] = 0;
        payload[offset++] = 0;
        payload[offset++] = 0;
        payload[offset++] = 0;

        std::memcpy(payload.data() + offset, username.data(), username.size());
        offset += username.size();
        payload[offset++] = 0;

        std::memcpy(payload.data() + offset, channelName.data(), channelName.size());
        offset += channelName.size();
        payload[offset++] = 0;

        return writeMessage(socket, kMessageServerUserInfoChange, payload.data(), offset);
    }

    bool sendDownloadBegin(juce::StreamingSocket& socket,
                           const std::array<std::uint8_t, 16>& guid,
                           std::uint32_t estSize,
                           const std::string& username,
                           std::uint8_t channelIndex)
    {
        std::vector<std::uint8_t> payload(25 + username.size() + 1);
        std::memcpy(payload.data(), guid.data(), guid.size());
        encodeLe32(estSize, payload.data() + 16);
        encodeLe32(kUploadFourCcOggv, payload.data() + 20);
        payload[24] = channelIndex;
        std::memcpy(payload.data() + 25, username.data(), username.size());
        payload[25 + username.size()] = 0;

        return writeMessage(socket, kMessageServerDownloadIntervalBegin, payload.data(), payload.size());
    }

    bool sendDownloadWrite(juce::StreamingSocket& socket,
                           const std::array<std::uint8_t, 16>& guid,
                           const std::uint8_t* audioData,
                           std::size_t audioBytes)
    {
        juce::MemoryBlock payload;
        payload.setSize(17 + audioBytes);

        auto* bytes = static_cast<std::uint8_t*>(payload.getData());
        std::memcpy(bytes, guid.data(), guid.size());
        bytes[16] = 1;

        if (audioBytes > 0)
            std::memcpy(bytes + 17, audioData, audioBytes);

        return writeMessage(socket, kMessageServerDownloadIntervalWrite, payload.getData(), payload.getSize());
    }

    bool flushPendingConfigChanges(juce::StreamingSocket& socket)
    {
        std::vector<std::pair<std::uint16_t, std::uint16_t>> updates;

        {
            const juce::ScopedLock lock(configLock_);
            updates.swap(pendingConfigChanges_);
        }

        for (const auto& [bpm, bpi] : updates)
        {
            if (! sendConfigChange(socket, bpm, bpi))
                return false;
        }

        return true;
    }

    bool flushPendingUserInfoChanges(juce::StreamingSocket& socket)
    {
        std::vector<UserInfoChange> updates;

        {
            const juce::ScopedLock lock(configLock_);
            updates.swap(pendingUserInfoChanges_);
        }

        for (const auto& update : updates)
        {
            if (! sendUserInfo(socket, update.username, update.channelName, update.channelIndex, update.active))
                return false;
        }

        return true;
    }

    bool flushPendingRoomMessages(juce::StreamingSocket& socket)
    {
        std::vector<RoomMessage> messages;

        {
            const juce::ScopedLock lock(configLock_);
            messages.swap(pendingRoomMessages_);
        }

        for (const auto& message : messages)
        {
            const auto payload = buildRoomMessagePayload(message);
            if (! writeMessage(socket, MESSAGE_CHAT_MESSAGE, payload.getData(), payload.getSize()))
                return false;
        }

        return true;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            authEvent_.reset();

            std::unique_ptr<juce::StreamingSocket> accepted(listener_.waitForNextConnection());
            if (accepted == nullptr)
                return;

            {
                const juce::ScopedLock lock(clientLock_);
                client_ = std::move(accepted);
            }

            juce::StreamingSocket* socket = nullptr;
            {
                const juce::ScopedLock lock(clientLock_);
                socket = client_.get();
            }

            if (socket == nullptr)
                return;

            if (! sendChallenge(*socket))
                return;

            std::array<std::uint8_t, 16> lastUploadGuid {};
            bool hasUploadGuid = false;
            bool authed = false;
            std::string effectiveUsername = "guest";
            int uploadBeginCount = 0;

            struct ActiveRemoteTransfer
            {
                std::array<std::uint8_t, 16> guid {};
                RemotePeer peer;
            };
            std::vector<ActiveRemoteTransfer> activeTransfers;

            while (! threadShouldExit())
            {
                if (authed && ! flushPendingConfigChanges(*socket))
                    return;
                if (authed && ! flushPendingUserInfoChanges(*socket))
                    return;
                if (authed && ! flushPendingRoomMessages(*socket))
                    return;

                if (socket->waitUntilReady(true, 20) <= 0)
                {
                    wakeEvent_.wait(1);
                    continue;
                }

                NetMessage message;
                if (! readMessage(*socket, message))
                    break;

                if (message.type == kMessageClientAuthUser)
                {
                    if (message.payload.getSize() < 29)
                        return;

                    const auto* payload = static_cast<const std::uint8_t*>(message.payload.getData());
                    const char* username = reinterpret_cast<const char*>(payload + 20);
                    const auto usernameMax = message.payload.getSize() - 20;
                    const auto usernameLen = boundedStrnlen(username, usernameMax);

                    if (usernameLen > 0 && usernameLen < usernameMax)
                        effectiveUsername.assign(username, usernameLen);

                    if (! sendAuthReply(*socket, effectiveUsername))
                        return;

                    std::uint16_t initialBpm = 120;
                    std::uint16_t initialBpi = 16;
                    std::vector<RemotePeer> peers;
                    {
                        const juce::ScopedLock lock(configLock_);
                        initialBpm = initialBpm_;
                        initialBpi = initialBpi_;
                        peers = remotePeers_;
                    }

                    if (! sendConfigChange(*socket, initialBpm, initialBpi))
                        return;

                    for (const auto& peer : peers)
                    {
                        if (! sendUserInfo(*socket, peer.username, "room", peer.channelIndex, true))
                            return;
                    }

                    authed = true;
                    authEvent_.signal();
                    continue;
                }

                if (! authed)
                    continue;

                if (message.type == kMessageClientUploadIntervalBegin)
                {
                    if (message.payload.getSize() < 25)
                        return;

                    const auto* payload = static_cast<const std::uint8_t*>(message.payload.getData());
                    std::memcpy(lastUploadGuid.data(), payload, lastUploadGuid.size());
                    const auto estSize = decodeLe32(payload + 16);
                    hasUploadGuid = true;
                    ++uploadBeginCount;
                    activeTransfers.clear();

                    std::vector<RemotePeer> peers;
                    {
                        const juce::ScopedLock lock(configLock_);
                        peers = remotePeers_;
                    }

                    if (peers.empty())
                        peers.push_back(RemotePeer {});

                    for (std::size_t peerIndex = 0; peerIndex < peers.size(); ++peerIndex)
                    {
                        const auto& peer = peers[peerIndex];
                        if (uploadBeginCount <= peer.startAfterUploadBegins)
                            continue;

                        ActiveRemoteTransfer transfer;
                        transfer.peer = peer;
                        encodeLe32(static_cast<std::uint32_t>(uploadBeginCount), transfer.guid.data());
                        encodeLe32(static_cast<std::uint32_t>(peerIndex + 1), transfer.guid.data() + 4);
                        encodeLe32(static_cast<std::uint32_t>(peer.channelIndex), transfer.guid.data() + 8);
                        encodeLe32(static_cast<std::uint32_t>(peer.username.size()), transfer.guid.data() + 12);

                        if (! sendDownloadBegin(*socket,
                                                transfer.guid,
                                                estSize,
                                                transfer.peer.username,
                                                transfer.peer.channelIndex))
                        {
                            return;
                        }

                        activeTransfers.push_back(transfer);
                    }

                    continue;
                }

                if (message.type == kMessageClientUploadIntervalWrite)
                {
                    if (message.payload.getSize() < 17 || ! hasUploadGuid)
                        continue;

                    const auto* payload = static_cast<const std::uint8_t*>(message.payload.getData());
                    std::array<std::uint8_t, 16> incomingGuid {};
                    std::memcpy(incomingGuid.data(), payload, incomingGuid.size());

                    if (incomingGuid != lastUploadGuid)
                        continue;

                    const auto audioData = payload + 17;
                    const auto audioBytes = message.payload.getSize() - 17;

                    for (const auto& transfer : activeTransfers)
                    {
                        if (! sendDownloadWrite(*socket, transfer.guid, audioData, audioBytes))
                            return;
                    }

                    continue;
                }

                if (message.type == MESSAGE_CHAT_MESSAGE)
                {
                    const auto roomMessage = parseRoomMessage(message.payload);

                    {
                        const juce::ScopedLock lock(clientLock_);
                        capturedRoomMessages_.push_back(roomMessage);
                    }

                    roomMessageEvent_.signal();
                    continue;
                }

                if (message.type == kMessageClientSetChannelInfo || message.type == kMessageClientSetUserMask)
                    continue;
            }

            {
                const juce::ScopedLock lock(clientLock_);
                client_.reset();
            }
        }
    }

    juce::StreamingSocket listener_;
    mutable juce::CriticalSection clientLock_;
    std::unique_ptr<juce::StreamingSocket> client_;
    mutable juce::WaitableEvent authEvent_;
    mutable juce::WaitableEvent roomMessageEvent_;
    juce::WaitableEvent wakeEvent_;
    mutable juce::CriticalSection configLock_;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> pendingConfigChanges_;
    std::vector<UserInfoChange> pendingUserInfoChanges_;
    std::vector<RoomMessage> pendingRoomMessages_;
    std::vector<RoomMessage> capturedRoomMessages_;
    std::vector<RemotePeer> remotePeers_ { RemotePeer {} };
    std::uint16_t initialBpm_ { 120 };
    std::uint16_t initialBpi_ { 16 };
    std::uint8_t keepAliveSeconds_ { 3 };
    int port_ { -1 };
};

inline void fillRampBuffer(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, static_cast<float>(sample) / static_cast<float>(buffer.getNumSamples()));
    }
}
} // namespace famalamajam::tests::integration
