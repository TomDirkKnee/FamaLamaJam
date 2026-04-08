#include <memory>
#include <cmath>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettingsController.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::app::session::ConnectionLifecycleSnapshot;
using famalamajam::app::session::ConnectionState;
using famalamajam::app::session::SessionSettings;
using famalamajam::app::session::SessionSettingsController;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::plugin::FamaLamaJamAudioProcessorEditor;
using famalamajam::tests::integration::MiniNinjamServer;

struct EditorHarness
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor;
    SessionSettings settings;
    ConnectionLifecycleSnapshot lifecycle;
    FamaLamaJamAudioProcessorEditor::TransportUiState transport;
    FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssist;
    FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState discovery;
    std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> mixerStrips;
    bool metronomeEnabled { false };
    int applyCallCount { 0 };
    int connectCallCount { 0 };
    int hostSyncAssistToggleCallCount { 0 };
    bool hostSyncAssistToggleResult { true };
    SessionSettingsController::ApplyResult applyResult { true, {}, "Applied" };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(ConnectionLifecycleSnapshot lifecycleSnapshot,
                  FamaLamaJamAudioProcessorEditor::TransportUiState transportState,
                  std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> strips = {},
                  FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssistState = {})
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(std::move(lifecycleSnapshot))
        , transport(std::move(transportState))
        , hostSyncAssist(std::move(hostSyncAssistState))
        , mixerStrips(std::move(strips))
    {
        editor = std::make_unique<FamaLamaJamAudioProcessorEditor>(
            processor,
            [this]() { return settings; },
            [this](SessionSettings draft) {
                ++applyCallCount;
                settings = std::move(draft);
                return applyResult;
            },
            [this]() { return lifecycle; },
            [this]() {
                ++connectCallCount;
                return true;
            },
            []() { return true; },
            [this]() { return transport; },
            [this]() { return hostSyncAssist; },
            [this]() {
                ++hostSyncAssistToggleCallCount;

                if (! hostSyncAssistToggleResult)
                    return false;

                hostSyncAssist.blocked = false;
                hostSyncAssist.failed = false;
                hostSyncAssist.blockReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::None;
                hostSyncAssist.failureReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::None;

                if (hostSyncAssist.armed || hostSyncAssist.waitingForHost)
                {
                    hostSyncAssist.armed = false;
                    hostSyncAssist.waitingForHost = false;
                    hostSyncAssist.armable = true;
                    return true;
                }

                if (! hostSyncAssist.armable)
                    return false;

                hostSyncAssist.armed = true;
                hostSyncAssist.waitingForHost = true;
                return true;
            },
            [this]() { return mixerStrips; },
            [this](const std::string& sourceId, float gain, float pan, bool muted) {
                for (auto& strip : mixerStrips)
                {
                    if (strip.sourceId == sourceId)
                    {
                        strip.gainDb = gain;
                        strip.pan = pan;
                        strip.muted = muted;
                        return true;
                    }
                }

                return false;
            },
            [this]() { return metronomeEnabled; },
            [this](bool enabled) { metronomeEnabled = enabled; },
            [this]() { return discovery; },
            [](bool) { return false; });
    }
};

template <typename ComponentType, typename Matcher>
ComponentType* findDirectChild(juce::Component& parent, Matcher&& matcher)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        if (auto* child = dynamic_cast<ComponentType*>(parent.getChildComponent(index)))
        {
            if (matcher(*child))
                return child;
        }
    }

    return nullptr;
}

juce::Label* findDirectLabelWithText(juce::Component& parent, const juce::String& text)
{
    return findDirectChild<juce::Label>(parent, [&](const juce::Label& label) { return label.getText() == text; });
}

juce::TextButton* findDirectButtonWithText(juce::Component& parent, const juce::String& text)
{
    return findDirectChild<juce::TextButton>(parent,
                                             [&](const juce::TextButton& button) { return button.getButtonText() == text; });
}

juce::TextButton* findDirectButtonContainingText(juce::Component& parent, const juce::String& text)
{
    return findDirectChild<juce::TextButton>(parent, [&](const juce::TextButton& button) {
        return button.getButtonText().contains(text);
    });
}

juce::TextEditor* findDirectTextEditorToRightOf(juce::Component& parent, const juce::Label& label)
{
    return findDirectChild<juce::TextEditor>(parent, [&](const juce::TextEditor& editor) {
        return editor.getX() >= label.getRight()
            && editor.getBounds().getCentreY() == label.getBounds().getCentreY();
    });
}

