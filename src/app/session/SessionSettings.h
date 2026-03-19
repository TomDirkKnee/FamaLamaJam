#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace famalamajam::app::session
{
struct SessionSettings
{
    std::string serverHost;
    std::uint16_t serverPort;
    std::string username;
    std::string password;
    float defaultChannelGainDb;
    float defaultChannelPan;
    bool defaultChannelMuted;

    bool operator==(const SessionSettings& other) const = default;
};

struct SessionSettingsValidationResult
{
    bool isValid { false };
    std::vector<std::string> errors;
};

SessionSettings makeDefaultSessionSettings();
SessionSettings normalizeSessionSettings(const SessionSettings& candidate);
SessionSettingsValidationResult validateSessionSettings(const SessionSettings& candidate);

class SessionSettingsStore
{
public:
    SessionSettingsStore();

    const SessionSettings& getActiveSettings() const noexcept;
    bool applyCandidate(SessionSettings candidate, SessionSettingsValidationResult* result = nullptr);

private:
    SessionSettings activeSettings_;
};
} // namespace famalamajam::app::session
