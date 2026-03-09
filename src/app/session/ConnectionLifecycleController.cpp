#include "ConnectionLifecycleController.h"

namespace famalamajam::app::session
{
ConnectionLifecycleController::ConnectionLifecycleController()
    : snapshot_ {}
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
        snapshot_ = transition.snapshot;

    return transition;
}

ConnectionLifecycleTransition ConnectionLifecycleController::handleEvent(const ConnectionEvent& event)
{
    const std::scoped_lock lock(mutex_);
    auto transition = reduceLifecycleEvent(snapshot_, event);

    if (transition.changed)
        snapshot_ = transition.snapshot;

    return transition;
}

ConnectionLifecycleTransition ConnectionLifecycleController::resetToIdle()
{
    const std::scoped_lock lock(mutex_);
    auto transition = reduceLifecycleCommand(snapshot_, ConnectionCommand::Disconnect);

    if (! transition.changed)
    {
        transition.snapshot = snapshot_;
        transition.suppressed = true;
        transition.effect = LifecycleEffect::None;
        return transition;
    }

    snapshot_ = transition.snapshot;
    return transition;
}
} // namespace famalamajam::app::session