template <typename ComponentType, typename Matcher>
ComponentType* findComponent(juce::Component& parent, Matcher&& matcher)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        if (auto* child = dynamic_cast<ComponentType*>(parent.getChildComponent(index)))
        {
            if (matcher(*child))
                return child;
        }

        if (auto* match = findComponent<ComponentType>(*parent.getChildComponent(index),
                                                       std::forward<Matcher>(matcher)))
        {
            return match;
        }
    }

    return nullptr;
}

juce::Rectangle<int> getBoundsInEditor(juce::Component& editor, juce::Component& component)
{
    return editor.getLocalArea(&component, component.getLocalBounds());
}

bool waitForLifecycleState(FamaLamaJamAudioProcessor& processor, ConnectionState expectedState, int attempts = 200)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        if (processor.getLifecycleSnapshot().state == expectedState)
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}
} // namespace

TEST_CASE("plugin rehearsal ui flow keeps setup connect and status above the mixer", "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready to join. Check settings, then press Connect.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = false,
                              .hasServerTiming = false,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected,
                              .metronomeAvailable = false,
                          },
                          {
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = "local-monitor",
                                .groupId = "local",
                                .groupLabel = "Local Monitor",
                                .displayName = "Local Monitor",
                                .subtitle = "Live monitor",
                                .active = true,
                                .visible = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                                .sourceId = "alice#0",
                                .groupId = "alice",
                                .groupLabel = "alice",
                                .displayName = "alice - guitar",
                                .subtitle = "guitar",
                                .active = true,
                                .visible = true },
                          });

    auto* hostLabel = findDirectLabelWithText(*harness.editor, "Host");
    auto* passwordLabel = findDirectLabelWithText(*harness.editor, "Password");
    auto* connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    auto* statusLabel =
        findDirectLabelWithText(*harness.editor, "Ready to join. Check settings, then press Connect.");
    auto* mixerSectionLabel = findDirectLabelWithText(*harness.editor, "Mixer");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(passwordLabel != nullptr);
    REQUIRE(connectButton != nullptr);
    REQUIRE(statusLabel != nullptr);
    REQUIRE(mixerSectionLabel != nullptr);

    CHECK(hostLabel->isVisible());
    CHECK(passwordLabel->isVisible());
    CHECK(connectButton->isVisible());
    CHECK(statusLabel->isVisible());
    CHECK(statusLabel->getY() < mixerSectionLabel->getY());
    CHECK(connectButton->getBottom() < mixerSectionLabel->getY());
    CHECK(harness.editor->getVisibleMixerStripLabelsForTesting().size() == 2);
}

TEST_CASE("plugin rehearsal ui flow keeps the disconnected setup above a strip-only local-first mixer plane",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready to join. Check settings, then press Connect.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = false,
                              .hasServerTiming = false,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected,
                              .metronomeAvailable = false,
                          },
                          {
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
                                .groupId = "local",
                                .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
                                .displayName = "Main",
                                .subtitle = "Live monitor",
                                .active = true,
                                .visible = true,
                                .editableName = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
                                .groupId = "local",
                                .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
                                .displayName = "Bass",
                                .subtitle = "Local Send 2",
                                .active = true,
                                .visible = true,
                                .editableName = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                                .sourceId = "alice#0",
                                .groupId = "alice",
                                .groupLabel = "alice",
                                .displayName = "alice - guitar",
                                .subtitle = "guitar",
                                .active = true,
                                .visible = true },
                          });

    auto* connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    auto* statusLabel =
        findDirectLabelWithText(*harness.editor, "Ready to join. Check settings, then press Connect.");
    auto* localHeaderLabel = findComponent<juce::Label>(
        *harness.editor,
        [](const juce::Label& label) { return label.getText() == FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle; });
    auto* aliceGroupLabel =
        findComponent<juce::Label>(*harness.editor, [](const juce::Label& label) { return label.getText() == "alice"; });

    REQUIRE(connectButton != nullptr);
    REQUIRE(statusLabel != nullptr);
    REQUIRE(localHeaderLabel != nullptr);
    REQUIRE(aliceGroupLabel != nullptr);

    const auto connectBounds = getBoundsInEditor(*harness.editor, *connectButton);
    const auto statusBounds = getBoundsInEditor(*harness.editor, *statusLabel);
    const auto localHeaderBounds = getBoundsInEditor(*harness.editor, *localHeaderLabel);
    const auto aliceBounds = getBoundsInEditor(*harness.editor, *aliceGroupLabel);

    CHECK(connectBounds.getBottom() < localHeaderBounds.getY());
    CHECK(statusBounds.getBottom() < localHeaderBounds.getY());
    CHECK(aliceBounds.getX() > localHeaderBounds.getRight());
    CHECK(std::abs(aliceBounds.getY() - localHeaderBounds.getY()) <= 24);

    harness.editor->setSize(980, 720);
    harness.editor->resized();

    connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    statusLabel = findDirectLabelWithText(*harness.editor, "Ready to join. Check settings, then press Connect.");
    localHeaderLabel = findComponent<juce::Label>(
        *harness.editor,
        [](const juce::Label& label) { return label.getText() == FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle; });
    aliceGroupLabel =
        findComponent<juce::Label>(*harness.editor, [](const juce::Label& label) { return label.getText() == "alice"; });

    REQUIRE(connectButton != nullptr);
    REQUIRE(statusLabel != nullptr);
    REQUIRE(localHeaderLabel != nullptr);
    REQUIRE(aliceGroupLabel != nullptr);

    const auto resizedConnectBounds = getBoundsInEditor(*harness.editor, *connectButton);
    const auto resizedStatusBounds = getBoundsInEditor(*harness.editor, *statusLabel);
    const auto resizedLocalHeaderBounds = getBoundsInEditor(*harness.editor, *localHeaderLabel);
    const auto resizedAliceBounds = getBoundsInEditor(*harness.editor, *aliceGroupLabel);

    CHECK(resizedConnectBounds.getBottom() < resizedLocalHeaderBounds.getY());
    CHECK(resizedStatusBounds.getBottom() < resizedLocalHeaderBounds.getY());
    CHECK(resizedAliceBounds.getX() > resizedLocalHeaderBounds.getRight());
}

