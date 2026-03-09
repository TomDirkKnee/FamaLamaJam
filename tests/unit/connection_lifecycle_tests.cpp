#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"

using namespace famalamajam::app::session;

TEST_CASE("connection lifecycle allows connect from idle and suppresses duplicate connect", "[connection_lifecycle]")
{
    const ConnectionLifecycleSnapshot idle;

    const auto connect = reduceLifecycleCommand(idle, ConnectionCommand::Connect);
    REQUIRE(connect.changed);
    CHECK_FALSE(connect.suppressed);
    CHECK(connect.snapshot.state == ConnectionState::Connecting);
    CHECK(connect.effect == LifecycleEffect::BeginConnect);

    const auto duplicate = reduceLifecycleCommand(connect.snapshot, ConnectionCommand::Connect);
    CHECK_FALSE(duplicate.changed);
    CHECK(duplicate.suppressed);
    CHECK(duplicate.snapshot.state == ConnectionState::Connecting);
}

TEST_CASE("connection lifecycle returns to idle when disconnecting while connecting or reconnecting", "[connection_lifecycle]")
{
    ConnectionLifecycleSnapshot connecting;
    connecting.state = ConnectionState::Connecting;
    connecting.statusMessage = "Connecting";

    const auto cancelConnecting = reduceLifecycleCommand(connecting, ConnectionCommand::Disconnect);
    REQUIRE(cancelConnecting.changed);
    CHECK(cancelConnecting.snapshot.state == ConnectionState::Idle);
    CHECK(cancelConnecting.effect == LifecycleEffect::CancelPendingConnect);

    ConnectionLifecycleSnapshot reconnecting;
    reconnecting.state = ConnectionState::Reconnecting;
    reconnecting.statusMessage = "Reconnecting";

    const auto cancelReconnecting = reduceLifecycleCommand(reconnecting, ConnectionCommand::Disconnect);
    REQUIRE(cancelReconnecting.changed);
    CHECK(cancelReconnecting.snapshot.state == ConnectionState::Idle);
    CHECK(cancelReconnecting.effect == LifecycleEffect::CancelPendingConnect);
}

TEST_CASE("connection lifecycle applies deterministic event transitions", "[connection_lifecycle]")
{
    auto state = reduceLifecycleCommand(ConnectionLifecycleSnapshot {}, ConnectionCommand::Connect).snapshot;

    const auto connected = reduceLifecycleEvent(state, ConnectionEvent {.type = ConnectionEventType::Connected});
    REQUIRE(connected.changed);
    CHECK(connected.snapshot.state == ConnectionState::Active);

    const auto reconnecting = reduceLifecycleEvent(connected.snapshot,
                                                   ConnectionEvent {
                                                       .type = ConnectionEventType::ConnectionLostRetryable,
                                                       .reason = "socket timeout",
                                                   });
    REQUIRE(reconnecting.changed);
    CHECK(reconnecting.snapshot.state == ConnectionState::Reconnecting);
    CHECK(reconnecting.snapshot.lastError == "socket timeout");

    const auto failed = reduceLifecycleEvent(reconnecting.snapshot,
                                             ConnectionEvent {
                                                 .type = ConnectionEventType::ConnectionFailed,
                                                 .reason = "auth rejected",
                                             });
    REQUIRE(failed.changed);
    CHECK(failed.snapshot.state == ConnectionState::Error);
    CHECK(failed.snapshot.lastError == "auth rejected");
}
