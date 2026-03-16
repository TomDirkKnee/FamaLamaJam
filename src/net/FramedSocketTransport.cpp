#include "net/FramedSocketTransport.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

namespace famalamajam::net
{
namespace
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

constexpr std::uint8_t kMessageKeepAlive = 0xfd;

constexpr std::uint32_t kProtoVerMin = 0x00020000u;
constexpr std::uint32_t kProtoVerMax = 0x0002ffffu;
constexpr std::uint32_t kProtoVerCur = 0x00020000u;

constexpr std::uint32_t kClientCapsLicenseAccepted = 1u << 0;
constexpr std::uint32_t kClientCapsHasVersion = 1u << 1;

constexpr std::size_t kMaxQueuedFrames = 32;
constexpr std::size_t kNetMessageMaxSize = 16384;
constexpr std::size_t kMaxUploadChunkBytes = 8192;
constexpr std::uint32_t kUploadFourCcOggv = static_cast<std::uint32_t>('O')
                                          | (static_cast<std::uint32_t>('G') << 8)
                                          | (static_cast<std::uint32_t>('G') << 16)
                                          | (static_cast<std::uint32_t>('v') << 24);

std::uint32_t decodeLe32(const void* data)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8)
         | (static_cast<std::uint32_t>(bytes[2]) << 16)
         | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void encodeLe32(std::uint32_t value, std::uint8_t* dest)
{
    dest[0] = static_cast<std::uint8_t>(value & 0xffu);
    dest[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    dest[2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    dest[3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

std::size_t boundedStrnlen(const char* text, std::size_t maxLen)
{
    std::size_t length = 0;
    while (length < maxLen && text[length] != '\0')
        ++length;

    return length;
}

std::string normalizeUserIdentity(std::string username)
{
    constexpr auto anonymousPrefix = "anonymous:";

    std::transform(username.begin(),
                   username.end(),
                   username.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    if (username.rfind(anonymousPrefix, 0) == 0)
        username.erase(0, std::char_traits<char>::length(anonymousPrefix));

    return username;
}

std::string makeRemoteChannelLabel(const std::string& username, const std::string& channelName, std::uint8_t channelIndex)
{
    if (! channelName.empty())
        return username + " - " + channelName;

    return username + " - Channel " + std::to_string(static_cast<unsigned>(channelIndex) + 1u);
}

std::uint32_t rotateLeft(std::uint32_t value, std::uint32_t bits)
{
    return (value << bits) | (value >> (32u - bits));
}

std::array<std::uint8_t, 20> sha1Digest(const void* data, std::size_t size)
{
    const auto* inputBytes = static_cast<const std::uint8_t*>(data);

    std::vector<std::uint8_t> buffer;
    buffer.reserve(size + 72);
    buffer.insert(buffer.end(), inputBytes, inputBytes + size);
    buffer.push_back(0x80u);

    while ((buffer.size() % 64u) != 56u)
        buffer.push_back(0u);

    const auto bitLength = static_cast<std::uint64_t>(size) * 8u;
    for (int shift = 56; shift >= 0; shift -= 8)
        buffer.push_back(static_cast<std::uint8_t>((bitLength >> static_cast<unsigned>(shift)) & 0xffu));

    std::uint32_t h0 = 0x67452301u;
    std::uint32_t h1 = 0xefcdab89u;
    std::uint32_t h2 = 0x98badcfeu;
    std::uint32_t h3 = 0x10325476u;
    std::uint32_t h4 = 0xc3d2e1f0u;

    std::array<std::uint32_t, 80> w {};

    for (std::size_t chunk = 0; chunk < buffer.size(); chunk += 64)
    {
        for (int i = 0; i < 16; ++i)
        {
            const auto idx = chunk + static_cast<std::size_t>(i) * 4u;
            w[static_cast<std::size_t>(i)] = (static_cast<std::uint32_t>(buffer[idx]) << 24)
                                             | (static_cast<std::uint32_t>(buffer[idx + 1]) << 16)
                                             | (static_cast<std::uint32_t>(buffer[idx + 2]) << 8)
                                             | static_cast<std::uint32_t>(buffer[idx + 3]);
        }

        for (int i = 16; i < 80; ++i)
        {
            w[static_cast<std::size_t>(i)] = rotateLeft(w[static_cast<std::size_t>(i - 3)]
                                                        ^ w[static_cast<std::size_t>(i - 8)]
                                                        ^ w[static_cast<std::size_t>(i - 14)]
                                                        ^ w[static_cast<std::size_t>(i - 16)],
                                                        1u);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (int i = 0; i < 80; ++i)
        {
            std::uint32_t f = 0;
            std::uint32_t k = 0;

            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999u;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ed9eba1u;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcu;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xca62c1d6u;
            }

            const auto temp = rotateLeft(a, 5u) + f + e + k + w[static_cast<std::size_t>(i)];
            e = d;
            d = c;
            c = rotateLeft(b, 30u);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<std::uint8_t, 20> digest {};

    auto writeBe32 = [&](std::uint32_t value, int offset) {
        digest[static_cast<std::size_t>(offset)] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
        digest[static_cast<std::size_t>(offset + 1)] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
        digest[static_cast<std::size_t>(offset + 2)] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
        digest[static_cast<std::size_t>(offset + 3)] = static_cast<std::uint8_t>(value & 0xffu);
    };

    writeBe32(h0, 0);
    writeBe32(h1, 4);
    writeBe32(h2, 8);
    writeBe32(h3, 12);
    writeBe32(h4, 16);

    return digest;
}
} // namespace

FramedSocketTransport::FramedSocketTransport(std::size_t maxFrameBytes)
    : juce::Thread("FramedSocketTransport")
    , maxFrameBytes_(maxFrameBytes)
{
}

FramedSocketTransport::~FramedSocketTransport()
{
    stop();
}

bool FramedSocketTransport::start(std::unique_ptr<juce::StreamingSocket> socket, std::string username, std::string password)
{
    if (socket == nullptr || ! socket->isConnected() || username.empty())
        return false;

    stop();

    const auto nowMs = static_cast<juce::int64>(juce::Time::getMillisecondCounter());

    {
        const juce::ScopedLock lock(stateLock_);
        socket_ = std::move(socket);
        username_ = std::move(username);
        password_ = std::move(password);

        outboundQueue_.clear();
        inboundQueue_.clear();
        remoteSourceActivityQueue_.clear();
        subscribedUserMasks_.clear();
        inboundTransfers_.clear();
        knownRemoteSourcesById_.clear();

        inboundHeader_ = { 0, 0, 0, 0, 0 };
        inboundHeaderBytes_ = 0;
        inboundType_ = 0;
        inboundPayload_.reset();
        inboundPayloadBytes_ = 0;
        inboundExpectedBytes_ = 0;

        socketFailed_ = false;
        authFailureReason_.clear();
        effectiveUsername_.clear();
        keepAliveSeconds_ = 3;
        awaitingAuthChallenge_ = true;
        awaitingAuthReply_ = false;
        authenticated_ = false;

        sentFrameCount_ = 0;
        receivedFrameCount_ = 0;
        serverTimingConfig_ = {};
        hasServerTimingConfig_ = false;

        lastSendMs_ = nowMs;
        lastReceiveMs_ = nowMs;
    }

    authEvent_.reset();
    wakeEvent_.signal();
    startThread();
    return true;
}

bool FramedSocketTransport::waitForAuthentication(int timeoutMs,
                                                  std::string* failureReason,
                                                  std::string* effectiveUsername)
{
    const auto boundedTimeout = juce::jmax(1, timeoutMs);

    if (! authEvent_.wait(boundedTimeout))
    {
        if (failureReason != nullptr)
            *failureReason = "authorization timeout";
        return false;
    }

    const juce::ScopedLock lock(stateLock_);

    if (authenticated_)
    {
        if (effectiveUsername != nullptr)
        {
            if (! effectiveUsername_.empty())
                *effectiveUsername = effectiveUsername_;
            else
                *effectiveUsername = username_;
        }

        return true;
    }

    if (failureReason != nullptr)
        *failureReason = authFailureReason_.empty() ? std::string("authentication failed") : authFailureReason_;

    return false;
}

void FramedSocketTransport::stop()
{
    signalThreadShouldExit();
    wakeEvent_.signal();
    authEvent_.signal();

    {
        const juce::ScopedLock lock(stateLock_);
        if (socket_ != nullptr)
            socket_->close();
    }

    stopThread(2000);

    {
        const juce::ScopedLock lock(stateLock_);
        socket_.reset();
        outboundQueue_.clear();
        inboundQueue_.clear();
        remoteSourceActivityQueue_.clear();
        subscribedUserMasks_.clear();
        inboundTransfers_.clear();
        knownRemoteSourcesById_.clear();

        inboundHeader_ = { 0, 0, 0, 0, 0 };
        inboundHeaderBytes_ = 0;
        inboundType_ = 0;
        inboundPayload_.reset();
        inboundPayloadBytes_ = 0;
        inboundExpectedBytes_ = 0;

        socketFailed_ = false;
        keepAliveSeconds_ = 3;
        authenticated_ = false;
        awaitingAuthChallenge_ = true;
        awaitingAuthReply_ = false;

        username_.clear();
        password_.clear();
        effectiveUsername_.clear();
        authFailureReason_.clear();
        serverTimingConfig_ = {};
        hasServerTimingConfig_ = false;

        lastSendMs_ = 0;
        lastReceiveMs_ = 0;
    }

    authEvent_.reset();
}

bool FramedSocketTransport::isRunning() const
{
    return isThreadRunning();
}

void FramedSocketTransport::enqueueOutbound(const juce::MemoryBlock& payload)
{
    if (payload.getSize() == 0)
        return;

    const juce::ScopedLock lock(stateLock_);
    outboundQueue_.push_back(payload);

    while (outboundQueue_.size() > kMaxQueuedFrames)
        outboundQueue_.pop_front();

    wakeEvent_.signal();
}

bool FramedSocketTransport::popInbound(juce::MemoryBlock& payload)
{
    InboundFrame frame;
    if (! popInboundFrame(frame))
        return false;

    payload = std::move(frame.payload);
    return true;
}

bool FramedSocketTransport::popInboundFrame(InboundFrame& frame)
{
    const juce::ScopedLock lock(stateLock_);

    if (inboundQueue_.empty())
        return false;

    frame = std::move(inboundQueue_.front());
    inboundQueue_.pop_front();
    return true;
}

bool FramedSocketTransport::popRemoteSourceActivityUpdate(RemoteSourceActivityUpdate& update)
{
    const juce::ScopedLock lock(stateLock_);

    if (remoteSourceActivityQueue_.empty())
        return false;

    update = std::move(remoteSourceActivityQueue_.front());
    remoteSourceActivityQueue_.pop_front();
    return true;
}


bool FramedSocketTransport::getServerTimingConfig(ServerTimingConfig& config) const
{
    const juce::ScopedLock lock(stateLock_);
    if (! hasServerTimingConfig_)
        return false;

    config = serverTimingConfig_;
    return true;
}
std::size_t FramedSocketTransport::getSentFrameCount() const
{
    const juce::ScopedLock lock(stateLock_);
    return sentFrameCount_;
}

std::size_t FramedSocketTransport::getReceivedFrameCount() const
{
    const juce::ScopedLock lock(stateLock_);
    return receivedFrameCount_;
}

int FramedSocketTransport::getKeepAliveSecondsForTesting() const
{
    const juce::ScopedLock lock(stateLock_);
    return keepAliveSeconds_;
}

void FramedSocketTransport::run()
{
    while (! threadShouldExit())
    {
        bool didWork = false;

        juce::MemoryBlock outbound;
        {
            const juce::ScopedLock lock(stateLock_);
            if (authenticated_ && ! outboundQueue_.empty())
            {
                outbound = outboundQueue_.front();
                outboundQueue_.pop_front();
            }
        }

        if (outbound.getSize() > 0)
        {
            if (! writeUploadInterval(outbound))
                break;

            {
                const juce::ScopedLock lock(stateLock_);
                ++sentFrameCount_;
            }

            didWork = true;
        }

        std::uint8_t inboundType = 0;
        juce::MemoryBlock inboundPayload;

        if (readOneMessage(inboundType, inboundPayload))
        {
            processInboundMessage(inboundType, inboundPayload);
            didWork = true;
        }

        bool shouldSendKeepAlive = false;
        bool markTimedOut = false;

        {
            const juce::ScopedLock lock(stateLock_);
            const auto nowMs = static_cast<juce::int64>(juce::Time::getMillisecondCounter());
            const auto keepAliveMs = static_cast<juce::int64>(juce::jmax(1, keepAliveSeconds_)) * 1000;

            if (authenticated_ && nowMs - lastSendMs_ >= keepAliveMs)
                shouldSendKeepAlive = true;

            if (nowMs - lastReceiveMs_ > keepAliveMs * 3)
                markTimedOut = true;
        }

        if (markTimedOut)
        {
            socketFailed_ = true;
            if (! authenticated_)
                failAuthentication("authorization timeout");
            break;
        }

        if (shouldSendKeepAlive)
        {
            if (! writeMessage(kMessageKeepAlive, nullptr, 0))
                break;

            didWork = true;
        }

        if (socketFailed_)
            break;

        if (! didWork)
            wakeEvent_.wait(5);
    }

    if (! threadShouldExit() && ! authenticated_ && authFailureReason_.empty())
        failAuthentication("connection closed");
}

bool FramedSocketTransport::writeMessage(std::uint8_t type, const void* payload, std::size_t payloadBytes)
{
    juce::StreamingSocket* socket = nullptr;

    {
        const juce::ScopedLock lock(stateLock_);
        socket = socket_.get();
    }

    if (socket == nullptr || ! socket->isConnected())
    {
        socketFailed_ = true;
        return false;
    }

    const auto maxPayloadBytes = juce::jmin(maxFrameBytes_, kNetMessageMaxSize);
    if (payloadBytes > maxPayloadBytes)
        return false;

    juce::MemoryBlock frame;
    frame.setSize(5 + payloadBytes);

    auto* bytes = static_cast<std::uint8_t*>(frame.getData());
    bytes[0] = type;
    encodeLe32(static_cast<std::uint32_t>(payloadBytes), bytes + 1);

    if (payloadBytes > 0 && payload != nullptr)
        std::memcpy(bytes + 5, payload, payloadBytes);

    int offset = 0;
    const auto total = static_cast<int>(frame.getSize());

    while (offset < total && ! threadShouldExit())
    {
        const auto ready = socket->waitUntilReady(false, 20);
        if (ready < 0)
        {
            socketFailed_ = true;
            return false;
        }

        if (ready == 0)
            continue;

        const auto wrote = socket->write(static_cast<const char*>(frame.getData()) + offset, total - offset);
        if (wrote <= 0)
        {
            socketFailed_ = true;
            return false;
        }

        offset += wrote;
    }

    if (offset == total)
    {
        const juce::ScopedLock lock(stateLock_);
        lastSendMs_ = static_cast<juce::int64>(juce::Time::getMillisecondCounter());
    }

    return offset == total;
}

bool FramedSocketTransport::readOneMessage(std::uint8_t& type, juce::MemoryBlock& payload)
{
    juce::StreamingSocket* socket = nullptr;

    {
        const juce::ScopedLock lock(stateLock_);
        socket = socket_.get();
    }

    if (socket == nullptr || ! socket->isConnected())
    {
        socketFailed_ = true;
        return false;
    }

    if (inboundHeaderBytes_ < 5)
    {
        const auto ready = socket->waitUntilReady(true, 0);
        if (ready < 0)
        {
            socketFailed_ = true;
            return false;
        }

        if (ready == 0)
            return false;

        const auto readBytes = socket->read(inboundHeader_.data() + inboundHeaderBytes_, 5 - inboundHeaderBytes_, false);
        if (readBytes < 0)
        {
            socketFailed_ = true;
            return false;
        }

        if (readBytes == 0)
            return false;

        inboundHeaderBytes_ += readBytes;

        if (inboundHeaderBytes_ < 5)
            return false;

        inboundType_ = static_cast<std::uint8_t>(inboundHeader_[0]);
        inboundExpectedBytes_ = decodeLe32(inboundHeader_.data() + 1);

        const auto maxPayloadBytes = static_cast<std::uint32_t>(juce::jmin(maxFrameBytes_, kNetMessageMaxSize));
        if (inboundExpectedBytes_ > maxPayloadBytes)
        {
            socketFailed_ = true;
            return false;
        }

        inboundPayload_.setSize(inboundExpectedBytes_);
        inboundPayloadBytes_ = 0;

        if (inboundExpectedBytes_ == 0)
        {
            type = inboundType_;
            payload.reset();

            inboundHeaderBytes_ = 0;
            inboundType_ = 0;
            inboundExpectedBytes_ = 0;

            const juce::ScopedLock lock(stateLock_);
            lastReceiveMs_ = static_cast<juce::int64>(juce::Time::getMillisecondCounter());
            return true;
        }
    }

    const auto ready = socket->waitUntilReady(true, 0);
    if (ready < 0)
    {
        socketFailed_ = true;
        return false;
    }

    if (ready == 0)
        return false;

    auto* inboundData = static_cast<char*>(inboundPayload_.getData());
    const auto readBytes = socket->read(inboundData + inboundPayloadBytes_,
                                        static_cast<int>(inboundExpectedBytes_) - inboundPayloadBytes_,
                                        false);

    if (readBytes < 0)
    {
        socketFailed_ = true;
        return false;
    }

    if (readBytes == 0)
        return false;

    inboundPayloadBytes_ += readBytes;

    if (inboundPayloadBytes_ < static_cast<int>(inboundExpectedBytes_))
        return false;

    type = inboundType_;
    payload = inboundPayload_;

    inboundHeaderBytes_ = 0;
    inboundType_ = 0;
    inboundExpectedBytes_ = 0;
    inboundPayload_.reset();
    inboundPayloadBytes_ = 0;

    const juce::ScopedLock lock(stateLock_);
    lastReceiveMs_ = static_cast<juce::int64>(juce::Time::getMillisecondCounter());
    return true;
}

bool FramedSocketTransport::writeUploadInterval(const juce::MemoryBlock& payload)
{
    if (payload.getSize() == 0)
        return true;

    std::array<std::uint8_t, 16> guid {};
    auto& random = juce::Random::getSystemRandom();
    for (auto& byte : guid)
        byte = static_cast<std::uint8_t>(random.nextInt(256));

    std::array<std::uint8_t, 25> beginPayload {};
    std::memcpy(beginPayload.data(), guid.data(), guid.size());
    encodeLe32(static_cast<std::uint32_t>(payload.getSize()), beginPayload.data() + 16);
    encodeLe32(kUploadFourCcOggv, beginPayload.data() + 20);
    beginPayload[24] = 0;

    if (! writeMessage(kMessageClientUploadIntervalBegin, beginPayload.data(), beginPayload.size()))
        return false;

    auto payloadOffset = static_cast<std::size_t>(0);
    const auto* payloadBytes = static_cast<const std::uint8_t*>(payload.getData());

    while (payloadOffset < payload.getSize())
    {
        const auto chunkBytes = juce::jmin(kMaxUploadChunkBytes, payload.getSize() - payloadOffset);

        juce::MemoryBlock writePayload;
        writePayload.setSize(17 + chunkBytes);
        auto* writeBytes = static_cast<std::uint8_t*>(writePayload.getData());
        std::memcpy(writeBytes, guid.data(), guid.size());
        writeBytes[16] = static_cast<std::uint8_t>((payloadOffset + chunkBytes) >= payload.getSize() ? 1u : 0u);
        std::memcpy(writeBytes + 17, payloadBytes + payloadOffset, chunkBytes);

        if (! writeMessage(kMessageClientUploadIntervalWrite, writePayload.getData(), writePayload.getSize()))
            return false;

        payloadOffset += chunkBytes;
    }

    return true;
}

void FramedSocketTransport::processInboundMessage(std::uint8_t type, const juce::MemoryBlock& payload)
{
    switch (type)
    {
        case kMessageServerAuthChallenge:
            handleAuthChallenge(payload);
            break;

        case kMessageServerAuthReply:
            handleAuthReply(payload);
            break;

        case kMessageServerConfigChange:
            handleConfigChange(payload);
            break;

        case kMessageServerUserInfoChange:
            handleUserInfo(payload);
            break;

        case kMessageServerDownloadIntervalBegin:
            handleDownloadBegin(payload);
            break;

        case kMessageServerDownloadIntervalWrite:
            handleDownloadWrite(payload);
            break;

        case kMessageKeepAlive:
            break;

        default:
            break;
    }
}

void FramedSocketTransport::handleAuthChallenge(const juce::MemoryBlock& payload)
{
    if (payload.getSize() < 16)
    {
        failAuthentication("invalid auth challenge");
        return;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        if (! awaitingAuthChallenge_)
            return;
    }

    std::array<std::uint8_t, 8> challenge {};
    const auto* payloadBytes = static_cast<const std::uint8_t*>(payload.getData());
    std::memcpy(challenge.data(), payloadBytes, challenge.size());

    const auto serverCaps = decodeLe32(payloadBytes + 8);
    const auto protocolVersion = decodeLe32(payloadBytes + 12);

    if (protocolVersion < kProtoVerMin || protocolVersion >= kProtoVerMax)
    {
        failAuthentication("server protocol mismatch");
        return;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        const auto keepAlive = static_cast<int>((serverCaps >> 8) & 0xffu);
        keepAliveSeconds_ = juce::jlimit(1, 60, keepAlive > 0 ? keepAlive : 3);
    }

    const auto passHash = buildAuthPassHash(challenge);

    std::vector<std::uint8_t> authPayload;
    {
        const juce::ScopedLock lock(stateLock_);

        if (username_.empty())
        {
            failAuthentication("username is empty");
            return;
        }

        const bool licenseRequired = (serverCaps & 1u) != 0u;
        const auto clientCaps = kClientCapsHasVersion | (licenseRequired ? kClientCapsLicenseAccepted : 0u);

        authPayload.resize(20 + username_.size() + 1 + 4 + 4);
        std::size_t offset = 0;

        std::memcpy(authPayload.data() + offset, passHash.data(), passHash.size());
        offset += passHash.size();

        std::memcpy(authPayload.data() + offset, username_.data(), username_.size());
        offset += username_.size();
        authPayload[offset++] = 0;

        encodeLe32(clientCaps, authPayload.data() + offset);
        offset += 4;
        encodeLe32(kProtoVerCur, authPayload.data() + offset);

        awaitingAuthChallenge_ = false;
        awaitingAuthReply_ = true;
    }

    if (! writeMessage(kMessageClientAuthUser, authPayload.data(), authPayload.size()))
        failAuthentication("failed to send auth request");
}

void FramedSocketTransport::handleAuthReply(const juce::MemoryBlock& payload)
{
    if (payload.getSize() < 1)
    {
        failAuthentication("invalid auth reply");
        return;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        if (! awaitingAuthReply_)
            return;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    const auto success = (bytes[0] & 1u) != 0u;

    std::string serverText;
    if (payload.getSize() > 1)
    {
        const char* text = reinterpret_cast<const char*>(bytes + 1);
        const auto maxTextLength = payload.getSize() - 1;
        const auto textLength = boundedStrnlen(text, maxTextLength);
        serverText.assign(text, textLength);
    }

    if (! success)
    {
        failAuthentication(serverText.empty() ? std::string("invalid login/password") : serverText);
        return;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        authenticated_ = true;
        awaitingAuthReply_ = false;
        authFailureReason_.clear();
        effectiveUsername_ = username_;
    }

    authEvent_.signal();

    sendDefaultChannelInfo();
}


void FramedSocketTransport::handleConfigChange(const juce::MemoryBlock& payload)
{
    if (payload.getSize() < 4)
        return;

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    const auto bpm = static_cast<int>(bytes[0]) | (static_cast<int>(bytes[1]) << 8);
    const auto bpi = static_cast<int>(bytes[2]) | (static_cast<int>(bytes[3]) << 8);

    if (bpm <= 0 || bpi <= 0)
        return;

    const juce::ScopedLock lock(stateLock_);
    serverTimingConfig_.beatsPerMinute = bpm;
    serverTimingConfig_.beatsPerInterval = bpi;
    hasServerTimingConfig_ = true;
}
void FramedSocketTransport::handleUserInfo(const juce::MemoryBlock& payload)
{
    bool isAuthenticated = false;
    std::string ownUsername;

    {
        const juce::ScopedLock lock(stateLock_);
        isAuthenticated = authenticated_;
        ownUsername = effectiveUsername_.empty() ? username_ : effectiveUsername_;
    }

    if (! isAuthenticated)
        return;

    const auto ownIdentity = normalizeUserIdentity(std::move(ownUsername));

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    std::size_t offset = 0;

    while (offset + 6 <= payload.getSize())
    {
        const bool isActive = bytes[offset++] != 0;
        const auto channel = bytes[offset++];
        offset += 2;
        offset += 1;
        offset += 1;

        const char* username = reinterpret_cast<const char*>(bytes + offset);
        const auto usernameAvail = payload.getSize() - offset;
        const auto usernameLength = boundedStrnlen(username, usernameAvail);
        if (usernameLength == usernameAvail)
            break;

        offset += usernameLength + 1;

        const char* channelName = reinterpret_cast<const char*>(bytes + offset);
        const auto channelNameAvail = payload.getSize() - offset;
        const auto channelNameLength = boundedStrnlen(channelName, channelNameAvail);
        if (channelNameLength == channelNameAvail)
            break;

        offset += channelNameLength + 1;

        const std::string channelLabel(channelName, channelNameLength);

        if (channel >= 32)
            continue;

        const std::string remoteUser(username, usernameLength);
        if (remoteUser.empty() || normalizeUserIdentity(remoteUser) == ownIdentity)
            continue;

        const auto sourceInfo = makeRemoteSourceInfo(remoteUser, channel, channelLabel);
        {
            const juce::ScopedLock lock(stateLock_);
            if (isActive)
                knownRemoteSourcesById_[sourceInfo.sourceId] = sourceInfo;
            else
                knownRemoteSourcesById_.erase(sourceInfo.sourceId);

            remoteSourceActivityQueue_.push_back(RemoteSourceActivityUpdate {
                .sourceInfo = sourceInfo,
                .active = isActive,
            });

            while (remoteSourceActivityQueue_.size() > kMaxQueuedFrames)
                remoteSourceActivityQueue_.pop_front();
        }

        if (! isActive)
            continue;

        const auto bit = static_cast<std::uint32_t>(1u << channel);

        std::uint32_t existingMask = 0;
        {
            const juce::ScopedLock lock(stateLock_);
            const auto it = subscribedUserMasks_.find(remoteUser);
            if (it != subscribedUserMasks_.end())
                existingMask = it->second;
        }

        const auto nextMask = existingMask | bit;
        if (nextMask != existingMask)
            sendUserMask(remoteUser, nextMask);
    }
}

void FramedSocketTransport::handleDownloadBegin(const juce::MemoryBlock& payload)
{
    if (payload.getSize() < 26)
        return;

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    const auto fourcc = decodeLe32(bytes + 20);

    if (fourcc == 0)
        return;

    const auto key = makeGuidKey(bytes);
    auto sourceInfo = makeDownloadSourceInfo(payload);
    if (sourceInfo.sourceId.empty())
    {
        sourceInfo.sourceId = key;
        sourceInfo.groupId = sourceInfo.sourceId;
        sourceInfo.userName = sourceInfo.sourceId;
        sourceInfo.displayName = sourceInfo.sourceId;
    }

    const juce::ScopedLock lock(stateLock_);
    auto& transfer = inboundTransfers_[key];
    transfer.payload.reset();
    transfer.sourceId = sourceInfo.sourceId;
    transfer.sourceInfo = std::move(sourceInfo);
}

void FramedSocketTransport::handleDownloadWrite(const juce::MemoryBlock& payload)
{
    if (payload.getSize() < 17)
        return;

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    const auto flags = bytes[16];

    const auto key = makeGuidKey(bytes);

    juce::MemoryBlock completedPayload;
    std::string sourceId;
    RemoteSourceInfo sourceInfo;

    {
        const juce::ScopedLock lock(stateLock_);
        auto transferIt = inboundTransfers_.find(key);

        if (transferIt == inboundTransfers_.end())
        {
            InboundTransferState seededTransfer;
            seededTransfer.sourceId = key;
            seededTransfer.sourceInfo.sourceId = key;
            seededTransfer.sourceInfo.groupId = key;
            seededTransfer.sourceInfo.userName = key;
            seededTransfer.sourceInfo.displayName = key;
            transferIt = inboundTransfers_.emplace(key, std::move(seededTransfer)).first;
        }

        auto& transfer = transferIt->second;

        const auto audioBytes = payload.getSize() - 17;
        if (audioBytes > 0)
            transfer.payload.append(bytes + 17, audioBytes);

        if ((flags & 1u) != 0u)
        {
            completedPayload = transfer.payload;
            sourceId = transfer.sourceId.empty() ? key : transfer.sourceId;
            sourceInfo = transfer.sourceInfo;
            inboundTransfers_.erase(transferIt);
        }
    }

    if (completedPayload.getSize() == 0)
        return;

    const juce::ScopedLock lock(stateLock_);
    if (sourceInfo.sourceId.empty())
    {
        sourceInfo.sourceId = sourceId;
        sourceInfo.groupId = sourceId;
        sourceInfo.userName = sourceId;
        sourceInfo.displayName = sourceId;
    }

    inboundQueue_.push_back(InboundFrame { std::move(completedPayload), std::move(sourceId), std::move(sourceInfo) });

    while (inboundQueue_.size() > kMaxQueuedFrames)
        inboundQueue_.pop_front();

    ++receivedFrameCount_;
}

void FramedSocketTransport::sendUserMask(const std::string& username, std::uint32_t channelMask)
{
    if (username.empty() || channelMask == 0)
        return;

    std::vector<std::uint8_t> payload(username.size() + 1 + 4);
    std::memcpy(payload.data(), username.data(), username.size());
    payload[username.size()] = 0;
    encodeLe32(channelMask, payload.data() + username.size() + 1);

    if (! writeMessage(kMessageClientSetUserMask, payload.data(), payload.size()))
    {
        socketFailed_ = true;
        return;
    }

    const juce::ScopedLock lock(stateLock_);
    subscribedUserMasks_[username] = channelMask;
}

void FramedSocketTransport::sendDefaultChannelInfo()
{
    std::array<std::uint8_t, 7> payload {};
    payload[0] = 4;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 0;
    payload[4] = 0;
    payload[5] = 0;
    payload[6] = 0;

    if (! writeMessage(kMessageClientSetChannelInfo, payload.data(), payload.size()))
        socketFailed_ = true;
}

void FramedSocketTransport::failAuthentication(const std::string& reason)
{
    {
        const juce::ScopedLock lock(stateLock_);
        if (! authFailureReason_.empty())
            return;

        authFailureReason_ = reason.empty() ? std::string("authentication failed") : reason;
        awaitingAuthChallenge_ = false;
        awaitingAuthReply_ = false;
        authenticated_ = false;
        socketFailed_ = true;
    }

    authEvent_.signal();
    wakeEvent_.signal();
}

std::array<std::uint8_t, 20> FramedSocketTransport::buildAuthPassHash(const std::array<std::uint8_t, 8>& challenge) const
{
    std::string passInput;

    {
        const juce::ScopedLock lock(stateLock_);
        passInput = username_;
        passInput += ':';
        passInput += password_;
    }

    const auto firstHash = sha1Digest(passInput.data(), passInput.size());

    std::array<std::uint8_t, 28> secondInput {};
    std::memcpy(secondInput.data(), firstHash.data(), firstHash.size());
    std::memcpy(secondInput.data() + firstHash.size(), challenge.data(), challenge.size());

    return sha1Digest(secondInput.data(), secondInput.size());
}

std::string FramedSocketTransport::makeGuidKey(const std::uint8_t* guidBytes) const
{
    return std::string(reinterpret_cast<const char*>(guidBytes), 16);
}

std::string FramedSocketTransport::makeDownloadSourceId(const juce::MemoryBlock& payload) const
{
    return makeDownloadSourceInfo(payload).sourceId;
}

FramedSocketTransport::RemoteSourceInfo FramedSocketTransport::makeRemoteSourceInfo(std::string username,
                                                                                   std::uint8_t channelIndex,
                                                                                   std::string channelName) const
{
    RemoteSourceInfo info;
    info.userName = std::move(username);
    info.channelName = std::move(channelName);
    info.channelIndex = channelIndex;
    info.groupId = normalizeUserIdentity(info.userName);
    info.sourceId = info.groupId + "#" + std::to_string(static_cast<unsigned>(channelIndex));
    info.displayName = makeRemoteChannelLabel(info.userName, info.channelName, channelIndex);
    return info;
}

FramedSocketTransport::RemoteSourceInfo FramedSocketTransport::makeDownloadSourceInfo(const juce::MemoryBlock& payload) const
{
    if (payload.getSize() < 26)
        return {};

    const auto* bytes = static_cast<const std::uint8_t*>(payload.getData());
    const auto channelIndex = bytes[24];

    const auto usernameBytes = bytes + 25;
    const auto usernameAvail = payload.getSize() - 25;
    const auto usernameLength = boundedStrnlen(reinterpret_cast<const char*>(usernameBytes), usernameAvail);

    if (usernameLength == 0 || usernameLength == usernameAvail)
        return {};

    const std::string username(reinterpret_cast<const char*>(usernameBytes), usernameLength);
    auto sourceInfo = makeRemoteSourceInfo(username, channelIndex, {});

    const juce::ScopedLock lock(stateLock_);
    if (const auto it = knownRemoteSourcesById_.find(sourceInfo.sourceId); it != knownRemoteSourcesById_.end())
        return it->second;

    return sourceInfo;
}
} // namespace famalamajam::net