TEST_CASE("plugin rehearsal ui flow keeps password entry and inline auth failure copy near the connect controls",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Error,
                              .statusMessage = "Error: Wrong room password. Update credentials and press Connect.",
                              .lastError = "Wrong room password",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = false,
                              .hasServerTiming = false,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost,
                              .metronomeAvailable = false,
                          });

    auto* passwordLabel = findDirectLabelWithText(*harness.editor, "Password");
    auto* passwordEditor = passwordLabel == nullptr ? nullptr : findDirectTextEditorToRightOf(*harness.editor, *passwordLabel);
    auto* connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    auto* authLabel = findDirectLabelWithText(*harness.editor, "Authentication failed: Wrong room password");
    auto* transportLabel = findDirectLabelWithText(*harness.editor, "Room timing lost.");

    REQUIRE(passwordLabel != nullptr);
    REQUIRE(passwordEditor != nullptr);
    REQUIRE(connectButton != nullptr);
    REQUIRE(authLabel != nullptr);
    REQUIRE(transportLabel != nullptr);

    CHECK(passwordLabel->isVisible());
    CHECK(passwordEditor->isVisible());
    CHECK(connectButton->isVisible());
    CHECK(authLabel->isVisible());
    CHECK(authLabel->getY() >= passwordLabel->getY());
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Room timing lost.");
}

TEST_CASE("plugin rehearsal ui flow keeps the mixer available without displacing recovery guidance",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Reconnecting,
                              .statusMessage = "Connection dropped. Reconnecting automatically.",
                              .lastError = "socket timeout",
                              .retryAttempt = 1,
                              .retryAttemptLimit = 6,
                              .nextRetryDelayMs = 2000,
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = false,
                              .hasServerTiming = false,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Reconnecting,
                              .metronomeAvailable = false,
                          },
                          {
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = "local-monitor",
                                .groupId = "local",
                                .groupLabel = "Local Monitor",
                                .displayName = "Local Monitor",
                                .subtitle = "Live monitor",
                                .active = true,
                                .visible = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                                .sourceId = "alice#0",
                                .groupId = "alice",
                                .groupLabel = "alice",
                                .displayName = "alice - guitar",
                                .subtitle = "guitar",
                                .active = true,
                                .visible = true },
                          });

    auto* statusLabel = findDirectLabelWithText(*harness.editor, "Connection dropped. Reconnecting automatically.");
    auto* mixerSectionLabel = findDirectLabelWithText(*harness.editor, "Mixer");

    REQUIRE(statusLabel != nullptr);
    REQUIRE(mixerSectionLabel != nullptr);

    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 2);
    CHECK(stripLabels[0] == "Local Monitor");
    CHECK(stripLabels[1] == "alice - guitar");
    CHECK(statusLabel->getBottom() < mixerSectionLabel->getY());
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Reconnecting room timing.");
}

