#include "RetryPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace famalamajam::app::session
{
int RetryPolicy::nextDelay(std::size_t attemptNumber) const noexcept
{
    if (attemptNumber == 0)
        return 0;

    const auto clampedInitial = std::max(0, initialDelayMs);
    const auto clampedMax = std::max(clampedInitial, maxDelayMs);
    const auto clampedMultiplier = std::max(1.0, multiplier);

    const auto power = static_cast<double>(attemptNumber - 1);
    const auto scaledDelay = static_cast<double>(clampedInitial) * std::pow(clampedMultiplier, power);
    const auto cappedDelay = std::min(scaledDelay, static_cast<double>(clampedMax));

    if (cappedDelay >= static_cast<double>(std::numeric_limits<int>::max()))
        return std::numeric_limits<int>::max();

    return static_cast<int>(cappedDelay);
}

bool RetryPolicy::hasAttemptsRemaining(std::size_t attemptsUsed) const noexcept
{
    return attemptsUsed < maxAttempts;
}
} // namespace famalamajam::app::session
