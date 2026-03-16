#include <memory>

#include <catch2/catch_approx.hpp>
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
    std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> mixerStrips;
    bool metronomeEnabled { false };
    int applyCallCount { 0 };
    int connectCallCount { 0 };
    SessionSettingsController::ApplyResult applyResult { true, {}, "Applied" };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(ConnectionLifecycleSnapshot lifecycleSnapshot,
                  FamaLamaJamAudioProcessorEditor::TransportUiState transportState,
                  std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> strips = {})
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(std::move(lifecycleSnapshot))
        , transport(std::move(transportState))
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
            [this](bool enabled) { metronomeEnabled = enabled; });
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
    auto* connectButton = findDirectButtonWithText(*harness.editor, "Connect");
    auto* statusLabel =
        findDirectLabelWithText(*harness.editor, "Ready to join. Check settings, then press Connect.");
    auto* mixerSectionLabel = findDirectLabelWithText(*harness.editor, "Mixer");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(connectButton != nullptr);
    REQUIRE(statusLabel != nullptr);
    REQUIRE(mixerSectionLabel != nullptr);

    CHECK(hostLabel->isVisible());
    CHECK(connectButton->isVisible());
    CHECK(statusLabel->isVisible());
    CHECK(statusLabel->getY() < mixerSectionLabel->getY());
    CHECK(connectButton->getBottom() < mixerSectionLabel->getY());
    CHECK(harness.editor->getVisibleMixerStripLabelsForTesting().size() == 2);
}

TEST_CASE("plugin rehearsal ui flow keeps actionable failure copy visible until the state changes",
          "[plugin_rehearsal_ui_flow]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Error,
                              .statusMessage = "Couldn't connect. Check settings and press Connect.",
                              .lastError = "auth rejected",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = false,
                              .hasServerTiming = false,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost,
                              .metronomeAvailable = false,
                          });

    auto* statusLabel =
        findDirectLabelWithText(*harness.editor, "Couldn't connect. Check settings and press Connect.");
    REQUIRE(statusLabel != nullptr);
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Interval timing lost. Wait for timing or reconnect.");

    harness.editor->refreshForTesting();
    CHECK(statusLabel->getText() == "Couldn't connect. Check settings and press Connect.");
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Interval timing lost. Wait for timing or reconnect.");

    harness.lifecycle = ConnectionLifecycleSnapshot {
        .state = ConnectionState::Active,
        .statusMessage = "Connected. Start playing when the beat appears.",
    };
    harness.transport = FamaLamaJamAudioProcessorEditor::TransportUiState {
        .connected = true,
        .hasServerTiming = false,
        .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming,
        .metronomeAvailable = false,
    };
    harness.editor->refreshForTesting();

    CHECK(statusLabel->getText() == "Connected. Start playing when the beat appears.");
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Waiting for interval timing. Start when the beat appears.");
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
    CHECK(harness.editor->getTransportStatusTextForTesting() == "Interval timing paused while reconnecting.");
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
    draft.defaultChannelGainDb = -9.0f;
    draft.defaultChannelPan = 0.25f;
    draft.defaultChannelMuted = true;

    harness.editor->setSettingsDraftForTesting(draft);
    harness.editor->clickConnectForTesting();

    CHECK(findDirectButtonWithText(*harness.editor, "Apply") == nullptr);
    CHECK(harness.applyCallCount == 1);
    CHECK(harness.connectCallCount == 1);
    CHECK(harness.settings.serverHost == "ninjam.example.net");
    CHECK(harness.settings.serverPort == 2049);
    CHECK(harness.settings.username == "Dirk");
    CHECK(harness.settings.defaultChannelGainDb == Catch::Approx(-9.0f));
    CHECK(harness.settings.defaultChannelPan == Catch::Approx(0.25f));
    CHECK(harness.settings.defaultChannelMuted);
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
