#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include "app/session/SessionSettings.h"

using namespace famalamajam::app::session;

namespace
{
template <typename T, typename = void>
constexpr bool hasPasswordField = false;

template <typename T>
constexpr bool hasPasswordField<T, std::void_t<decltype(std::declval<T&>().password)>> = true;

template <typename T, std::enable_if_t<hasPasswordField<T>, int> = 0>
void setPasswordImpl(T& settings, std::string password)
{
    settings.password = std::move(password);
}

template <typename T, std::enable_if_t<! hasPasswordField<T>, int> = 0>
void setPasswordImpl(T&, std::string)
{
}

void setPassword(SessionSettings& settings, std::string password)
{
    setPasswordImpl(settings, std::move(password));
}

template <typename T, std::enable_if_t<hasPasswordField<T>, int> = 0>
std::string readPasswordImpl(const T& settings)
{
    return settings.password;
}

template <typename T, std::enable_if_t<! hasPasswordField<T>, int> = 0>
std::string readPasswordImpl(const T&)
{
    return {};
}

std::string readPassword(const SessionSettings& settings)
{
    return readPasswordImpl(settings);
}
} // namespace

TEST_CASE("settings validation accepts normalized valid values", "[settings_validation]")
{
    SessionSettingsStore store;
    SessionSettingsValidationResult result;

    const SessionSettings candidate {
        .serverHost = "  ninjam.example.org  ",
        .serverPort = 2049,
        .username = "  tester  ",
        .defaultChannelGainDb = 24.0f,
        .defaultChannelPan = -4.0f,
        .defaultChannelMuted = false,
    };

    REQUIRE(store.applyCandidate(candidate, &result));
    REQUIRE(result.isValid);

    const auto& active = store.getActiveSettings();
    CHECK(active.serverHost == "ninjam.example.org");
    CHECK(active.serverPort == 2049);
    CHECK(active.username == "tester");
    CHECK(active.defaultChannelGainDb == Catch::Approx(12.0f));
    CHECK(active.defaultChannelPan == Catch::Approx(-1.0f));
}

TEST_CASE("settings validation rejects invalid endpoint and username", "[settings_validation]")
{
    SessionSettingsStore store;
    const SessionSettings original = store.getActiveSettings();

    SessionSettingsValidationResult result;
    const SessionSettings candidate {
        .serverHost = "   ",
        .serverPort = 2049,
        .username = "   ",
        .defaultChannelGainDb = 0.0f,
        .defaultChannelPan = 0.0f,
        .defaultChannelMuted = false,
    };

    REQUIRE_FALSE(store.applyCandidate(candidate, &result));
    REQUIRE_FALSE(result.isValid);
    CHECK(result.errors.size() >= 2);
    CHECK(store.getActiveSettings() == original);
}

TEST_CASE("settings validation rejects invalid port", "[settings_validation]")
{
    SessionSettingsStore store;
    const SessionSettings original = store.getActiveSettings();

    SessionSettingsValidationResult result;
    SessionSettings candidate = original;
    candidate.serverPort = 0;

    REQUIRE_FALSE(store.applyCandidate(candidate, &result));
    REQUIRE_FALSE(result.isValid);
    CHECK(result.errors.size() == 1);
    CHECK(store.getActiveSettings() == original);
}

TEST_CASE("settings validation keeps password visible while preserving blank-public-room behavior",
          "[settings_validation]")
{
    SessionSettingsStore store;
    SessionSettingsValidationResult result;

    SessionSettings privateRoomCandidate = makeDefaultSessionSettings();
    privateRoomCandidate.serverHost = " private.example.org ";
    privateRoomCandidate.username = "  private_user  ";
    setPassword(privateRoomCandidate, "  room secret  ");

    REQUIRE(store.applyCandidate(privateRoomCandidate, &result));
    REQUIRE(result.isValid);

    const auto privateRoomSettings = store.getActiveSettings();
    CHECK(hasPasswordField<SessionSettings>);
    CHECK(privateRoomSettings.serverHost == "private.example.org");
    CHECK(privateRoomSettings.username == "private_user");
    CHECK(readPassword(privateRoomSettings) == "  room secret  ");

    SessionSettings publicRoomCandidate = privateRoomSettings;
    setPassword(publicRoomCandidate, {});

    REQUIRE(store.applyCandidate(publicRoomCandidate, &result));
    REQUIRE(result.isValid);
    CHECK(readPassword(store.getActiveSettings()).empty());
}


