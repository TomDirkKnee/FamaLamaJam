#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "infra/state/RememberedServerStore.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/FakeServerDiscoveryClient.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::infra::state::makeRememberedServerStore;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::FakeServerDiscoveryClient;

namespace
{
class ScopedTempStoreFile
{
public:
    ScopedTempStoreFile()
        : directory_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getNonexistentChildFile("famalamajam-discovery-store", {}, false))
        , file_(directory_.getChildFile("remembered-private-servers.xml"))
    {
        REQUIRE(directory_.createDirectory());
    }

    ~ScopedTempStoreFile()
    {
        directory_.deleteRecursively();
    }

    [[nodiscard]] const juce::File& file() const noexcept { return file_; }

private:
    juce::File directory_;
    juce::File file_;
};

void completeSuccessfulConnection(FamaLamaJamAudioProcessor& processor,
                                  const std::string& host,
                                  std::uint16_t port,
                                  const std::string& username = "discovery_user",
                                  const std::string& password = {})
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = host;
    settings.serverPort = port;
    settings.username = username;
    settings.password = password;

    REQUIRE(processor.applySettingsFromUi(settings));
    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent { .type = ConnectionEventType::Connected });
    REQUIRE(processor.isSessionConnected());
    REQUIRE(processor.requestDisconnect());
}
} // namespace

TEST_CASE("plugin server discovery remembers only successful private connections", "[plugin_server_discovery]")
{
    ScopedTempStoreFile tempStore;

    {
        FamaLamaJamAudioProcessor processor(makeRememberedServerStore(tempStore.file()));

        auto settings = processor.getActiveSettings();
        settings.serverHost = "draft-only.example.org";
        settings.serverPort = 2049;
        settings.username = "draft_user";
        settings.password = "draft-secret";
        REQUIRE(processor.applySettingsFromUi(settings));

        auto discovery = processor.getServerDiscoveryUiState();
        CHECK(discovery.combinedEntries.empty());
        CHECK_FALSE(discovery.fetchInProgress);
        CHECK_FALSE(discovery.hasStalePublicData);
        CHECK(discovery.statusText.empty());

        settings.serverHost = "failed.example.org";
        settings.serverPort = 2050;
        settings.username = "failed_user";
        settings.password = "wrong-secret";

        REQUIRE(processor.applySettingsFromUi(settings));
        REQUIRE(processor.requestConnect());
        processor.handleConnectionEvent(ConnectionEvent {
            .type = ConnectionEventType::ConnectionFailed,
            .reason = "auth rejected",
        });

        discovery = processor.getServerDiscoveryUiState();
        CHECK(discovery.combinedEntries.empty());
        CHECK_FALSE(discovery.fetchInProgress);
        CHECK_FALSE(discovery.hasStalePublicData);
        CHECK(discovery.statusText.empty());

        settings.serverHost = "successful.example.org";
        settings.serverPort = 2051;
        settings.username = "remembered_user";
        settings.password = "correct-secret";
        REQUIRE(processor.applySettingsFromUi(settings));
        REQUIRE(processor.requestConnect());
        processor.handleConnectionEvent(ConnectionEvent { .type = ConnectionEventType::Connected });

        discovery = processor.getServerDiscoveryUiState();
        REQUIRE(discovery.combinedEntries.size() == 1);
        CHECK(discovery.combinedEntries.front().source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Remembered);
        CHECK(discovery.combinedEntries.front().host == "successful.example.org");
        CHECK(discovery.combinedEntries.front().port == 2051);
        CHECK(discovery.combinedEntries.front().label == "successful.example.org:2051");
        CHECK(discovery.combinedEntries.front().username == "remembered_user");
        CHECK(discovery.combinedEntries.front().password == "correct-secret");
        CHECK(discovery.combinedEntries.front().connectedUsers == -1);
        CHECK_FALSE(discovery.combinedEntries.front().stale);
    }

    FamaLamaJamAudioProcessor restored(makeRememberedServerStore(tempStore.file()));
    const auto restoredDiscovery = restored.getServerDiscoveryUiState();
    REQUIRE(restoredDiscovery.combinedEntries.size() == 1);
    CHECK(restoredDiscovery.combinedEntries.front().host == "successful.example.org");
    CHECK(restoredDiscovery.combinedEntries.front().port == 2051);
    CHECK(restoredDiscovery.combinedEntries.front().username == "remembered_user");
    CHECK(restoredDiscovery.combinedEntries.front().password == "correct-secret");
}

