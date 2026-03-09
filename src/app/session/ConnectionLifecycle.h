#pragma once

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

    [[nodiscard]] bool canConnect() const noexcept;
    [[nodiscard]] bool canDisconnect() const noexcept;
    [[nodiscard]] bool isConnected() const noexcept;
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