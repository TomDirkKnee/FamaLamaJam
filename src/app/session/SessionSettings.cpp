#include "SessionSettings.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace famalamajam::app::session
{
namespace
{
constexpr std::size_t kMaxHostLength = 253;
constexpr std::size_t kMaxUsernameLength = 64;
constexpr float kMinChannelGainDb = -60.0f;
constexpr float kMaxChannelGainDb = 12.0f;
constexpr float kMinChannelPan = -1.0f;
constexpr float kMaxChannelPan = 1.0f;

[[nodiscard]] std::string trim(const std::string& input)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    auto start = std::find_if_not(input.begin(), input.end(), isSpace);
    auto end = std::find_if_not(input.rbegin(), input.rend(), isSpace).base();

    if (start >= end)
        return {};

    return std::string(start, end);
}
} // namespace

SessionSettings makeDefaultSessionSettings()
{
    return SessionSettings {
        .serverHost = "ninjam.com",
        .serverPort = 2049,
        .username = "guest",
        .defaultChannelGainDb = 0.0f,
        .defaultChannelPan = 0.0f,
        .defaultChannelMuted = false,
    };
}

SessionSettings normalizeSessionSettings(const SessionSettings& candidate)
{
    SessionSettings normalized = candidate;
    normalized.serverHost = trim(candidate.serverHost);
    normalized.username = trim(candidate.username);
    normalized.defaultChannelGainDb = std::clamp(candidate.defaultChannelGainDb, kMinChannelGainDb, kMaxChannelGainDb);
    normalized.defaultChannelPan = std::clamp(candidate.defaultChannelPan, kMinChannelPan, kMaxChannelPan);
    return normalized;
}

SessionSettingsValidationResult validateSessionSettings(const SessionSettings& candidate)
{
    SessionSettingsValidationResult result {
        .isValid = true,
        .errors = {},
    };

    if (candidate.serverHost.empty())
        result.errors.emplace_back("Server host is required.");

    if (candidate.serverHost.size() > kMaxHostLength)
        result.errors.emplace_back("Server host exceeds maximum length.");

    if (candidate.serverPort == 0 || candidate.serverPort > std::numeric_limits<std::uint16_t>::max())
        result.errors.emplace_back("Server port is out of range.");

    if (candidate.username.empty())
        result.errors.emplace_back("Username is required.");

    if (candidate.username.size() > kMaxUsernameLength)
        result.errors.emplace_back("Username exceeds maximum length.");

    result.isValid = result.errors.empty();
    return result;
}

SessionSettingsStore::SessionSettingsStore()
    : activeSettings_(makeDefaultSessionSettings())
{
}

const SessionSettings& SessionSettingsStore::getActiveSettings() const noexcept
{
    return activeSettings_;
}

bool SessionSettingsStore::applyCandidate(SessionSettings candidate, SessionSettingsValidationResult* result)
{
    const SessionSettings normalized = normalizeSessionSettings(candidate);
    const SessionSettingsValidationResult validation = validateSessionSettings(normalized);

    if (result != nullptr)
        *result = validation;

    if (! validation.isValid)
        return false;

    activeSettings_ = normalized;
    return true;
}
} // namespace famalamajam::app::session
