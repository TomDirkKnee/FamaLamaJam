#include <juce_core/juce_core.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;

TEST_CASE("plugin_state_roundtrip restores saved settings in fresh instance", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor source;

    famalamajam::app::session::SessionSettings draft {
        .serverHost = "ninjam.example.org",
        .serverPort = 2050,
        .username = "roundtrip_user",
        .defaultChannelGainDb = -4.0f,
        .defaultChannelPan = 0.35f,
        .defaultChannelMuted = true,
    };

    famalamajam::app::session::SessionSettingsValidationResult validation;
    REQUIRE(source.applySettingsFromUi(draft, &validation));

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    const auto active = restored.getActiveSettings();
    CHECK(active.serverHost == "ninjam.example.org");
    CHECK(active.serverPort == 2050);
    CHECK(active.username == "roundtrip_user");
    CHECK(active.defaultChannelGainDb == Catch::Approx(-4.0f));
    CHECK(active.defaultChannelPan == Catch::Approx(0.35f));
    CHECK(active.defaultChannelMuted == true);
    CHECK(restored.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(restored.isSessionConnected());
    CHECK(restored.getLastStatusMessage() == "Settings restored");
}

TEST_CASE("plugin_state_roundtrip restores disconnected startup with brief last-error context", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor source;

    REQUIRE(source.requestConnect());
    source.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionFailed,
        .reason = "auth token expired",
    });
    REQUIRE(source.getLifecycleSnapshot().state == ConnectionState::Error);

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    CHECK(restored.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(restored.isSessionConnected());
    CHECK(restored.getLastStatusMessage().find("Last error: auth token expired") != std::string::npos);

    REQUIRE(restored.requestConnect());
    CHECK(restored.getLastStatusMessage() == "Connecting");
}

TEST_CASE("plugin_state_roundtrip falls back safely for invalid state blob", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor processor;

    const unsigned char invalid[] {0xAA, 0x00, 0x11};
    processor.setStateInformation(invalid, static_cast<int>(sizeof(invalid)));

    const auto active = processor.getActiveSettings();
    const auto defaults = famalamajam::app::session::makeDefaultSessionSettings();

    CHECK(active == defaults);
    CHECK(processor.getLastStatusMessage().find("Defaults restored") != std::string::npos);
    CHECK_FALSE(processor.isSessionConnected());
}
