#include "ConnectionLifecycleController.h"

#include <utility>

namespace famalamajam::app::session
{
namespace
{
[[nodiscard]] int secondsFromDelayMs(int delayMs)
{
    if (delayMs <= 0)
        return 0;

    return (delayMs + 999) / 1000;
}
} // namespace

ConnectionLifecycleController::ConnectionLifecycleController(RetryPolicy retryPolicy)
    : snapshot_ {}
    , retryPolicy_(std::move(retryPolicy))
{
}

ConnectionLifecycleSnapshot ConnectionLifecycleController::getSnapshot() const
{
    const std::scoped_lock lock(mutex_);
    return snapshot_;
}

ConnectionLifecycleTransition ConnectionLifecycleController::handleCommand(ConnectionCommand command)
{
    const std::scoped_lock lock(mutex_);
    auto transition = reduceLifecycleCommand(snapshot_, command);

    if (transition.changed)
    {
        if (command == ConnectionCommand::Connect || command == ConnectionCommand::ApplySettings)
        {
            transition.snapshot.retryAttempt = 0;
            transition.snapshot.retryAttemptLimit = retryPolicy_.maxAttempts;
            transition.snapshot.nextRetryDelayMs = 0;
        }

        snapshot_ = transition.snapshot;
    }

    return transition;
}

ConnectionLifecycleTransition ConnectionLifecycleController::handleEvent(const ConnectionEvent& event)
{
    const std::scoped_lock lock(mutex_);
    auto transition = reduceLifecycleEvent(snapshot_, event);

    if (! transition.changed)
        return transition;

    if (event.type == ConnectionEventType::Connected || event.type == ConnectionEventType::Disconnected)
    {
        transition.snapshot.retryAttempt = 0;
        transition.snapshot.retryAttemptLimit = retryPolicy_.maxAttempts;
        transition.snapshot.nextRetryDelayMs = 0;
        transition.effect = LifecycleEffect::None;

        snapshot_ = transition.snapshot;
        return transition;
    }

    const auto failureKind = classifyFailure(event.type, event.reason);
    transition.snapshot.retryAttemptLimit = retryPolicy_.maxAttempts;

    if (isRetryableFailure(failureKind))
    {
        if (retryPolicy_.hasAttemptsRemaining(snapshot_.retryAttempt))
        {
            const auto nextAttempt = snapshot_.retryAttempt + 1;
            const auto nextDelayMs = retryPolicy_.nextDelay(nextAttempt);

            transition.snapshot.state = ConnectionState::Reconnecting;
            transition.snapshot.lastError = event.reason;
            transition.snapshot.retryAttempt = nextAttempt;
            transition.snapshot.nextRetryDelayMs = nextDelayMs;
            transition.snapshot.statusMessage = makeReconnectStatus(nextAttempt,
                                                                    retryPolicy_.maxAttempts,
                                                                    nextDelayMs,
                                                                    event.reason);
            transition.effect = LifecycleEffect::ScheduleReconnect;

            snapshot_ = transition.snapshot;
            return transition;
        }

        transition.snapshot.state = ConnectionState::Error;
        transition.snapshot.retryAttempt = snapshot_.retryAttempt;
        transition.snapshot.nextRetryDelayMs = 0;
        transition.snapshot.statusMessage = makeRetryExhaustedStatus(retryPolicy_.maxAttempts, event.reason);
        transition.effect = LifecycleEffect::None;

        snapshot_ = transition.snapshot;
        return transition;
    }

    transition.snapshot.state = ConnectionState::Error;
    transition.snapshot.retryAttempt = snapshot_.retryAttempt;
    transition.snapshot.nextRetryDelayMs = 0;
    transition.snapshot.statusMessage = makeTerminalStatus(failureKind, event.reason);
    transition.effect = LifecycleEffect::None;

    snapshot_ = transition.snapshot;
    return transition;
}

ConnectionLifecycleTransition ConnectionLifecycleController::triggerScheduledReconnect()
{
    const std::scoped_lock lock(mutex_);

    if (! snapshot_.hasPendingRetry())
    {
        return ConnectionLifecycleTransition {
            .snapshot = snapshot_,
            .effect = LifecycleEffect::None,
            .changed = false,
            .suppressed = true,
        };
    }

    auto next = snapshot_;
    next.state = ConnectionState::Connecting;
    next.statusMessage = "Reconnecting (attempt " + std::to_string(next.retryAttempt) + "/"
                       + std::to_string(next.retryAttemptLimit) + ")";
    next.nextRetryDelayMs = 0;

    snapshot_ = next;

    return ConnectionLifecycleTransition {
        .snapshot = snapshot_,
        .effect = LifecycleEffect::BeginConnect,
        .changed = true,
        .suppressed = false,
    };
}

ConnectionLifecycleTransition ConnectionLifecycleController::resetToIdle()
{
    const std::scoped_lock lock(mutex_);

    snapshot_ = ConnectionLifecycleSnapshot {};

    return ConnectionLifecycleTransition {
        .snapshot = snapshot_,
        .effect = LifecycleEffect::None,
        .changed = true,
        .suppressed = false,
    };
}

std::string ConnectionLifecycleController::makeReconnectStatus(std::size_t attempt,
                                                               std::size_t attemptLimit,
                                                               int delayMs,
                                                               const std::string& reason)
{
    std::string status = "Reconnecting in " + std::to_string(secondsFromDelayMs(delayMs)) + "s (attempt "
                       + std::to_string(attempt) + "/" + std::to_string(attemptLimit) + ")";

    if (! reason.empty())
        status += ": " + reason;

    return status;
}

std::string ConnectionLifecycleController::makeRetryExhaustedStatus(std::size_t attemptLimit, const std::string& reason)
{
    std::string status = "Error: retries exhausted after " + std::to_string(attemptLimit) + " attempts";

    if (! reason.empty())
        status += ": " + reason;

    status += ". Press Connect to retry.";
    return status;
}

std::string ConnectionLifecycleController::makeTerminalStatus(ConnectionFailureKind kind, const std::string& reason)
{
    std::string status = "Error";

    if (! reason.empty())
        status += ": " + reason;

    switch (kind)
    {
        case ConnectionFailureKind::NonRetryableConfiguration:
            status += ". Fix settings and press Connect.";
            break;
        case ConnectionFailureKind::NonRetryableAuthentication:
            status += ". Update credentials and press Connect.";
            break;
        case ConnectionFailureKind::NonRetryableProtocol:
            status += ". Check server compatibility and press Connect.";
            break;
        case ConnectionFailureKind::RetryableTransport:
        case ConnectionFailureKind::NonRetryableUnknown:
            status += ". Press Connect to retry.";
            break;
    }

    return status;
}
} // namespace famalamajam::app::session
