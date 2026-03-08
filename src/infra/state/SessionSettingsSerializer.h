#pragma once

#include <JuceHeader.h>

#include "app/session/SessionSettings.h"

namespace famalamajam::infra::state
{
class SessionSettingsSerializer
{
public:
    static constexpr int schemaVersion = 1;

    static void serialize(const app::session::SessionSettings& settings, juce::MemoryBlock& destination);

    static app::session::SessionSettings deserializeOrDefault(const void* data,
                                                              int sizeInBytes,
                                                              bool* usedFallback = nullptr);

private:
    static juce::ValueTree toValueTree(const app::session::SessionSettings& settings);
};
} // namespace famalamajam::infra::state