TEST_CASE("plugin rehearsal ui flow keeps host sync assist in the footer transport workflow", "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected. Start playing when the beat appears.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 3,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 6,
                          },
                          {
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = "local-monitor",
                                .groupId = "local",
                                .groupLabel = "Local Monitor",
                                .displayName = "Local Monitor",
                                .subtitle = "Live monitor",
                                .active = true,
                                .visible = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                                .sourceId = "alice#0",
                                .groupId = "alice",
                                .groupLabel = "alice",
                                .displayName = "alice - guitar",
                                .subtitle = "guitar",
                                .active = true,
                                .visible = true },
                          },
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .armable = true,
                              .targetBeatsPerMinute = 120,
                              .targetBeatsPerInterval = 16,
                          });
    auto draft = harness.settings;
    draft.serverHost = "jam.example.net";
    draft.serverPort = 2049;
    draft.username = "Dirk";
    harness.editor->setSettingsDraftForTesting(draft);
    harness.editor->refreshForTesting();

    auto* syncButton = findDirectButtonWithText(*harness.editor, "Arm Sync to Ableton Play");
    auto* mixerSectionLabel = findDirectLabelWithText(*harness.editor, "Mixer");

    REQUIRE(syncButton != nullptr);
    REQUIRE(mixerSectionLabel != nullptr);

    CHECK(syncButton->isVisible());
    CHECK(syncButton->getY() > harness.editor->getHeight() / 2);
    CHECK(syncButton->getY() > mixerSectionLabel->getY());
    CHECK(harness.editor->getServerSettingsSummaryForTesting() == "Connected as Dirk | jam.example.net:2049");
    CHECK(harness.editor->getTransportStatusTextForTesting() == "120 BPM | 16 BPI");
}

TEST_CASE("plugin rehearsal ui flow exposes the new stem-run control only once stem capture is armed",
          "[plugin_rehearsal_ui_flow]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor(true, true);
    auto editor = std::unique_ptr<FamaLamaJamAudioProcessorEditor>(
        dynamic_cast<FamaLamaJamAudioProcessorEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);

    CHECK_FALSE(editor->canClickNewStemRunForTesting());

    const auto tempDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getNonexistentChildFile("famalamajam-stem-run-ui", {}, false);
    REQUIRE(tempDirectory.createDirectory());

    editor->setStemCaptureDirectoryForTesting(tempDirectory.getFullPathName());
    editor->clickStemCaptureToggleForTesting();
    editor->refreshForTesting();

    CHECK(editor->isStemCaptureEnabledForTesting());
    CHECK(editor->canClickNewStemRunForTesting());

    tempDirectory.deleteRecursively();
}

TEST_CASE("plugin rehearsal ui flow places host tempo mismatch guidance on the sync assist button",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected. Start playing when the beat appears.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 123,
                              .beatsPerInterval = 16,
                              .currentBeat = 3,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 6,
                          },
                          {},
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .blocked = true,
                              .blockReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::HostTempoMismatch,
                              .targetBeatsPerMinute = 123,
                              .targetBeatsPerInterval = 16,
                          });

    CHECK(harness.editor->getHostSyncAssistButtonTextForTesting() == "Set DAW tempo to 123");
    CHECK_FALSE(harness.editor->isHostSyncAssistEnabledForTesting());
}

TEST_CASE("plugin rehearsal ui flow keeps the room workflow in a fixed right sidebar instead of above the mixer",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected. Start playing when the beat appears.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 3,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 6,
                          },
                          {
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
                                .sourceId = "local-monitor",
                                .groupId = "local",
                                .groupLabel = "Local Monitor",
                                .displayName = "Local Monitor",
                                .subtitle = "Live monitor",
                                .active = true,
                                .visible = true },
                              { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                                .sourceId = "alice#0",
                                .groupId = "alice",
                                .groupLabel = "alice",
                                .displayName = "alice - guitar",
                                .subtitle = "guitar",
                                .active = true,
                                .visible = true },
                          },
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .armable = true,
                              .targetBeatsPerMinute = 120,
                              .targetBeatsPerInterval = 16,
                          });

    auto* hostLabel = findDirectLabelWithText(*harness.editor, "Host");
    auto* roomLabel = findDirectLabelWithText(*harness.editor, "Room Chat");
    auto* mixerSectionLabel = findDirectLabelWithText(*harness.editor, "Mixer");
    auto* roomTabButton = findDirectButtonWithText(*harness.editor, "Room Tab");
    auto* popoutButton = findDirectButtonWithText(*harness.editor, "Open Chat");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(roomLabel != nullptr);
    REQUIRE(mixerSectionLabel != nullptr);

    CHECK(roomLabel->getX() > hostLabel->getRight());
    CHECK(roomLabel->getX() > mixerSectionLabel->getX());
    CHECK_FALSE(harness.editor->isRoomSectionAboveMixerForTesting());
    CHECK(roomTabButton == nullptr);
    CHECK(popoutButton == nullptr);
}

