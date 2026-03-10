#pragma once

#include <cstddef>
#include <string>

namespace famalamajam::app::session
{
enum class ConnectionState
{
    Idle,
    Connecting,
    Active,
    Reconnecting,
    Error,
};

enum class ConnectionCommand
{
    Connect,
    Disconnect,
    ApplySettings,
};

enum class ConnectionEventType
{
    Connected,
    Disconnected,
    ConnectionLostRetryable,
    ConnectionFailed,
};

enum class LifecycleEffect
{
    None,
    BeginConnect,
    BeginDisconnect,
    CancelPendingConnect,
    ScheduleReconnect,
};

struct ConnectionEvent
{
    ConnectionEventType type { ConnectionEventType::Connected };
    std::string reason;
};

struct ConnectionLifecycleSnapshot
{
    ConnectionState state { ConnectionState::Idle };
    std::string statusMessage { "Idle" };
    std::string lastError;
    std::size_t retryAttempt { 0 };
    std::size_t retryAttemptLimit { 0 };
    int nextRetryDelayMs { 0 };

    [[nodiscard]] bool canConnect() const noexcept;
    [[nodiscard]] bool canDisconnect() const noexcept;
    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] bool hasPendingRetry() const noexcept;
};

struct ConnectionLifecycleTransition
{
    ConnectionLifecycleSnapshot snapshot;
    LifecycleEffect effect { LifecycleEffect::None };
    bool changed { false };
    bool suppressed { false };
};

[[nodiscard]] const char* toString(ConnectionState state) noexcept;

[[nodiscard]] ConnectionLifecycleTransition reduceLifecycleCommand(const ConnectionLifecycleSnapshot& current,
                                                                  ConnectionCommand command);
[[nodiscard]] ConnectionLifecycleTransition reduceLifecycleEvent(const ConnectionLifecycleSnapshot& current,
                                                                const ConnectionEvent& event);
} // namespace famalamajam::app::session
