#pragma once

#include <string_view>

#include "ConnectionLifecycle.h"

namespace famalamajam::app::session
{
enum class ConnectionFailureKind
{
    RetryableTransport,
    NonRetryableConfiguration,
    NonRetryableAuthentication,
    NonRetryableProtocol,
    NonRetryableUnknown,
};

[[nodiscard]] ConnectionFailureKind classifyFailure(ConnectionEventType eventType, std::string_view reason) noexcept;
[[nodiscard]] bool isRetryableFailure(ConnectionFailureKind kind) noexcept;
} // namespace famalamajam::app::session
