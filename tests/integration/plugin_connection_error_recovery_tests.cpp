#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;

TEST_CASE("plugin_connection_error_recovery corrected apply exits error without implicit reconnect", "[plugin_connection_error_recovery]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionFailed,
        .reason = "auth rejected",
    });

    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Error);

    auto corrected = processor.getActiveSettings();
    corrected.username = "fixed-user";
    corrected.serverHost = "recover.ninjam.example.org";

    famalamajam::app::session::SessionSettingsValidationResult validation;
    REQUIRE(processor.applySettingsFromUi(corrected, &validation));
    REQUIRE(validation.isValid);

    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Idle);
    CHECK(snapshot.statusMessage == "Ready. Press Connect to retry.");
    CHECK(snapshot.retryAttempt == 0);
    CHECK(snapshot.nextRetryDelayMs == 0);
    CHECK(snapshot.lastError.empty());
    CHECK(snapshot.canConnect());
    CHECK_FALSE(snapshot.canDisconnect());
    CHECK_FALSE(processor.isSessionConnected());
    CHECK(processor.getScheduledReconnectDelayMs() == 0);
    CHECK_FALSE(processor.triggerScheduledReconnectForTesting());

    REQUIRE(processor.requestConnect());
    CHECK(processor.getLifecycleSnapshot().state == ConnectionState::Connecting);
}

TEST_CASE("plugin_connection_error_recovery corrected apply clears exhausted retry loop state", "[plugin_connection_error_recovery]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});

    for (int attempt = 1; attempt <= 7; ++attempt)
    {
        processor.handleConnectionEvent(ConnectionEvent {
            .type = ConnectionEventType::ConnectionLostRetryable,
            .reason = "network timeout",
        });
    }

    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Error);
    CHECK(snapshot.retryAttempt == 6);

    auto corrected = processor.getActiveSettings();
    corrected.serverPort = 2051;

    famalamajam::app::session::SessionSettingsValidationResult validation;
    REQUIRE(processor.applySettingsFromUi(corrected, &validation));
    REQUIRE(validation.isValid);

    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Idle);
    CHECK(snapshot.retryAttempt == 0);
    CHECK(snapshot.nextRetryDelayMs == 0);
    CHECK(snapshot.statusMessage == "Ready. Press Connect to retry.");
    CHECK_FALSE(processor.triggerScheduledReconnectForTesting());
}
