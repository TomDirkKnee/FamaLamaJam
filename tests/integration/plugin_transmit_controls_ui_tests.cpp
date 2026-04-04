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

TEST_CASE("plugin transmit controls ui keeps transmit off the transport row and on the local strip",
          "[plugin_transmit_controls_ui]")
{
    EditorHarness harness;

    auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();

    REQUIRE(stripLabels.size() == 2);
    CHECK(stripLabels[0] == "Local Monitor");
    CHECK(stripLabels[1] == "alice - guitar");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting("local-monitor") == "Not transmitting");
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting("local-monitor") == "Voice Off");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting("alice#0").isEmpty());
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting("alice#0").isEmpty());
}
