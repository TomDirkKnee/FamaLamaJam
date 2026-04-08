#include <memory>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettingsController.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"

namespace
{
using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
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
    FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssist;
    bool metronomeEnabled { false };
    int hostSyncAssistToggleCallCount { 0 };
    bool hostSyncAssistToggleResult { true };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(ConnectionLifecycleSnapshot lifecycleSnapshot,
                  FamaLamaJamAudioProcessorEditor::TransportUiState transportState,
                  bool initialMetronomeEnabled = false,
                  FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssistState = {})
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(std::move(lifecycleSnapshot))
        , transport(std::move(transportState))
        , hostSyncAssist(std::move(hostSyncAssistState))
        , metronomeEnabled(initialMetronomeEnabled)
    {
        editor = std::make_unique<FamaLamaJamAudioProcessorEditor>(
            processor,
            [this]() { return settings; },
            [this](SessionSettings draft) {
                settings = std::move(draft);
                return SessionSettingsController::ApplyResult { true, {}, "Applied" };
            },
            [this]() { return lifecycle; },
            []() { return true; },
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
            []() { return std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> {}; },
            [](const std::string&, float, float, bool) { return true; },
            [this]() { return metronomeEnabled; },
            [this](bool enabled) { metronomeEnabled = enabled; },
            []() { return FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState {}; },
            [](bool) { return false; });
    }
};

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

juce::Label* findLabelWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::Label>(parent, [&](const juce::Label& label) { return label.getText() == text; });
}

juce::TextButton* findButtonWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::TextButton>(parent,
                                           [&](const juce::TextButton& button) { return button.getButtonText() == text; });
}

juce::ToggleButton* findToggleButtonWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::ToggleButton>(parent,
                                             [&](const juce::ToggleButton& button) { return button.getButtonText() == text; });
}

juce::Slider* findSlider(juce::Component& parent, juce::Slider::SliderStyle style, juce::Slider::TextEntryBoxPosition textBox)
{
    return findComponent<juce::Slider>(parent, [&](const juce::Slider& slider) {
        return slider.getSliderStyle() == style && slider.getTextBoxPosition() == textBox;
    });
}

juce::Rectangle<int> getBoundsInEditor(juce::Component& editor, juce::Component& component)
{
    return editor.getLocalArea(&component, component.getLocalBounds());
}
} // namespace

TEST_CASE("plugin transport ui sync exposes processor sync states", "[plugin_transport_ui_sync]")
{
    FamaLamaJamAudioProcessor processor;

    auto state = processor.getTransportUiState();
    CHECK(state.syncHealth == FamaLamaJamAudioProcessor::SyncHealth::Disconnected);
    CHECK_FALSE(state.metronomeAvailable);

    REQUIRE(processor.requestConnect());
    state = processor.getTransportUiState();
    CHECK(state.syncHealth == FamaLamaJamAudioProcessor::SyncHealth::WaitingForTiming);
    CHECK_FALSE(state.metronomeAvailable);

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::Connected,
        .reason = {},
    });
    state = processor.getTransportUiState();
    CHECK(state.syncHealth == FamaLamaJamAudioProcessor::SyncHealth::WaitingForTiming);

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "transport disconnected",
    });
    state = processor.getTransportUiState();
    CHECK(state.syncHealth == FamaLamaJamAudioProcessor::SyncHealth::Reconnecting);
    CHECK_FALSE(state.metronomeAvailable);

    FamaLamaJamAudioProcessor errorProcessor;
    REQUIRE(errorProcessor.requestConnect());
    errorProcessor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionFailed,
        .reason = "timing unavailable",
    });

    state = errorProcessor.getTransportUiState();
    CHECK(state.syncHealth == FamaLamaJamAudioProcessor::SyncHealth::TimingLost);
    CHECK_FALSE(state.metronomeAvailable);
}

