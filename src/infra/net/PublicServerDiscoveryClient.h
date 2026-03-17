#pragma once

#include <memory>
#include <string>

namespace famalamajam::infra::net
{
class PublicServerDiscoveryClient
{
public:
    struct Result
    {
        bool succeeded { false };
        std::string payloadText;
        std::string errorText;
        int httpStatusCode { 0 };
    };

    virtual ~PublicServerDiscoveryClient() = default;

    [[nodiscard]] virtual bool requestRefresh(bool manual) = 0;
    [[nodiscard]] virtual bool popResult(Result& result) = 0;
    [[nodiscard]] virtual bool isRefreshInProgress() const = 0;
};

[[nodiscard]] std::unique_ptr<PublicServerDiscoveryClient> makePublicServerDiscoveryClient();
} // namespace famalamajam::infra::net
