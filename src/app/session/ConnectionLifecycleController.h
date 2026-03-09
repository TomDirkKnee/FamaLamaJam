#pragma once

#include <mutex>

#include "ConnectionLifecycle.h"

namespace famalamajam::app::session
{
class ConnectionLifecycleController
{
public:
    ConnectionLifecycleController();

    [[nodiscard]] ConnectionLifecycleSnapshot getSnapshot() const;
    [[nodiscard]] ConnectionLifecycleTransition handleCommand(ConnectionCommand command);
    [[nodiscard]] ConnectionLifecycleTransition handleEvent(const ConnectionEvent& event);

    ConnectionLifecycleTransition resetToIdle();

private:
    mutable std::mutex mutex_;
    ConnectionLifecycleSnapshot snapshot_;
};
} // namespace famalamajam::app::session
