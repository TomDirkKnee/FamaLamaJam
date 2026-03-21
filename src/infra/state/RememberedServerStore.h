#pragma once

#include <memory>
#include <vector>

#include "app/session/ServerDiscoveryModel.h"

namespace juce
{
class File;
}

namespace famalamajam::infra::state
{
class RememberedServerStore
{
public:
    virtual ~RememberedServerStore() = default;

    [[nodiscard]] virtual std::vector<app::session::RememberedServerEntry> load() const = 0;
    [[nodiscard]] virtual bool save(const std::vector<app::session::RememberedServerEntry>& entries) = 0;
};

[[nodiscard]] std::unique_ptr<RememberedServerStore> makeRememberedServerStore();
[[nodiscard]] std::unique_ptr<RememberedServerStore> makeRememberedServerStore(const juce::File& file);
} // namespace famalamajam::infra::state