TEST_CASE("plugin rehearsal ui flow keeps connection actions visible and server fields aligned when settings expand",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected. Start playing when the beat appears.",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 3,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 6,
                          });

    auto* connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    auto* disconnectButton = findDirectButtonWithText(*harness.editor, "Disconnect");
    auto* settingsToggle = findDirectButtonContainingText(*harness.editor, "Server Settings");

    REQUIRE(connectButton != nullptr);
    REQUIRE(disconnectButton != nullptr);
    REQUIRE(settingsToggle != nullptr);

    CHECK(connectButton->isVisible());
    CHECK(disconnectButton->isVisible());

    auto* hostLabel = findDirectLabelWithText(*harness.editor, "Host");
    auto* portLabel = findDirectLabelWithText(*harness.editor, "Port");
    auto* usernameLabel = findDirectLabelWithText(*harness.editor, "Username");
    auto* passwordLabel = findDirectLabelWithText(*harness.editor, "Password");
    connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    disconnectButton = findDirectButtonWithText(*harness.editor, "Disconnect");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(portLabel != nullptr);
    REQUIRE(usernameLabel != nullptr);
    REQUIRE(passwordLabel != nullptr);
    REQUIRE(connectButton != nullptr);
    REQUIRE(disconnectButton != nullptr);

    auto* hostEditor = findDirectTextEditorToRightOf(*harness.editor, *hostLabel);
    auto* portEditor = findDirectTextEditorToRightOf(*harness.editor, *portLabel);
    auto* usernameEditor = findDirectTextEditorToRightOf(*harness.editor, *usernameLabel);
    auto* passwordEditor = findDirectTextEditorToRightOf(*harness.editor, *passwordLabel);

    REQUIRE(hostEditor != nullptr);
    REQUIRE(portEditor != nullptr);
    REQUIRE(usernameEditor != nullptr);
    REQUIRE(passwordEditor != nullptr);

    CHECK(hostLabel->isVisible());
    CHECK(portLabel->isVisible());
    CHECK(usernameLabel->isVisible());
    CHECK(passwordLabel->isVisible());
    CHECK(connectButton->isVisible());
    CHECK(disconnectButton->isVisible());
    CHECK(getBoundsInEditor(*harness.editor, *hostEditor).getX()
          == getBoundsInEditor(*harness.editor, *usernameEditor).getX());
    CHECK(getBoundsInEditor(*harness.editor, *portEditor).getX()
          == getBoundsInEditor(*harness.editor, *passwordEditor).getX());

    const auto passwordBounds = getBoundsInEditor(*harness.editor, *passwordEditor);
    const auto connectBounds = getBoundsInEditor(*harness.editor, *connectButton);
    const auto disconnectBounds = getBoundsInEditor(*harness.editor, *disconnectButton);

    CHECK(connectBounds.getHeight() >= 24);
    CHECK(disconnectBounds.getHeight() >= 24);
    CHECK(connectBounds.getY() >= passwordBounds.getBottom());
    CHECK(disconnectBounds.getY() >= passwordBounds.getBottom());
    CHECK(connectBounds.getBottom() <= passwordBounds.getBottom() + 44);
}

TEST_CASE("plugin rehearsal ui flow applies the current draft when Connect is pressed and hides the separate Apply button",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {});

    SessionSettings draft = harness.settings;
    draft.serverHost = "ninjam.example.net";
    draft.serverPort = 2049;
    draft.username = "Dirk";
    draft.password = "secret-room";

    harness.editor->setSettingsDraftForTesting(draft);
    harness.editor->clickConnectForTesting();

    CHECK(findDirectButtonWithText(*harness.editor, "Apply") == nullptr);
    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 1);
    CHECK(harness.settings.serverHost == "ninjam.example.net");
    CHECK(harness.settings.serverPort == 2049);
    CHECK(harness.settings.username == "Dirk");
    CHECK(harness.settings.password == "secret-room");
}

