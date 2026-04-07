#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

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

template <typename T, typename = void>
constexpr bool hasTransmitStateField = false;

template <typename T>
constexpr bool hasTransmitStateField<T, std::void_t<decltype(std::declval<T&>().transmitState)>> = true;

template <typename T, typename = void>
constexpr bool hasUnsupportedVoiceModeField = false;

template <typename T>
constexpr bool hasUnsupportedVoiceModeField<T,
                                            std::void_t<decltype(std::declval<T&>().unsupportedVoiceMode)>> = true;

template <typename T, typename = void>
constexpr bool hasVoiceModeField = false;

template <typename T>
constexpr bool hasVoiceModeField<T, std::void_t<decltype(std::declval<T&>().voiceMode)>> = true;

template <typename T, typename = void>
constexpr bool hasLocalChannelModeField = false;

template <typename T>
constexpr bool hasLocalChannelModeField<T, std::void_t<decltype(std::declval<T&>().localChannelMode)>> = true;

template <typename T, typename = void>
constexpr bool hasSoloedField = false;

template <typename T>
constexpr bool hasSoloedField<T, std::void_t<decltype(std::declval<T&>().soloed)>> = true;

struct EditorHarness
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor;
    SessionSettings settings;
    ConnectionLifecycleSnapshot lifecycle;
    FamaLamaJamAudioProcessorEditor::TransportUiState transport;
    std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> mixerStrips;
    int transmitToggleCount { 0 };
    int voiceToggleCount { 0 };
    bool localVoiceModeEnabled { false };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness()
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(ConnectionLifecycleSnapshot { .state = ConnectionState::Active, .statusMessage = "Connected" })
        , transport(FamaLamaJamAudioProcessorEditor::TransportUiState {
              .connected = true,
              .hasServerTiming = true,
              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
              .metronomeAvailable = true,
              .beatsPerMinute = 120,
              .beatsPerInterval = 16,
              .currentBeat = 1,
              .intervalProgress = 0.1f,
              .intervalIndex = 1,
          })
        , mixerStrips({
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
          })
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
            []() { return FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {}; },
            []() { return false; },
            [this]() { return mixerStrips; },
            [](const std::string&, float, float, bool) { return true; },
            []() { return false; },
            [](bool) {},
            []() { return FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState {}; },
            [](bool) { return false; },
            FamaLamaJamAudioProcessorEditor::RoomUiGetter {},
            FamaLamaJamAudioProcessorEditor::RoomMessageHandler {},
            FamaLamaJamAudioProcessorEditor::RoomVoteHandler {},
            FamaLamaJamAudioProcessorEditor::DiagnosticsTextGetter {},
            FamaLamaJamAudioProcessorEditor::FloatGetter {},
            FamaLamaJamAudioProcessorEditor::FloatSetter {},
            FamaLamaJamAudioProcessorEditor::FloatGetter {},
            FamaLamaJamAudioProcessorEditor::FloatSetter {},
            FamaLamaJamAudioProcessorEditor::StemCaptureUiGetter {},
            FamaLamaJamAudioProcessorEditor::StemCaptureSettingsSetter {},
            FamaLamaJamAudioProcessorEditor::StemCaptureNewRunHandler {},
            FamaLamaJamAudioProcessorEditor::MixerStripNameSetter {},
            FamaLamaJamAudioProcessorEditor::MixerStripOutputAssignmentSetter {},
            FamaLamaJamAudioProcessorEditor::LocalChannelVisibilitySetter {},
            FamaLamaJamAudioProcessorEditor::CommandHandler {},
            [this]() {
                ++transmitToggleCount;
                for (auto& strip : mixerStrips)
                {
                    if (strip.kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
                    {
                        strip.transmitState = strip.transmitState == FamaLamaJamAudioProcessorEditor::TransmitState::Disabled
                            ? FamaLamaJamAudioProcessorEditor::TransmitState::Active
                            : FamaLamaJamAudioProcessorEditor::TransmitState::Disabled;
                    }
                }
                return true;
            },
            [this]() {
                ++voiceToggleCount;
                localVoiceModeEnabled = ! localVoiceModeEnabled;
                for (auto& strip : mixerStrips)
                {
                    if (strip.kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
                    {
                        strip.localChannelMode = localVoiceModeEnabled
                            ? FamaLamaJamAudioProcessorEditor::LocalChannelMode::Voice
                            : FamaLamaJamAudioProcessorEditor::LocalChannelMode::Interval;
                    }
                }
                return true;
            });
    }
};
} // namespace

TEST_CASE("plugin transmit controls ui reserves local strip projection fields for transmit and solo",
          "[plugin_transmit_controls_ui]")
{
    CHECK(hasTransmitStateField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
    CHECK(hasSoloedField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
}

TEST_CASE("plugin transmit controls ui reserves unsupported-voice badges in normal strip state",
          "[plugin_transmit_controls_ui]")
{
    CHECK(hasUnsupportedVoiceModeField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
    CHECK(hasTransmitStateField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
    CHECK(hasVoiceModeField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
    CHECK(hasLocalChannelModeField<FamaLamaJamAudioProcessorEditor::MixerStripState>);
}

TEST_CASE("plugin transmit controls ui keeps compact transmit and mode controls on every local strip",
          "[plugin_transmit_controls_ui]")
{
    EditorHarness harness;

    auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();

    REQUIRE(stripLabels.size() == 3);
    CHECK(stripLabels[0] == "Main");
    CHECK(stripLabels[1] == "Bass");
    CHECK(stripLabels[2] == "alice - guitar");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId) == "TX");
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId) == "INT");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId) == "TX");
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId) == "INT");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting("alice#0").isEmpty());
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting("alice#0").isEmpty());

    bool mainVoiceEnabled = false;
    bool sendVoiceEnabled = false;
    CHECK(harness.editor->getMixerStripVoiceToggleStateForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId,
                                                                  mainVoiceEnabled));
    CHECK(harness.editor->getMixerStripVoiceToggleStateForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId,
                                                                  sendVoiceEnabled));
    CHECK_FALSE(mainVoiceEnabled);
    CHECK_FALSE(sendVoiceEnabled);
    CHECK(harness.editor->clickMixerStripTransmitForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId));
    CHECK(harness.editor->clickMixerStripVoiceToggleForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId));
    CHECK(harness.transmitToggleCount == 1);
    CHECK(harness.voiceToggleCount == 1);
}