TEST_CASE("plugin server discovery does not persist failed connects across new instances", "[plugin_server_discovery]")
{
    ScopedTempStoreFile tempStore;

    {
        FamaLamaJamAudioProcessor processor(makeRememberedServerStore(tempStore.file()));
        auto settings = processor.getActiveSettings();
        settings.serverHost = "failed.example.org";
        settings.serverPort = 2050;
        settings.username = "failed_user";
        settings.password = "wrong-secret";

        REQUIRE(processor.applySettingsFromUi(settings));
        REQUIRE(processor.requestConnect());
        processor.handleConnectionEvent(ConnectionEvent {
            .type = ConnectionEventType::ConnectionFailed,
            .reason = "auth rejected",
        });
        CHECK(processor.getServerDiscoveryUiState().combinedEntries.empty());
    }

    FamaLamaJamAudioProcessor restored(makeRememberedServerStore(tempStore.file()));
    CHECK(restored.getServerDiscoveryUiState().combinedEntries.empty());
}

TEST_CASE("plugin server discovery keeps remembered history bounded and recent", "[plugin_server_discovery]")
{
    FamaLamaJamAudioProcessor processor;

    for (int index = 0; index < 13; ++index)
    {
        completeSuccessfulConnection(processor,
                                     "jam" + std::to_string(index) + ".example.org",
                                     static_cast<std::uint16_t>(2050 + index));
    }

    auto discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 12);
    CHECK(discovery.combinedEntries.front().host == "jam12.example.org");
    CHECK(discovery.combinedEntries.front().port == 2062);
    CHECK(discovery.combinedEntries.back().host == "jam1.example.org");
    CHECK(discovery.combinedEntries.back().port == 2051);

    completeSuccessfulConnection(processor, "jam5.example.org", 2055);

    discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 12);
    CHECK(discovery.combinedEntries.front().host == "jam5.example.org");
    CHECK(discovery.combinedEntries.front().port == 2055);

    int duplicateCount = 0;
    for (const auto& entry : discovery.combinedEntries)
    {
        if (entry.host == "jam5.example.org" && entry.port == 2055)
            ++duplicateCount;
    }

    CHECK(duplicateCount == 1);
}

TEST_CASE("plugin server discovery deduplicates remembered private entries by endpoint and keeps newest credentials",
          "[plugin_server_discovery]")
{
    FamaLamaJamAudioProcessor processor;

    completeSuccessfulConnection(processor, "private.example.org", 2050, "old_user", "old-secret");
    completeSuccessfulConnection(processor, "private.example.org", 2050, "new_user", "new-secret");

    const auto discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 1);
    CHECK(discovery.combinedEntries.front().host == "private.example.org");
    CHECK(discovery.combinedEntries.front().port == 2050);
    CHECK(discovery.combinedEntries.front().username == "new_user");
    CHECK(discovery.combinedEntries.front().password == "new-secret");
}

