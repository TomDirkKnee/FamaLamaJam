#pragma once

#include <string>

#include "SessionSettings.h"

namespace famalamajam::app::session
{
class SessionSettingsController
{
public:
    struct ApplyResult
    {
        bool applied { false };
        SessionSettingsValidationResult validation;
        std::string statusMessage;
    };

    explicit SessionSettingsController(SessionSettingsStore& store);

    ApplyResult applyDraft(SessionSettings draft);

private:
    SessionSettingsStore& store_;
};
} // namespace famalamajam::app::session
