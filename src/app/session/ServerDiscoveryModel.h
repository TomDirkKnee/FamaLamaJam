#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace famalamajam::app::session
{
struct RememberedServerEntry
{
    std::string host;
    std::uint16_t port { 0 };
    std::string username;
    std::string password;

    bool operator==(const RememberedServerEntry& other) const = default;
};

struct ParsedPublicServerEntry
{
    std::string host;
    std::uint16_t port { 0 };
    std::string infoText;
    std::string usersText;
    int connectedUsers { -1 };
    int maxUsers { -1 };

    bool operator==(const ParsedPublicServerEntry& other) const = default;
};

[[nodiscard]] std::string normalizeDiscoveryHost(std::string host);
[[nodiscard]] std::string makeDiscoveryEndpointKey(std::string_view host, std::uint16_t port);
void rememberSuccessfulServer(std::vector<RememberedServerEntry>& history,
                              RememberedServerEntry entry,
                              std::size_t maxEntries = 12);
[[nodiscard]] std::vector<ParsedPublicServerEntry> parsePublicServerList(std::string_view payload);
} // namespace famalamajam::app::session
