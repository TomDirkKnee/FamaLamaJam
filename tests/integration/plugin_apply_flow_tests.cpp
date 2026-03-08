#include <catch2/catch_test_macros.hpp>

#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"

using namespace famalamajam::app::session;

TEST_CASE("plugin apply flow accepts valid draft", "[plugin_apply_flow]")
{
    SessionSettingsStore store;
    SessionSettingsController controller(store);

    SessionSettings draft {
        .serverHost = "ninjam.example.org",
        .serverPort = 2049,
        .username = "player",
        .defaultChannelGainDb = -2.0f,
        .defaultChannelPan = 0.2f,
        .defaultChannelMuted = false,
    };

    const auto result = controller.applyDraft(draft);

    REQUIRE(result.applied);
    CHECK(result.validation.isValid);
    CHECK(store.getActiveSettings().serverHost == "ninjam.example.org");
    CHECK(result.statusMessage == "Settings applied");
}

TEST_CASE("plugin apply flow rejects invalid draft and keeps active settings", "[plugin_apply_flow]")
{
    SessionSettingsStore store;
    SessionSettingsController controller(store);
    const auto before = store.getActiveSettings();

    SessionSettings draft = before;
    draft.serverHost = " ";
    draft.username = " ";

    const auto result = controller.applyDraft(draft);

    REQUIRE_FALSE(result.applied);
    CHECK_FALSE(result.validation.isValid);
    CHECK(store.getActiveSettings() == before);
    CHECK_FALSE(result.statusMessage.empty());
}
