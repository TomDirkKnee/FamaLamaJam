#pragma once

#include <mutex>

#include "ConnectionFailureClassifier.h"
#include "ConnectionLifecycle.h"
#include "RetryPolicy.h"

namespace famalamajam::app::session
{
class ConnectionLifecycleController
{
public:
    explicit ConnectionLifecycleController(RetryPolicy retryPolicy = {});

    [[nodiscard]] ConnectionLifecycleSnapshot getSnapshot() const;
    [[nodiscard]] ConnectionLifecycleTransition handleCommand(ConnectionCommand command);
    [[nodiscard]] ConnectionLifecycleTransition handleEvent(const ConnectionEvent& event);

    [[nodiscard]] ConnectionLifecycleTransition triggerScheduledReconnect();
    ConnectionLifecycleTransition resetToIdle();

private:
    [[nodiscard]] static std::string makeReconnectStatus(std::size_t attempt,
                                                         std::size_t attemptLimit,
                                                         int delayMs,
                                                         const std::string& reason);
    [[nodiscard]] static std::string makeRetryExhaustedStatus(std::size_t attemptLimit, const std::string& reason);
    [[nodiscard]] static std::string makeTerminalStatus(ConnectionFailureKind kind, const std::string& reason);

    mutable std::mutex mutex_;
    ConnectionLifecycleSnapshot snapshot_;
    RetryPolicy retryPolicy_;
};
} // namespace famalamajam::app::session