TEST_CASE("plugin server discovery combines remembered and public entries without duplicating endpoints",
          "[plugin_server_discovery]")
{
    FamaLamaJamAudioProcessor processor;
    auto client = std::make_unique<FakeServerDiscoveryClient>();
    client->enqueueResult({
        .succeeded = true,
        .payloadText = R"({
            "servers": [
                {
                    "name": "recent.example.org:2050",
                    "user_max": "8",
                    "users": [{ "name": "already-remembered" }],
                    "bpm": "120",
                    "bpi": "16"
                },
                {
                    "name": "quiet.example.org:2051",
                    "user_max": "8",
                    "users": [{ "name": "solo" }],
                    "bpm": "100",
                    "bpi": "16"
                },
                {
                    "name": "zeta.example.org:2052",
                    "user_max": "8",
                    "users": [{ "name": "amy" }, { "name": "bob" }],
                    "bpm": "120",
                    "bpi": "16"
                },
                {
                    "name": "alpha.example.org:2053",
                    "user_max": "8",
                    "users": [{ "name": "amy" }, { "name": "bob" }],
                    "bpm": "120",
                    "bpi": "16"
                },
                {
                    "name": "busy.example.org:2054",
                    "user_max": "10",
                    "users": [{ "name": "amy" }, { "name": "bob" }, { "name": "carol" }, { "name": "ninbot_" }],
                    "bpm": "135",
                    "bpi": "16"
                }
            ]
        })",
    });
    processor.setPublicServerDiscoveryClientForTesting(std::move(client));

    completeSuccessfulConnection(processor, "recent.example.org", 2050);
    completeSuccessfulConnection(processor, "remembered.example.org", 2060);

    REQUIRE(processor.requestPublicServerDiscoveryRefreshForTesting(true));
    const auto discovery = processor.getServerDiscoveryUiState();

    REQUIRE(discovery.combinedEntries.size() == 6);
    CHECK_FALSE(discovery.fetchInProgress);
    CHECK_FALSE(discovery.hasStalePublicData);
    CHECK(discovery.statusText.empty());

    CHECK(discovery.combinedEntries[0].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Remembered);
    CHECK(discovery.combinedEntries[0].host == "remembered.example.org");
    CHECK(discovery.combinedEntries[0].port == 2060);

    CHECK(discovery.combinedEntries[1].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Remembered);
    CHECK(discovery.combinedEntries[1].host == "recent.example.org");
    CHECK(discovery.combinedEntries[1].port == 2050);

    CHECK(discovery.combinedEntries[2].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[2].host == "busy.example.org");
    CHECK(discovery.combinedEntries[2].port == 2054);
    CHECK(discovery.combinedEntries[2].connectedUsers == 3);
    CHECK(discovery.combinedEntries[2].label == "busy.example.org:2054 - 135 BPM / 16 BPI (3/10 users)");

    CHECK(discovery.combinedEntries[3].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[3].host == "alpha.example.org");
    CHECK(discovery.combinedEntries[3].port == 2053);
    CHECK(discovery.combinedEntries[3].connectedUsers == 2);

    CHECK(discovery.combinedEntries[4].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[4].host == "zeta.example.org");
    CHECK(discovery.combinedEntries[4].port == 2052);
    CHECK(discovery.combinedEntries[4].connectedUsers == 2);

    CHECK(discovery.combinedEntries[5].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[5].host == "quiet.example.org");
    CHECK(discovery.combinedEntries[5].port == 2051);
    CHECK(discovery.combinedEntries[5].connectedUsers == 1);

    for (const auto& entry : discovery.combinedEntries)
    {
        const bool isDuplicatedPublicRecent = entry.host == "recent.example.org"
            && entry.port == 2050
            && entry.source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public;
        CHECK_FALSE(isDuplicatedPublicRecent);
    }
}

TEST_CASE("plugin server discovery keeps remembered servers ahead of sorted public rooms",
          "[plugin_server_discovery]")
{
    FamaLamaJamAudioProcessor processor;
    auto client = std::make_unique<FakeServerDiscoveryClient>();
    client->enqueueResult({
        .succeeded = true,
        .payloadText = R"({
            "servers": [
                {
                    "name": "late.example.org:2059",
                    "user_max": "8",
                    "users": [{ "name": "solo" }],
                    "bpm": "100",
                    "bpi": "16"
                },
                {
                    "name": "busy.example.org:2054",
                    "user_max": "10",
                    "users": [{ "name": "amy" }, { "name": "bob" }, { "name": "carol" }],
                    "bpm": "135",
                    "bpi": "16"
                }
            ]
        })",
    });
    processor.setPublicServerDiscoveryClientForTesting(std::move(client));

    completeSuccessfulConnection(processor, "remembered.example.org", 2060);
    REQUIRE(processor.requestPublicServerDiscoveryRefreshForTesting(true));

    const auto discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 3);

    CHECK(discovery.combinedEntries[0].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Remembered);
    CHECK(discovery.combinedEntries[0].host == "remembered.example.org");

    CHECK(discovery.combinedEntries[1].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[1].host == "busy.example.org");

    CHECK(discovery.combinedEntries[2].source == FamaLamaJamAudioProcessor::ServerDiscoveryEntry::Source::Public);
    CHECK(discovery.combinedEntries[2].host == "late.example.org");
}

TEST_CASE("plugin server discovery keeps stale cached public entries visible after refresh failure",
          "[plugin_server_discovery]")
{
    FamaLamaJamAudioProcessor processor;
    auto client = std::make_unique<FakeServerDiscoveryClient>();
    client->enqueueResult({
        .succeeded = true,
        .payloadText = "SERVER \"public.example.org:2051\" \"Public Groove\" \"12/20 users\"\nEND\n",
    });
    client->enqueueResult({
        .succeeded = false,
        .errorText = "Couldn't refresh the public server list.",
    });
    processor.setPublicServerDiscoveryClientForTesting(std::move(client));

    REQUIRE(processor.requestPublicServerDiscoveryRefreshForTesting(false));
    auto discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 1);
    CHECK_FALSE(discovery.hasStalePublicData);

    REQUIRE(processor.requestPublicServerDiscoveryRefreshForTesting(true));
    discovery = processor.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 1);
    CHECK(discovery.hasStalePublicData);
    CHECK(discovery.combinedEntries[0].stale);
    CHECK(discovery.statusText == "Couldn't refresh the public server list. Showing cached servers.");
}
