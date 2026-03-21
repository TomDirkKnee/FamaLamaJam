#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include "infra/state/RememberedServerStore.h"

using famalamajam::app::session::RememberedServerEntry;
using famalamajam::infra::state::makeRememberedServerStore;

namespace
{
class ScopedTempStoreFile
{
public:
    ScopedTempStoreFile()
        : directory_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getNonexistentChildFile("famalamajam-remembered-server-store", {}, false))
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
} // namespace

TEST_CASE("remembered server store saves stable XML and reloads across instances", "[remembered_server_store]")
{
    ScopedTempStoreFile tempStore;
    auto store = makeRememberedServerStore(tempStore.file());

    const std::vector<RememberedServerEntry> entries {
        { .host = "Recent.Example.org", .port = 2050, .username = "recent_user", .password = "recent-secret" },
        { .host = "older.example.org", .port = 2051, .username = "older_user", .password = "older-secret" },
    };

    REQUIRE(store->save(entries));
    REQUIRE(tempStore.file().existsAsFile());

    const auto firstXml = tempStore.file().loadFileAsString().replace("\r\n", "\n");
    CHECK(firstXml.contains("<famalamajam.rememberedServers"));
    CHECK(firstXml.contains("schemaVersion=\"1\""));
    CHECK(firstXml.contains("host=\"recent.example.org\""));
    CHECK(firstXml.contains("username=\"recent_user\""));
    CHECK(firstXml.contains("password=\"recent-secret\""));

    auto secondStore = makeRememberedServerStore(tempStore.file());
    const auto reloaded = secondStore->load();

    REQUIRE(reloaded.size() == 2);
    CHECK(reloaded[0] == RememberedServerEntry {
        .host = "recent.example.org",
        .port = 2050,
        .username = "recent_user",
        .password = "recent-secret",
    });
    CHECK(reloaded[1] == RememberedServerEntry {
        .host = "older.example.org",
        .port = 2051,
        .username = "older_user",
        .password = "older-secret",
    });

    REQUIRE(secondStore->save(reloaded));
    CHECK(tempStore.file().loadFileAsString().replace("\r\n", "\n") == firstXml);
}

TEST_CASE("remembered server store treats missing files as empty history", "[remembered_server_store]")
{
    ScopedTempStoreFile tempStore;
    auto store = makeRememberedServerStore(tempStore.file());

    REQUIRE_FALSE(tempStore.file().existsAsFile());
    CHECK(store->load().empty());
}

TEST_CASE("remembered server store falls back to empty history on corrupt XML", "[remembered_server_store]")
{
    ScopedTempStoreFile tempStore;
    REQUIRE(tempStore.file().replaceWithText("not valid xml", false, false, "\n"));

    auto store = makeRememberedServerStore(tempStore.file());
    CHECK(store->load().empty());
}
