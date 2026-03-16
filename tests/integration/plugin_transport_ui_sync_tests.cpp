#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
    bool metronomeEnabled { false };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(ConnectionLifecycleSnapshot lifecycleSnapshot,
                  FamaLamaJamAudioProcessorEditor::TransportUiState transportState,
                  bool initialMetronomeEnabled = false)
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(std::move(lifecycleSnapshot))
        , transport(std::move(transportState))
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
            []() { return std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> {}; },
            [](const std::string&, float, float, bool) { return true; },
            [this]() { return metronomeEnabled; },
            [this](bool enabled) { metronomeEnabled = enabled; });
    }
};
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

    CHECK(waitingHarness.editor->getTransportStatusTextForTesting() == "Waiting for interval timing. Start when the beat appears.");
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

    CHECK(reconnectHarness.editor->getTransportStatusTextForTesting() == "Interval timing paused while reconnecting.");
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

    CHECK(healthyHarness.editor->getTransportStatusTextForTesting() == "Beat 3/8 | 120 BPM | 38%");
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

    CHECK(lostHarness.editor->getTransportStatusTextForTesting() == "Interval timing lost. Wait for timing or reconnect.");
    CHECK(lostHarness.editor->getIntervalProgressForTesting() == Catch::Approx(0.0));
    CHECK(lostHarness.editor->getIntervalBeatDivisionsForTesting() == 0);
    CHECK_FALSE(lostHarness.editor->isMetronomeToggleEnabledForTesting());
}
