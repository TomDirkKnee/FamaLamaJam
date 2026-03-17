#pragma once

#include <deque>
#include <utility>
#include <vector>

#include "infra/net/PublicServerDiscoveryClient.h"

namespace famalamajam::tests::integration
{
class FakeServerDiscoveryClient final : public infra::net::PublicServerDiscoveryClient
{
public:
    void enqueueResult(Result result)
    {
        queuedResults_.push_back(std::move(result));
    }

    [[nodiscard]] bool requestRefresh(bool manual) override
    {
        requestedManualFlags_.push_back(manual);
        if (! queuedResults_.empty())
        {
            ++readyResults_;
            refreshInProgress_ = true;
        }
        else
        {
            refreshInProgress_ = false;
        }

        return true;
    }

    [[nodiscard]] bool popResult(Result& result) override
    {
        if (queuedResults_.empty() || readyResults_ <= 0)
        {
            refreshInProgress_ = false;
            return false;
        }

        result = std::move(queuedResults_.front());
        queuedResults_.pop_front();
        --readyResults_;
        refreshInProgress_ = false;
        return true;
    }

    [[nodiscard]] bool isRefreshInProgress() const override
    {
        return refreshInProgress_;
    }

    [[nodiscard]] int getRequestCount() const noexcept
    {
        return static_cast<int>(requestedManualFlags_.size());
    }

    [[nodiscard]] bool getLastRequestWasManual() const noexcept
    {
        return ! requestedManualFlags_.empty() && requestedManualFlags_.back();
    }

private:
    std::deque<Result> queuedResults_;
    std::vector<bool> requestedManualFlags_;
    int readyResults_ { 0 };
    bool refreshInProgress_ { false };
};
} // namespace famalamajam::tests::integration
