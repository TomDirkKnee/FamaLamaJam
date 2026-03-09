#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionFailureClassifier.h"

using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionFailureKind;
using famalamajam::app::session::classifyFailure;
using famalamajam::app::session::isRetryableFailure;

TEST_CASE("connection_failure_classification marks retryable transport failures", "[connection_failure_classification]")
{
    CHECK(classifyFailure(ConnectionEventType::ConnectionLostRetryable, "socket timeout")
          == ConnectionFailureKind::RetryableTransport);
    CHECK(classifyFailure(ConnectionEventType::ConnectionFailed, "network timeout")
          == ConnectionFailureKind::RetryableTransport);
    CHECK(isRetryableFailure(classifyFailure(ConnectionEventType::ConnectionFailed, "transport unreachable")));
}

TEST_CASE("connection_failure_classification marks non-retryable auth config and protocol failures", "[connection_failure_classification]")
{
    CHECK(classifyFailure(ConnectionEventType::ConnectionFailed, "auth rejected")
          == ConnectionFailureKind::NonRetryableAuthentication);
    CHECK(classifyFailure(ConnectionEventType::ConnectionFailed, "invalid host configuration")
          == ConnectionFailureKind::NonRetryableConfiguration);
    CHECK(classifyFailure(ConnectionEventType::ConnectionFailed, "protocol mismatch")
          == ConnectionFailureKind::NonRetryableProtocol);

    CHECK_FALSE(isRetryableFailure(classifyFailure(ConnectionEventType::ConnectionFailed, "auth rejected")));
    CHECK_FALSE(isRetryableFailure(classifyFailure(ConnectionEventType::ConnectionFailed, "protocol mismatch")));
}

TEST_CASE("connection_failure_classification defaults unknown failures to non-retryable", "[connection_failure_classification]")
{
    CHECK(classifyFailure(ConnectionEventType::ConnectionFailed, "mystery failure")
          == ConnectionFailureKind::NonRetryableUnknown);
    CHECK_FALSE(isRetryableFailure(classifyFailure(ConnectionEventType::ConnectionFailed, "mystery failure")));
}
