#include "ConnectionFailureClassifier.h"

#include <array>
#include <string>

namespace famalamajam::app::session
{
namespace
{
[[nodiscard]] bool containsToken(const std::string& value, const std::array<const char*, 6>& tokens)
{
    for (const auto* token : tokens)
    {
        if (value.find(token) != std::string::npos)
            return true;
    }

    return false;
}

[[nodiscard]] bool containsToken(const std::string& value, const std::array<const char*, 7>& tokens)
{
    for (const auto* token : tokens)
    {
        if (value.find(token) != std::string::npos)
            return true;
    }

    return false;
}

[[nodiscard]] std::string normalizeReason(std::string_view reason)
{
    std::string normalized(reason);
    for (auto& ch : normalized)
    {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }

    return normalized;
}
} // namespace

ConnectionFailureKind classifyFailure(ConnectionEventType eventType, std::string_view reason) noexcept
{
    if (eventType == ConnectionEventType::ConnectionLostRetryable)
        return ConnectionFailureKind::RetryableTransport;

    if (eventType != ConnectionEventType::ConnectionFailed)
        return ConnectionFailureKind::NonRetryableUnknown;

    const auto normalizedReason = normalizeReason(reason);

    static constexpr std::array<const char*, 6> authTokens {
        "auth",
        "unauthorized",
        "forbidden",
        "credential",
        "password",
        "token",
    };

    static constexpr std::array<const char*, 6> protocolTokens {
        "protocol",
        "handshake",
        "malformed",
        "version",
        "framing",
        "codec",
    };

    static constexpr std::array<const char*, 7> configurationTokens {
        "config",
        "configuration",
        "invalid host",
        "invalid port",
        "invalid user",
        "missing host",
        "missing user",
    };

    static constexpr std::array<const char*, 7> transportTokens {
        "timeout",
        "timed out",
        "socket",
        "network",
        "connection reset",
        "unreachable",
        "transport",
    };

    if (containsToken(normalizedReason, authTokens))
        return ConnectionFailureKind::NonRetryableAuthentication;

    if (containsToken(normalizedReason, protocolTokens))
        return ConnectionFailureKind::NonRetryableProtocol;

    if (containsToken(normalizedReason, configurationTokens))
        return ConnectionFailureKind::NonRetryableConfiguration;

    if (containsToken(normalizedReason, transportTokens))
        return ConnectionFailureKind::RetryableTransport;

    return ConnectionFailureKind::NonRetryableUnknown;
}

bool isRetryableFailure(ConnectionFailureKind kind) noexcept
{
    return kind == ConnectionFailureKind::RetryableTransport;
}
} // namespace famalamajam::app::session
