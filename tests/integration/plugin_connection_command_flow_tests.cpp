#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;

TEST_CASE("plugin connection command flow supports repeated connect disconnect cycles", "[plugin_connection_command_flow]")
{
    FamaLamaJamAudioProcessor processor;

    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Idle);
    CHECK(snapshot.canConnect());
    CHECK_FALSE(snapshot.canDisconnect());

    REQUIRE(processor.requestConnect());
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Connecting);
    CHECK(processor.getLastStatusMessage() == "Connecting");
    CHECK_FALSE(snapshot.canConnect());
    CHECK(snapshot.canDisconnect());

    CHECK_FALSE(processor.requestConnect());
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Connecting);

    REQUIRE(processor.requestDisconnect());
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Idle);
    CHECK(processor.getLastStatusMessage() == "Disconnected");
    CHECK(snapshot.canConnect());
    CHECK_FALSE(snapshot.canDisconnect());

    CHECK_FALSE(processor.requestDisconnect());

    REQUIRE(processor.requestConnect());
    REQUIRE(processor.requestDisconnect());
    CHECK(processor.getLifecycleSnapshot().state == ConnectionState::Idle);
}

TEST_CASE("plugin connection command flow applies lifecycle events deterministically", "[plugin_connection_command_flow]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());

    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});
    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Active);
    CHECK(processor.getLastStatusMessage() == "Connected");
    CHECK(processor.isSessionConnected());

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "socket timeout",
    });

    snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Reconnecting);
    CHECK(snapshot.statusMessage.find("Reconnecting") != std::string::npos);
    CHECK(processor.getLastStatusMessage().find("Reconnecting") != std::string::npos);

    REQUIRE(processor.requestDisconnect());
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Idle);
    CHECK_FALSE(processor.isSessionConnected());
    CHECK(processor.getLastStatusMessage() == "Disconnected");
}

TEST_CASE("plugin connection command flow keeps apply decoupled from immediate reconnect", "[plugin_connection_command_flow]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});
    REQUIRE(processor.getLifecycleSnapshot().state == ConnectionState::Active);

    famalamajam::app::session::SessionSettings settings = processor.getActiveSettings();
    settings.serverHost = "updated.ninjam.example.org";
    settings.username = "updated-user";

    famalamajam::app::session::SessionSettingsValidationResult validation;
    REQUIRE(processor.applySettingsFromUi(settings, &validation));
    CHECK(validation.isValid);
    CHECK(processor.getLifecycleSnapshot().state == ConnectionState::Active);
    CHECK(processor.isSessionConnected());
}
