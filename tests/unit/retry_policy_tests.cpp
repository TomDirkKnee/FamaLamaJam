#include <catch2/catch_test_macros.hpp>

#include "app/session/RetryPolicy.h"

using famalamajam::app::session::RetryPolicy;

TEST_CASE("retry_policy computes exponential delay progression", "[retry_policy]")
{
    RetryPolicy policy;

    CHECK(policy.nextDelay(0) == 0);
    CHECK(policy.nextDelay(1) == 1000);
    CHECK(policy.nextDelay(2) == 2000);
    CHECK(policy.nextDelay(3) == 4000);
}

TEST_CASE("retry_policy enforces delay cap", "[retry_policy]")
{
    RetryPolicy policy;
    policy.initialDelayMs = 750;
    policy.multiplier = 2.0;
    policy.maxDelayMs = 2500;

    CHECK(policy.nextDelay(1) == 750);
    CHECK(policy.nextDelay(2) == 1500);
    CHECK(policy.nextDelay(3) == 2500);
    CHECK(policy.nextDelay(4) == 2500);
}

TEST_CASE("retry_policy tracks attempt exhaustion", "[retry_policy]")
{
    RetryPolicy policy;
    policy.maxAttempts = 3;

    CHECK(policy.hasAttemptsRemaining(0));
    CHECK(policy.hasAttemptsRemaining(1));
    CHECK(policy.hasAttemptsRemaining(2));
    CHECK_FALSE(policy.hasAttemptsRemaining(3));
}
