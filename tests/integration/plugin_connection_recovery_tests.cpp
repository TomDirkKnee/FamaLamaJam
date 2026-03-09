#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;

TEST_CASE("plugin_connection_recovery transient failure schedules reconnect and recovers", "[plugin_connection_recovery]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});
    REQUIRE(processor.getLifecycleSnapshot().state == ConnectionState::Active);

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "socket timeout",
    });

    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Reconnecting);
    CHECK(snapshot.retryAttempt == 1);
    CHECK(snapshot.retryAttemptLimit == 6);
    CHECK(snapshot.nextRetryDelayMs > 0);
    CHECK(processor.getScheduledReconnectDelayMs() == snapshot.nextRetryDelayMs);
    CHECK(snapshot.statusMessage.find("attempt 1/6") != std::string::npos);

    REQUIRE(processor.triggerScheduledReconnectForTesting());
    snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Connecting);

    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Active);
    CHECK(snapshot.retryAttempt == 0);
    CHECK(snapshot.nextRetryDelayMs == 0);
}

TEST_CASE("plugin_connection_recovery retries exhaust to error until manual connect reset", "[plugin_connection_recovery]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {.type = ConnectionEventType::Connected});

    for (int attempt = 1; attempt <= 6; ++attempt)
    {
        processor.handleConnectionEvent(ConnectionEvent {
            .type = ConnectionEventType::ConnectionLostRetryable,
            .reason = "network timeout",
        });

        const auto snapshot = processor.getLifecycleSnapshot();
        REQUIRE(snapshot.state == ConnectionState::Reconnecting);
        CHECK(snapshot.retryAttempt == static_cast<std::size_t>(attempt));
        CHECK(snapshot.nextRetryDelayMs > 0);
    }

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "network timeout",
    });

    auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Error);
    CHECK(snapshot.retryAttempt == 6);
    CHECK(snapshot.nextRetryDelayMs == 0);
    CHECK(snapshot.statusMessage.find("retries exhausted") != std::string::npos);
    CHECK_FALSE(processor.triggerScheduledReconnectForTesting());

    REQUIRE(processor.requestConnect());
    snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Connecting);
    CHECK(snapshot.retryAttempt == 0);
    CHECK(snapshot.nextRetryDelayMs == 0);
}

TEST_CASE("plugin_connection_recovery non-retryable failure transitions to error without scheduling", "[plugin_connection_recovery]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionFailed,
        .reason = "auth rejected",
    });

    const auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Error);
    CHECK(snapshot.retryAttempt == 0);
    CHECK(snapshot.nextRetryDelayMs == 0);
    CHECK(processor.getScheduledReconnectDelayMs() == 0);
    CHECK(snapshot.statusMessage.find("credentials") != std::string::npos);
    CHECK_FALSE(processor.triggerScheduledReconnectForTesting());
}
