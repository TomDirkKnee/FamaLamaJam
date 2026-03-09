#pragma once

#include <cstddef>

namespace famalamajam::app::session
{
struct RetryPolicy
{
    int initialDelayMs { 1000 };
    double multiplier { 2.0 };
    int maxDelayMs { 15000 };
    std::size_t maxAttempts { 6 };

    [[nodiscard]] int nextDelay(std::size_t attemptNumber) const noexcept;
    [[nodiscard]] bool hasAttemptsRemaining(std::size_t attemptsUsed) const noexcept;
};
} // namespace famalamajam::app::session
