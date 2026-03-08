#include <catch2/catch_test_macros.hpp>

#include "app/session/SessionSettings.h"

using namespace famalamajam::app::session;

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
    CHECK(active.defaultChannelGainDb == Approx(12.0f));
    CHECK(active.defaultChannelPan == Approx(-1.0f));
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
