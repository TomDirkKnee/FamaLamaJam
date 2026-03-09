#include <juce_data_structures/juce_data_structures.h>

#include <catch2/catch_test_macros.hpp>

#include "app/session/SessionSettings.h"
#include "infra/state/SessionSettingsSerializer.h"

using namespace famalamajam::app::session;
using famalamajam::infra::state::SessionSettingsSerializer;

TEST_CASE("state serialization round-trip preserves values", "[state_serialization]")
{
    const SessionSettings settings {
        .serverHost = "ninjam.example.org",
        .serverPort = 2050,
        .username = "player_one",
        .defaultChannelGainDb = -3.0f,
        .defaultChannelPan = 0.5f,
        .defaultChannelMuted = true,
    };

    juce::MemoryBlock blob;
    SessionSettingsSerializer::serialize(settings, blob);

    bool usedFallback = true;
    const SessionSettings restored = SessionSettingsSerializer::deserializeOrDefault(blob.getData(),
                                                                                     static_cast<int>(blob.getSize()),
                                                                                     &usedFallback);

    REQUIRE_FALSE(usedFallback);
    CHECK(restored == settings);
}

TEST_CASE("state serialization falls back on corrupt payload", "[state_serialization]")
{
    const unsigned char invalidBytes[] {0x01, 0x02, 0x03, 0x04};

    bool usedFallback = false;
    const SessionSettings restored = SessionSettingsSerializer::deserializeOrDefault(invalidBytes,
                                                                                     static_cast<int>(sizeof(invalidBytes)),
                                                                                     &usedFallback);

    REQUIRE(usedFallback);
    CHECK(restored == makeDefaultSessionSettings());
}

TEST_CASE("state serialization ignores unknown fields", "[state_serialization]")
{
    juce::ValueTree tree("famalamajam.session.settings");
    tree.setProperty("schemaVersion", SessionSettingsSerializer::schemaVersion, nullptr);
    tree.setProperty("serverHost", "ninjam.example.org", nullptr);
    tree.setProperty("serverPort", 2049, nullptr);
    tree.setProperty("username", "guest", nullptr);
    tree.setProperty("defaultChannelGainDb", 0.0f, nullptr);
    tree.setProperty("defaultChannelPan", 0.0f, nullptr);
    tree.setProperty("defaultChannelMuted", false, nullptr);
    tree.setProperty("futureField", "ignored", nullptr);

    juce::MemoryBlock blob;
    juce::MemoryOutputStream stream(blob, false);
    tree.writeToStream(stream);

    bool usedFallback = true;
    const SessionSettings restored = SessionSettingsSerializer::deserializeOrDefault(blob.getData(),
                                                                                     static_cast<int>(blob.getSize()),
                                                                                     &usedFallback);

    REQUIRE_FALSE(usedFallback);
    CHECK(restored.serverHost == "ninjam.example.org");
    CHECK(restored.serverPort == 2049);
    CHECK(restored.username == "guest");
}

