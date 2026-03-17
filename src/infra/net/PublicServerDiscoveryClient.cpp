#include "PublicServerDiscoveryClient.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <juce_core/juce_core.h>

namespace famalamajam::infra::net
{
namespace
{
constexpr const char* kStructuredPublicServerListUrl = "http://ninbot.com/app/servers.php";
constexpr const char* kLegacyPublicServerListUrl = "http://ninjam.com/serverlist.php";
constexpr int kDiscoveryTimeoutMs = 5000;

[[nodiscard]] bool tryFetchDiscoveryPayload(const juce::URL& url, PublicServerDiscoveryClient::Result& result)
{
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(kDiscoveryTimeoutMs)
                             .withNumRedirectsToFollow(3);
    auto stream = url.createInputStream(options);

    if (stream == nullptr)
        return false;

    result.payloadText = stream->readEntireStreamAsString().toStdString();
    return ! result.payloadText.empty();
}

class JucePublicServerDiscoveryClient final : public PublicServerDiscoveryClient
{
public:
    JucePublicServerDiscoveryClient() = default;
    ~JucePublicServerDiscoveryClient() override = default;

    [[nodiscard]] bool requestRefresh(bool /*manual*/) override
    {
        bool expected = false;
        if (! sharedState_->refreshInProgress.compare_exchange_strong(expected, true))
            return false;

        std::thread([sharedState = sharedState_]() {
            Result result;

            try
            {
                result.succeeded = tryFetchDiscoveryPayload(juce::URL(kStructuredPublicServerListUrl), result)
                    || tryFetchDiscoveryPayload(juce::URL(kLegacyPublicServerListUrl), result);

                if (! result.succeeded)
                    result.errorText = "Couldn't reach the public server list.";
            }
            catch (...)
            {
                result.errorText = "Public server list refresh failed.";
            }

            if (result.succeeded && result.payloadText.empty())
            {
                result.succeeded = false;
                result.errorText = "Public server list returned no data.";
            }

            {
                const std::scoped_lock lock(sharedState->mutex);
                sharedState->results.push(std::move(result));
            }

            sharedState->refreshInProgress.store(false);
        }).detach();

        return true;
    }

    [[nodiscard]] bool popResult(Result& result) override
    {
        const std::scoped_lock lock(sharedState_->mutex);
        if (sharedState_->results.empty())
            return false;

        result = std::move(sharedState_->results.front());
        sharedState_->results.pop();
        return true;
    }

    [[nodiscard]] bool isRefreshInProgress() const override
    {
        return sharedState_->refreshInProgress.load();
    }

private:
    struct SharedState
    {
        std::mutex mutex;
        std::queue<Result> results;
        std::atomic<bool> refreshInProgress { false };
    };

    std::shared_ptr<SharedState> sharedState_ { std::make_shared<SharedState>() };
};
} // namespace

std::unique_ptr<PublicServerDiscoveryClient> makePublicServerDiscoveryClient()
{
    return std::make_unique<JucePublicServerDiscoveryClient>();
}
} // namespace famalamajam::infra::net