TEST_CASE("plugin transport ui sync renders waiting and reconnecting states explicitly", "[plugin_transport_ui_sync]")
{
    EditorHarness waitingHarness(ConnectionLifecycleSnapshot {
                                     .state = ConnectionState::Active,
                                     .statusMessage = "Connected",
                                 },
                                 FamaLamaJamAudioProcessorEditor::TransportUiState {
                                     .connected = true,
                                     .hasServerTiming = false,
                                     .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming,
                                     .metronomeAvailable = false,
                                 },
                                 true);

    CHECK(waitingHarness.editor->getTransportStatusTextForTesting() == "Waiting for room timing.");
    CHECK(waitingHarness.editor->getIntervalProgressForTesting() == Catch::Approx(0.0));
    CHECK(waitingHarness.editor->getIntervalBeatDivisionsForTesting() == 0);
    CHECK_FALSE(waitingHarness.editor->isMetronomeToggleEnabledForTesting());

    EditorHarness reconnectHarness(ConnectionLifecycleSnapshot {
                                       .state = ConnectionState::Reconnecting,
                                       .statusMessage = "Reconnecting",
                                   },
                                   FamaLamaJamAudioProcessorEditor::TransportUiState {
                                       .connected = false,
                                       .hasServerTiming = false,
                                       .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Reconnecting,
                                       .metronomeAvailable = false,
                                   });

    CHECK(reconnectHarness.editor->getTransportStatusTextForTesting() == "Reconnecting room timing.");
    CHECK(reconnectHarness.editor->getIntervalProgressForTesting() == Catch::Approx(0.0));
    CHECK(reconnectHarness.editor->getIntervalBeatDivisionsForTesting() == 0);
}

TEST_CASE("plugin transport ui sync renders healthy beat-divided progress and timing loss honestly",
          "[plugin_transport_ui_sync]")
{
    EditorHarness healthyHarness(ConnectionLifecycleSnapshot {
                                     .state = ConnectionState::Active,
                                     .statusMessage = "Connected",
                                 },
                                 FamaLamaJamAudioProcessorEditor::TransportUiState {
                                     .connected = true,
                                     .hasServerTiming = true,
                                     .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                                     .metronomeAvailable = true,
                                     .beatsPerMinute = 120,
                                     .beatsPerInterval = 8,
                                     .currentBeat = 3,
                                     .intervalProgress = 0.375f,
                                     .intervalIndex = 4,
                                 },
                                 true);

    CHECK(healthyHarness.editor->getTransportStatusTextForTesting() == "120 BPM | 8 BPI");
    CHECK(healthyHarness.editor->getIntervalProgressForTesting() == Catch::Approx(0.375));
    CHECK(healthyHarness.editor->getIntervalBeatDivisionsForTesting() == 8);
    CHECK(healthyHarness.editor->isMetronomeToggleEnabledForTesting());

    EditorHarness lostHarness(ConnectionLifecycleSnapshot {
                                  .state = ConnectionState::Error,
                                  .statusMessage = "Error",
                                  .lastError = "timing unavailable",
                              },
                              FamaLamaJamAudioProcessorEditor::TransportUiState {
                                  .connected = false,
                                  .hasServerTiming = false,
                                  .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost,
                                  .metronomeAvailable = false,
                                  .beatsPerMinute = 120,
                                  .beatsPerInterval = 8,
                                  .currentBeat = 4,
                                  .intervalProgress = 0.625f,
                              },
                              true);

    CHECK(lostHarness.editor->getTransportStatusTextForTesting() == "120 BPM | 8 BPI");
    CHECK(lostHarness.editor->getIntervalProgressForTesting() == Catch::Approx(0.0));
    CHECK(lostHarness.editor->getIntervalBeatDivisionsForTesting() == 0);
    CHECK_FALSE(lostHarness.editor->isMetronomeToggleEnabledForTesting());
}

TEST_CASE("plugin transport ui sync shows a ready arm control with room timing context", "[plugin_transport_ui_sync]")
{
    EditorHarness readyHarness(ConnectionLifecycleSnapshot {
                                   .state = ConnectionState::Active,
                                   .statusMessage = "Connected",
                               },
                               FamaLamaJamAudioProcessorEditor::TransportUiState {
                                   .connected = true,
                                   .hasServerTiming = true,
                                   .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                                   .metronomeAvailable = true,
                                   .beatsPerMinute = 120,
                                   .beatsPerInterval = 16,
                                   .currentBeat = 5,
                                   .intervalProgress = 0.25f,
                                   .intervalIndex = 9,
                               },
                               true,
                               FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                                   .armable = true,
                                   .targetBeatsPerMinute = 120,
                                   .targetBeatsPerInterval = 16,
                               });

    CHECK(readyHarness.editor->getHostSyncAssistButtonTextForTesting() == "Arm Sync to Ableton Play");
    CHECK(readyHarness.editor->getHostSyncAssistStatusTextForTesting().isEmpty());
    CHECK(readyHarness.editor->isHostSyncAssistEnabledForTesting());
}

