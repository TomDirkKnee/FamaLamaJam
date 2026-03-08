#include "SessionSettingsController.h"

namespace famalamajam::app::session
{
SessionSettingsController::SessionSettingsController(SessionSettingsStore& store)
    : store_(store)
{
}

SessionSettingsController::ApplyResult SessionSettingsController::applyDraft(SessionSettings draft)
{
    SessionSettingsValidationResult validation;
    const bool applied = store_.applyCandidate(draft, &validation);

    ApplyResult result;
    result.applied = applied;
    result.validation = std::move(validation);

    if (applied)
    {
        result.statusMessage = "Settings applied";
        return result;
    }

    if (! result.validation.errors.empty())
    {
        result.statusMessage = result.validation.errors.front();
        return result;
    }

    result.statusMessage = "Invalid settings";
    return result;
}
} // namespace famalamajam::app::session
