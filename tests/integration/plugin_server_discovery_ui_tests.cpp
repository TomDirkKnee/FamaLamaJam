#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettingsController.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"

namespace
{
using famalamajam::app::session::ConnectionLifecycleSnapshot;
using famalamajam::app::session::ConnectionState;
using famalamajam::app::session::SessionSettings;
using famalamajam::app::session::SessionSettingsController;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::plugin::FamaLamaJamAudioProcessorEditor;

struct EditorHarness
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor;
    SessionSettings settings;
    ConnectionLifecycleSnapshot lifecycle;
    FamaLamaJamAudioProcessorEditor::TransportUiState transport;
    FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState discovery;
    int connectCallCount { 0 };
    int refreshCallCount { 0 };
    bool lastRefreshWasManual { false };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness()
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle({
              .state = ConnectionState::Idle,
              .statusMessage = "Ready",
          })
    {
        discovery.combinedEntries = {
            {
                .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered,
                .label = "recent.example.org:2050",
                .host = "recent.example.org",
                .port = 2050,
                .username = "remembered_user",
                .password = "remembered-secret",
            },
            {
                .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
                .label = "public.example.org:2051 - Public Groove (12/20 users)",
                .host = "public.example.org",
                .port = 2051,
                .connectedUsers = 12,
            },
        };

        editor = std::make_unique<FamaLamaJamAudioProcessorEditor>(
            processor,
            [this]() { return settings; },
            [this](SessionSettings draft) {
                settings = std::move(draft);
                return SessionSettingsController::ApplyResult { true, {}, "Applied" };
            },
            [this]() { return lifecycle; },
            [this]() {
                ++connectCallCount;
                return true;
            },
            []() { return true; },
            [this]() { return transport; },
            []() { return FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {}; },
            []() { return false; },
            []() { return std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> {}; },
            [](const std::string&, float, float, bool) { return false; },
            []() { return false; },
            [](bool) {},
            [this]() { return discovery; },
            [this](bool manual) {
                ++refreshCallCount;
                lastRefreshWasManual = manual;
                return true;
            });
    }
};
} // namespace

TEST_CASE("plugin server discovery ui shows remembered entries before public entries and auto-refreshes on open",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;

    const auto labels = harness.editor->getVisibleServerDiscoveryLabelsForTesting();
    REQUIRE(labels.size() == 2);
    CHECK(labels[0] == "recent.example.org:2050");
    CHECK(labels[1] == "public.example.org:2051 - Public Groove (12/20 users)");
    CHECK(harness.refreshCallCount == 1);
    CHECK_FALSE(harness.lastRefreshWasManual);
}

TEST_CASE("plugin server discovery ui selection fills host and port without auto-connecting",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;
    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(1));

    CHECK(harness.editor->getHostTextForTesting() == "public.example.org");
    CHECK(harness.editor->getPortTextForTesting() == "2051");
    CHECK(harness.connectCallCount == 0);

    harness.settings.serverHost = "typed.example.org";
    harness.settings.serverPort = 2090;
    harness.editor->setSettingsDraftForTesting(harness.settings);
    CHECK(harness.editor->getHostTextForTesting() == "typed.example.org");
    CHECK(harness.editor->getPortTextForTesting() == "2090");
}

TEST_CASE("plugin server discovery ui shows a compact disconnected target summary without empty timing filler",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;

    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(1));

    CHECK(harness.editor->getServerSettingsSummaryForTesting() == "Disconnected from public.example.org:2051");
    CHECK(harness.editor->getTransportStatusTextForTesting().isEmpty());
}

TEST_CASE("plugin server discovery ui recalls remembered private credentials with a masked password placeholder",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;
    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(0));

    CHECK(harness.editor->getHostTextForTesting() == "recent.example.org");
    CHECK(harness.editor->getPortTextForTesting() == "2050");
    CHECK(harness.editor->getUsernameTextForTesting() == "remembered_user");
    CHECK(harness.editor->getPasswordTextForTesting() == "********");
    CHECK(harness.connectCallCount == 0);
}

TEST_CASE("plugin server discovery ui exposes stale failure state and manual refresh action",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;
    harness.discovery.statusText = "Couldn't refresh the public server list. Showing cached servers.";
    harness.discovery.hasStalePublicData = true;
    harness.discovery.combinedEntries[1].stale = true;
    harness.editor->refreshForTesting();

    CHECK(harness.editor->getServerDiscoveryStatusTextForTesting()
          == "Couldn't refresh the public server list. Showing cached servers.");

    REQUIRE(harness.editor->clickServerDiscoveryRefreshForTesting());
    CHECK(harness.refreshCallCount == 2);
    CHECK(harness.lastRefreshWasManual);
}

TEST_CASE("plugin server discovery ui preserves selected endpoint across public reordering",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;
    harness.discovery.combinedEntries.push_back({
        .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
        .label = "busy.example.org:2054 - Busy Groove (20 users)",
        .host = "busy.example.org",
        .port = 2054,
        .connectedUsers = 20,
    });
    harness.editor->refreshForTesting();

    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(2));
    CHECK(harness.editor->getSelectedServerDiscoveryLabelForTesting() == "busy.example.org:2054 - Busy Groove (20 users)");

    harness.discovery.combinedEntries = {
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered,
            .label = "recent.example.org:2050",
            .host = "recent.example.org",
            .port = 2050,
        },
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
            .label = "busy.example.org:2054 - Busy Groove (20 users)",
            .host = "busy.example.org",
            .port = 2054,
            .connectedUsers = 20,
        },
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
            .label = "public.example.org:2051 - Public Groove (12/20 users)",
            .host = "public.example.org",
            .port = 2051,
            .connectedUsers = 12,
        },
    };

    harness.editor->refreshForTesting();

    CHECK(harness.editor->getSelectedServerDiscoveryLabelForTesting() == "busy.example.org:2054 - Busy Groove (20 users)");
}

TEST_CASE("plugin server discovery ui keeps the public row selected while restoring remembered credentials after refresh",
          "[plugin_server_discovery_ui]")
{
    EditorHarness harness;
    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(1));
    CHECK(harness.editor->getSelectedServerDiscoveryLabelForTesting() == "public.example.org:2051 - Public Groove (12/20 users)");

    harness.discovery.combinedEntries = {
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
            .label = "public.example.org:2051 - Public Groove (12/20 users)",
            .host = "public.example.org",
            .port = 2051,
            .username = "public_user",
            .password = "public-secret",
            .connectedUsers = 12,
        },
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered,
            .label = "recent.example.org:2050",
            .host = "recent.example.org",
            .port = 2050,
            .username = "remembered_user",
            .password = "remembered-secret",
        },
        {
            .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
            .label = "busy.example.org:2054 - Busy Groove (20 users)",
            .host = "busy.example.org",
            .port = 2054,
            .connectedUsers = 20,
        },
    };

    harness.editor->refreshForTesting();

    CHECK(harness.editor->getSelectedServerDiscoveryLabelForTesting()
          == "public.example.org:2051 - Public Groove (12/20 users)");
    CHECK(harness.editor->getHostTextForTesting() == "public.example.org");
    CHECK(harness.editor->getPortTextForTesting() == "2051");
    CHECK(harness.editor->getUsernameTextForTesting() == "public_user");
    CHECK(harness.editor->getPasswordTextForTesting() == "********");
}