TEST_CASE("plugin transport ui sync keeps timing text and sync assist in a footer below the strip field",
          "[plugin_transport_ui_sync]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 5,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 9,
                          },
                          true,
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .armable = true,
                              .targetBeatsPerMinute = 120,
                              .targetBeatsPerInterval = 16,
                          });

    auto* transportLabel = findLabelWithText(*harness.editor, "120 BPM | 16 BPI");
    auto* syncButton = findButtonWithText(*harness.editor, "Arm Sync to Ableton Play");
    auto* masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");

    REQUIRE(transportLabel != nullptr);
    REQUIRE(syncButton != nullptr);
    REQUIRE(masterOutputLabel != nullptr);

    CHECK(transportLabel->getY() > harness.editor->getHeight() / 2);
    CHECK(syncButton->getY() > harness.editor->getHeight() / 2);

    harness.editor->setSize(760, 760);
    harness.editor->resized();

    transportLabel = findLabelWithText(*harness.editor, "120 BPM | 16 BPI");
    syncButton = findButtonWithText(*harness.editor, "Arm Sync to Ableton Play");
    masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");

    REQUIRE(transportLabel != nullptr);
    REQUIRE(syncButton != nullptr);
    REQUIRE(masterOutputLabel != nullptr);

    CHECK(syncButton->getRight() < masterOutputLabel->getX());
    CHECK(transportLabel->getBottom() <= masterOutputLabel->getBottom() + 10);

    harness.editor->setSize(700, 760);
    harness.editor->resized();

    transportLabel = findLabelWithText(*harness.editor, "120 BPM | 16 BPI");
    syncButton = findButtonWithText(*harness.editor, "Arm Sync to Ableton Play");
    masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");

    REQUIRE(transportLabel != nullptr);
    REQUIRE(syncButton != nullptr);
    REQUIRE(masterOutputLabel != nullptr);

    CHECK(syncButton->getRight() < masterOutputLabel->getX());
    CHECK(transportLabel->getBottom() <= masterOutputLabel->getBottom() + 10);
}

TEST_CASE("plugin transport ui sync disables the arm control with explicit blocked guidance",
          "[plugin_transport_ui_sync]")
{
    EditorHarness missingTimingHarness(ConnectionLifecycleSnapshot {
                                           .state = ConnectionState::Active,
                                           .statusMessage = "Connected",
                                       },
                                       FamaLamaJamAudioProcessorEditor::TransportUiState {
                                           .connected = true,
                                           .hasServerTiming = false,
                                           .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming,
                                           .metronomeAvailable = false,
                                       },
                                       false,
                                       FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                                           .blocked = true,
                                           .blockReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::MissingServerTiming,
                                           .targetBeatsPerMinute = 120,
                                           .targetBeatsPerInterval = 16,
                                       });

    CHECK(missingTimingHarness.editor->getHostSyncAssistButtonTextForTesting() == "Arm Sync to Ableton Play");
    CHECK(missingTimingHarness.editor->getHostSyncAssistStatusTextForTesting()
          == "Sync assist needs room timing before it can arm.");
    CHECK_FALSE(missingTimingHarness.editor->isHostSyncAssistEnabledForTesting());

    EditorHarness mismatchHarness(ConnectionLifecycleSnapshot {
                                      .state = ConnectionState::Active,
                                      .statusMessage = "Connected",
                                  },
                                  FamaLamaJamAudioProcessorEditor::TransportUiState {
                                      .connected = true,
                                      .hasServerTiming = true,
                                      .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                                      .metronomeAvailable = true,
                                      .beatsPerMinute = 121,
                                      .beatsPerInterval = 16,
                                      .currentBeat = 2,
                                      .intervalProgress = 0.15f,
                                      .intervalIndex = 7,
                                  },
                                  true,
                                  FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                                      .blocked = true,
                                      .blockReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::HostTempoMismatch,
                                      .targetBeatsPerMinute = 121,
                                      .targetBeatsPerInterval = 16,
                                  });

    CHECK(mismatchHarness.editor->getHostSyncAssistButtonTextForTesting() == "Set DAW tempo to 121");
    CHECK(mismatchHarness.editor->getHostSyncAssistStatusTextForTesting().isEmpty());
    CHECK_FALSE(mismatchHarness.editor->isHostSyncAssistEnabledForTesting());
}

