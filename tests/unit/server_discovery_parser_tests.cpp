#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include "app/session/ServerDiscoveryModel.h"

using famalamajam::app::session::ParsedPublicServerEntry;
using famalamajam::app::session::RememberedServerEntry;
using famalamajam::app::session::makeDiscoveryEndpointKey;
using famalamajam::app::session::normalizeDiscoveryHost;
using famalamajam::app::session::parsePublicServerList;
using famalamajam::app::session::rememberSuccessfulServer;

namespace
{
template <typename T, typename = void>
struct HasMaxUsersMember : std::false_type
{
};

template <typename T>
struct HasMaxUsersMember<T, std::void_t<decltype(std::declval<const T&>().maxUsers)>> : std::true_type
{
};

template <typename T>
int getMaxUsersOrSentinel(const T& entry)
{
    if constexpr (HasMaxUsersMember<T>::value)
        return entry.maxUsers;

    return -1;
}
} // namespace

TEST_CASE("server discovery parser extracts classic server rows and connected user counts",
          "[server_discovery_parser]")
{
    const auto entries = parsePublicServerList(
        "SERVER \"NinBot.com:2049\" \"120 BPM / 16 BPI\" \"5 users\"\n"
        "END\n");

    REQUIRE(entries.size() == 1);
    CHECK(entries.front().host == "ninbot.com");
    CHECK(entries.front().port == 2049);
    CHECK(entries.front().infoText == "120 BPM / 16 BPI");
    CHECK(entries.front().usersText == "5 users");
    CHECK(entries.front().connectedUsers == 5);
}

TEST_CASE("server discovery parser tolerates malformed rows and lobby style user text",
          "[server_discovery_parser]")
{
    const auto entries = parsePublicServerList(
        "SERVER \"missing-port\" \"ignored\" \"ignored\"\n"
        "SERVER \"jam.example.org:2050\" \"Lobby / open session\" \"12/20 users\"\n"
        "END\n"
        "SERVER \"after-end.example.org:2051\" \"Should not parse\" \"3 users\"\n");

    REQUIRE(entries.size() == 1);
    CHECK(entries.front().host == "jam.example.org");
    CHECK(entries.front().port == 2050);
    CHECK(entries.front().infoText == "Lobby / open session");
    CHECK(entries.front().usersText == "12/20 users");
    CHECK(entries.front().connectedUsers == 12);
}

TEST_CASE("server discovery parser prefers structured room counts and sorts busiest rooms first",
          "[server_discovery_parser]")
{
    const auto entries = parsePublicServerList(R"({
        "servers": [
            {
                "name": "zeta.example.org:2053",
                "user_max": "8",
                "users": [{ "name": "amy" }, { "name": "bob" }],
                "bpm": "120",
                "bpi": "16"
            },
            {
                "name": "busy.example.org:2052",
                "user_max": "10",
                "users": [{ "name": "amy" }, { "name": "bob" }, { "name": "carol" }, { "name": "ninbot" }],
                "bpm": "135",
                "bpi": "16"
            },
            {
                "name": "alpha.example.org:2050",
                "user_max": "8",
                "users": [{ "name": "amy" }, { "name": "bob" }],
                "bpm": "120",
                "bpi": "16"
            },
            {
                "name": "quiet.example.org:2051",
                "user_max": "6",
                "users": [{ "name": "solo" }],
                "bpm": "90",
                "bpi": "16"
            }
        ]
    })");

    REQUIRE(HasMaxUsersMember<ParsedPublicServerEntry>::value);
    REQUIRE(entries.size() == 4);

    CHECK(entries[0].host == "busy.example.org");
    CHECK(entries[0].port == 2052);
    CHECK(entries[0].connectedUsers == 3);
    CHECK(getMaxUsersOrSentinel(entries[0]) == 10);

    CHECK(entries[1].host == "alpha.example.org");
    CHECK(entries[1].connectedUsers == 2);
    CHECK(getMaxUsersOrSentinel(entries[1]) == 8);

    CHECK(entries[2].host == "zeta.example.org");
    CHECK(entries[2].connectedUsers == 2);
    CHECK(getMaxUsersOrSentinel(entries[2]) == 8);

    CHECK(entries[3].host == "quiet.example.org");
    CHECK(entries[3].connectedUsers == 1);
    CHECK(getMaxUsersOrSentinel(entries[3]) == 6);
}

TEST_CASE("server discovery parser extracts capacity from legacy user text when structured data is absent",
          "[server_discovery_parser]")
{
    const auto entries = parsePublicServerList(
        "SERVER \"jam.example.org:2050\" \"Lobby / open session\" \"12/20 users\"\n"
        "END\n");

    REQUIRE(HasMaxUsersMember<ParsedPublicServerEntry>::value);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().connectedUsers == 12);
    CHECK(getMaxUsersOrSentinel(entries.front()) == 20);
}

TEST_CASE("server discovery history helper normalizes hosts and keeps a deduped recent list",
          "[server_discovery_parser]")
{
    std::vector<RememberedServerEntry> history;

    rememberSuccessfulServer(history, { .host = "  Private.Example.Org  ", .port = 2050 }, 3);
    rememberSuccessfulServer(history, { .host = "jam.example.org", .port = 2051 }, 3);
    rememberSuccessfulServer(history, { .host = "PRIVATE.EXAMPLE.ORG", .port = 2050 }, 3);
    rememberSuccessfulServer(history, { .host = "third.example.org", .port = 2052 }, 3);
    rememberSuccessfulServer(history, { .host = "fourth.example.org", .port = 2053 }, 3);

    REQUIRE(history.size() == 3);
    CHECK(history[0] == RememberedServerEntry { .host = "fourth.example.org", .port = 2053 });
    CHECK(history[1] == RememberedServerEntry { .host = "third.example.org", .port = 2052 });
    CHECK(history[2] == RememberedServerEntry { .host = "private.example.org", .port = 2050 });

    CHECK(normalizeDiscoveryHost("  Mixed.Case.Host ") == "mixed.case.host");
    CHECK(makeDiscoveryEndpointKey("Mixed.Case.Host", 2050) == "mixed.case.host:2050");
}
