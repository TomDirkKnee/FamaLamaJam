#include "ConnectionLifecycle.h"

#include <utility>

namespace famalamajam::app::session
{
namespace
{
[[nodiscard]] ConnectionLifecycleTransition makeSuppressed(const ConnectionLifecycleSnapshot& current)
{
    return ConnectionLifecycleTransition {
        .snapshot = current,
        .effect = LifecycleEffect::None,
        .changed = false,
        .suppressed = true,
    };
}

[[nodiscard]] ConnectionLifecycleSnapshot makeSnapshot(ConnectionState state,
                                                       std::string statusMessage,
                                                       std::string lastError)
{
    return ConnectionLifecycleSnapshot {
        .state = state,
        .statusMessage = std::move(statusMessage),
        .lastError = std::move(lastError),
    };
}

[[nodiscard]] std::string withReason(const char* prefix, const std::string& reason)
{
    if (reason.empty())
        return prefix;

    std::string message(prefix);
    message += ": ";
    message += reason;
    return message;
}
} // namespace

bool ConnectionLifecycleSnapshot::canConnect() const noexcept
{
    return state == ConnectionState::Idle || state == ConnectionState::Error;
}

bool ConnectionLifecycleSnapshot::canDisconnect() const noexcept
{
    return state == ConnectionState::Connecting || state == ConnectionState::Active || state == ConnectionState::Reconnecting;
}

bool ConnectionLifecycleSnapshot::isConnected() const noexcept
{
    return state == ConnectionState::Active;
}

bool ConnectionLifecycleSnapshot::hasPendingRetry() const noexcept
{
    return state == ConnectionState::Reconnecting && nextRetryDelayMs > 0;
}

const char* toString(ConnectionState state) noexcept
{
    switch (state)
    {
        case ConnectionState::Idle:
            return "idle";
        case ConnectionState::Connecting:
            return "connecting";
        case ConnectionState::Active:
            return "active";
        case ConnectionState::Reconnecting:
            return "reconnecting";
        case ConnectionState::Error:
            return "error";
        default:
            return "unknown";
    }
}

ConnectionLifecycleTransition reduceLifecycleCommand(const ConnectionLifecycleSnapshot& current,
                                                     ConnectionCommand command)
{
    if (command == ConnectionCommand::Connect)
    {
        if (! current.canConnect())
            return makeSuppressed(current);

        return ConnectionLifecycleTransition {
            .snapshot = makeSnapshot(ConnectionState::Connecting, "Connecting", ""),
            .effect = LifecycleEffect::BeginConnect,
            .changed = true,
            .suppressed = false,
        };
    }

    if (command == ConnectionCommand::ApplySettings)
    {
        if (current.state != ConnectionState::Error)
            return makeSuppressed(current);

        return ConnectionLifecycleTransition {
            .snapshot = makeSnapshot(ConnectionState::Idle, "Ready. Press Connect to retry.", ""),
            .effect = LifecycleEffect::None,
            .changed = true,
            .suppressed = false,
        };
    }

    if (! current.canDisconnect())
        return makeSuppressed(current);

    const bool cancellingPendingConnect = current.state == ConnectionState::Connecting || current.state == ConnectionState::Reconnecting;
    return ConnectionLifecycleTransition {
        .snapshot = makeSnapshot(ConnectionState::Idle, "Disconnected", ""),
        .effect = cancellingPendingConnect ? LifecycleEffect::CancelPendingConnect : LifecycleEffect::BeginDisconnect,
        .changed = true,
        .suppressed = false,
    };
}

ConnectionLifecycleTransition reduceLifecycleEvent(const ConnectionLifecycleSnapshot& current, const ConnectionEvent& event)
{
    switch (event.type)
    {
        case ConnectionEventType::Connected:
        {
            if (current.state != ConnectionState::Connecting && current.state != ConnectionState::Reconnecting)
                return makeSuppressed(current);

            const auto connectedStatus = event.reason.empty() ? std::string("Connected") : event.reason;
            return ConnectionLifecycleTransition {
                .snapshot = makeSnapshot(ConnectionState::Active, connectedStatus, ""),
                .effect = LifecycleEffect::None,
                .changed = true,
                .suppressed = false,
            };
        }

        case ConnectionEventType::Disconnected:
            if (current.state == ConnectionState::Idle)
                return makeSuppressed(current);

            return ConnectionLifecycleTransition {
                .snapshot = makeSnapshot(ConnectionState::Idle, "Disconnected", current.lastError),
                .effect = LifecycleEffect::None,
                .changed = true,
                .suppressed = false,
            };

        case ConnectionEventType::ConnectionLostRetryable:
            if (current.state != ConnectionState::Active && current.state != ConnectionState::Connecting
                && current.state != ConnectionState::Reconnecting)
            {
                return makeSuppressed(current);
            }

            return ConnectionLifecycleTransition {
                .snapshot = makeSnapshot(ConnectionState::Reconnecting, withReason("Reconnecting", event.reason), event.reason),
                .effect = LifecycleEffect::None,
                .changed = true,
                .suppressed = false,
            };

        case ConnectionEventType::ConnectionFailed:
            if (current.state == ConnectionState::Idle)
                return makeSuppressed(current);

            return ConnectionLifecycleTransition {
                .snapshot = makeSnapshot(ConnectionState::Error, withReason("Error", event.reason), event.reason),
                .effect = LifecycleEffect::None,
                .changed = true,
                .suppressed = false,
            };
    }

    return makeSuppressed(current);
}
} // namespace famalamajam::app::session