TEST_CASE("plugin transport ui sync restores a wider default editor and a full-width footer with metronome volume",
          "[plugin_transport_ui_sync]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 5,
                              .intervalProgress = 0.25f,
                              .intervalIndex = 9,
                          },
                          true,
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .armable = true,
                              .targetBeatsPerMinute = 120,
                              .targetBeatsPerInterval = 16,
                          });

    auto* transportLabel = findLabelWithText(*harness.editor, "120 BPM | 16 BPI");
    auto* metronomeToggle = findToggleButtonWithText(*harness.editor, "Metronome");
    auto* syncButton = findButtonWithText(*harness.editor, "Arm Sync to Ableton Play");
    auto* masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");
    auto* metronomeVolumeSlider = findSlider(*harness.editor,
                                             juce::Slider::RotaryHorizontalVerticalDrag,
                                             juce::Slider::TextBoxBelow);
    auto* intervalProgress =
        findComponent<famalamajam::plugin::BeatDividedProgressBar>(*harness.editor,
                                                                   [](const auto&) { return true; });

    REQUIRE(transportLabel != nullptr);
    REQUIRE(metronomeToggle != nullptr);
    REQUIRE(syncButton != nullptr);
    REQUIRE(masterOutputLabel != nullptr);
    REQUIRE(metronomeVolumeSlider != nullptr);
    REQUIRE(intervalProgress != nullptr);

    const auto transportBounds = getBoundsInEditor(*harness.editor, *transportLabel);
    const auto metronomeBounds = getBoundsInEditor(*harness.editor, *metronomeVolumeSlider);
    const auto syncBounds = getBoundsInEditor(*harness.editor, *syncButton);
    const auto masterBounds = getBoundsInEditor(*harness.editor, *masterOutputLabel);
    const auto progressBounds = getBoundsInEditor(*harness.editor, *intervalProgress);

    CHECK(harness.editor->getWidth() >= 1350);
    CHECK(transportBounds.getY() > harness.editor->getHeight() / 2);
    CHECK(metronomeBounds.getY() > harness.editor->getHeight() / 2);
    CHECK(syncBounds.getRight() < masterBounds.getX());
    CHECK(progressBounds.getX() <= 16);
    CHECK(progressBounds.getRight() >= harness.editor->getWidth() - 16);
}

TEST_CASE("plugin transport ui sync renders armed canceled and failed-start guidance in plain language",
          "[plugin_transport_ui_sync]")
{
    EditorHarness harness(ConnectionLifecycleSnapshot {
                              .state = ConnectionState::Active,
                              .statusMessage = "Connected",
                          },
                          FamaLamaJamAudioProcessorEditor::TransportUiState {
                              .connected = true,
                              .hasServerTiming = true,
                              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
                              .metronomeAvailable = true,
                              .beatsPerMinute = 120,
                              .beatsPerInterval = 16,
                              .currentBeat = 1,
                              .intervalProgress = 0.0f,
                              .intervalIndex = 12,
                          },
                          true,
                          FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
                              .armable = true,
                              .targetBeatsPerMinute = 120,
                              .targetBeatsPerInterval = 16,
                          });

    harness.editor->clickHostSyncAssistForTesting();
    CHECK(harness.hostSyncAssistToggleCallCount == 1);
    CHECK(harness.editor->getHostSyncAssistButtonTextForTesting() == "Cancel Sync to Ableton Play");
    CHECK(harness.editor->getHostSyncAssistStatusTextForTesting() == "Armed. Press Play in Ableton to start aligned.");
    CHECK(harness.editor->isHostSyncAssistEnabledForTesting());

    harness.editor->clickHostSyncAssistForTesting();
    CHECK(harness.hostSyncAssistToggleCallCount == 2);
    CHECK(harness.editor->getHostSyncAssistButtonTextForTesting() == "Arm Sync to Ableton Play");
    CHECK(harness.editor->getHostSyncAssistStatusTextForTesting()
          == "Sync assist canceled. Arm again when you want Ableton Play to start aligned.");
    CHECK(harness.editor->isHostSyncAssistEnabledForTesting());

    harness.hostSyncAssist = FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
        .armable = true,
        .failed = true,
        .failureReason = FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::MissingHostMusicalPosition,
        .targetBeatsPerMinute = 120,
        .targetBeatsPerInterval = 16,
    };
    harness.editor->refreshForTesting();

    CHECK(harness.editor->getHostSyncAssistButtonTextForTesting() == "Arm Sync to Ableton Play");
    CHECK(harness.editor->getHostSyncAssistStatusTextForTesting()
          == "Couldn't align from Ableton's playhead. Re-arm and try again.");
    CHECK(harness.editor->isHostSyncAssistEnabledForTesting());
}