TEST_CASE("plugin rehearsal ui flow applies the current draft to the live auth path when Connect is pressed",
          "[plugin_rehearsal_ui_flow]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setAuthRules(MiniNinjamServer::AuthRules {
        .validateUsername = true,
        .expectedUsername = "fresh-user",
        .validatePassword = true,
        .expectedPassword = "secret-room",
        .failureText = "Wrong room password",
    });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "stale-user";
    settings.password = "old-room";
    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editorOwner(processor.createEditor());
    auto* editor = dynamic_cast<FamaLamaJamAudioProcessorEditor*>(editorOwner.get());
    REQUIRE(editor != nullptr);

    settings.username = "fresh-user";
    settings.password = "secret-room";
    editor->setSettingsDraftForTesting(settings);
    editor->clickConnectForTesting();

    REQUIRE(waitForLifecycleState(processor, ConnectionState::Active));
    const auto authAttempt = processor.getLastLiveAuthAttemptForTesting();
    CHECK(authAttempt.settingsUsername == "fresh-user");
    CHECK(authAttempt.protocolUsername == "fresh-user");
    CHECK(authAttempt.failureReason.empty());
    CHECK(authAttempt.authenticated);
    CHECK(processor.getActiveSettings().username == "fresh-user");
    CHECK(processor.getActiveSettings().password == "secret-room");
    CHECK(server.getLastAuthUsername() == "fresh-user");
    CHECK(processor.getLifecycleSnapshot().statusMessage.find("NINJAM auth ok") != std::string::npos);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin rehearsal ui flow reuses a recalled remembered password when Connect is pressed",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {});
    harness.discovery = {
        .combinedEntries = {
            {
                .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered,
                .label = "private.example.org:2050",
                .host = "private.example.org",
                .port = 2050,
                .username = "remembered_user",
                .password = "remembered-secret",
            },
        },
    };
    harness.editor->refreshForTesting();

    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(0));
    CHECK(harness.editor->getPasswordTextForTesting() == "********");

    harness.editor->clickConnectForTesting();

    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 1);
    CHECK(harness.settings.serverHost == "private.example.org");
    CHECK(harness.settings.serverPort == 2050);
    CHECK(harness.settings.username == "remembered_user");
    CHECK(harness.settings.password == "remembered-secret");
}

TEST_CASE("plugin rehearsal ui flow replaces remembered password provenance after the user edits the field",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {});
    harness.discovery = {
        .combinedEntries = {
            {
                .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered,
                .label = "private.example.org:2050",
                .host = "private.example.org",
                .port = 2050,
                .username = "remembered_user",
                .password = "remembered-secret",
            },
        },
    };
    harness.editor->refreshForTesting();

    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(0));
    harness.editor->setPasswordTextForTesting("replacement-secret");

    harness.editor->clickConnectForTesting();

    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 1);
    CHECK(harness.settings.password == "replacement-secret");
}

TEST_CASE("plugin rehearsal ui flow reuses overlaid remembered credentials from a selected public row",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {});
    harness.discovery = {
        .combinedEntries = {
            {
                .source = FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
                .label = "public.example.org:2051 - Public Groove (12/20 users)",
                .host = "public.example.org",
                .port = 2051,
                .username = "public_user",
                .password = "remembered-public-secret",
                .connectedUsers = 12,
            },
        },
    };
    harness.editor->refreshForTesting();

    REQUIRE(harness.editor->selectServerDiscoveryEntryForTesting(0));
    CHECK(harness.editor->getPasswordTextForTesting() == "********");

    harness.editor->clickConnectForTesting();

    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 1);
    CHECK(harness.settings.serverHost == "public.example.org");
    CHECK(harness.settings.serverPort == 2051);
    CHECK(harness.settings.username == "public_user");
    CHECK(harness.settings.password == "remembered-public-secret");
}

TEST_CASE("plugin rehearsal ui flow does not connect when the current draft fails validation",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Idle,
                              .statusMessage = "Ready",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {});
    harness.applyResult = SessionSettingsController::ApplyResult { false, {}, "Host is required" };

    SessionSettings draft = harness.settings;
    draft.serverHost.clear();
    harness.editor->setSettingsDraftForTesting(draft);
    harness.editor->clickConnectForTesting();

    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 0);
    auto* statusLabel = findDirectLabelWithText(*harness.editor, "Host is required");
    REQUIRE(statusLabel != nullptr);
}
