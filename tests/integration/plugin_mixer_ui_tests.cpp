#include <memory>
#include <algorithm>
#include <unordered_map>

#include <catch2/catch_approx.hpp>
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

struct EditorHarness
{
    juce::ScopedJuceInitialiser_GUI gui;
    FamaLamaJamAudioProcessor processor;
    SessionSettings settings;
    ConnectionLifecycleSnapshot lifecycle;
    FamaLamaJamAudioProcessorEditor::TransportUiState transport;
    FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssist;
    std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> mixerStrips;
    bool metronomeEnabled { false };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> strips)
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(ConnectionLifecycleSnapshot { .state = ConnectionState::Active, .statusMessage = "Connected" })
        , transport(FamaLamaJamAudioProcessorEditor::TransportUiState {
              .connected = true,
              .hasServerTiming = true,
              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
              .metronomeAvailable = true,
              .beatsPerMinute = 120,
              .beatsPerInterval = 8,
              .currentBeat = 1,
              .intervalProgress = 0.1f,
              .intervalIndex = 1,
          })
        , mixerStrips(std::move(strips))
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
            []() { return false; },
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

juce::ToggleButton* findToggleWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::ToggleButton>(parent,
                                             [&](const juce::ToggleButton& toggle) { return toggle.getButtonText() == text; });
}
} // namespace

TEST_CASE("plugin mixer ui groups local and remote strips with stable order", "[plugin_mixer_ui]")
{
    EditorHarness harness({
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
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
          .sourceId = "alice#1",
          .groupId = "alice",
          .groupLabel = "alice",
          .displayName = "alice - vocal",
          .subtitle = "vocal",
          .active = true,
          .visible = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
          .sourceId = "bob#0",
          .groupId = "bob",
          .groupLabel = "bob",
          .displayName = "bob - bass",
          .subtitle = "bass",
          .active = false,
          .visible = true },
    });

    const auto groupLabels = harness.editor->getVisibleMixerGroupLabelsForTesting();
    REQUIRE(groupLabels.size() == 2);
    CHECK(groupLabels[0] == "alice");
    CHECK(groupLabels[1] == "bob");

    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 4);
    CHECK(stripLabels[0] == "Local Monitor");
    CHECK(stripLabels[1] == "alice - guitar");
    CHECK(stripLabels[2] == "alice - vocal");
    CHECK(stripLabels[3] == "bob - bass");

    auto* mixerSectionLabel = findLabelWithText(*harness.editor, "Mixer");
    auto* masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");

    REQUIRE(mixerSectionLabel != nullptr);
    REQUIRE(masterOutputLabel != nullptr);
    CHECK(masterOutputLabel->getY() > mixerSectionLabel->getY());
    CHECK(std::none_of(stripLabels.begin(),
                       stripLabels.end(),
                       [](const juce::String& label) { return label == "Master Output"; }));
}

TEST_CASE("plugin mixer ui writes strip controls back into processor-owned state and reflects meters",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = "local-monitor",
          .groupId = "local",
          .groupLabel = "Local Monitor",
          .displayName = "Local Monitor",
          .subtitle = "Live monitor",
          .gainDb = -2.0f,
          .pan = 0.1f,
          .meterLeft = 0.4f,
          .meterRight = 0.2f,
          .active = true,
          .visible = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
          .sourceId = "alice#0",
          .groupId = "alice",
          .groupLabel = "alice",
          .displayName = "alice - guitar",
          .subtitle = "guitar",
          .gainDb = -4.0f,
          .pan = -0.2f,
          .meterLeft = 0.6f,
          .meterRight = 0.5f,
          .active = true,
          .visible = true },
    });

    double gain = 0.0;
    double pan = 0.0;
    bool muted = false;
    REQUIRE(harness.editor->getMixerStripControlStateForTesting("alice#0", gain, pan, muted));
    CHECK(gain == Catch::Approx(-4.0));
    CHECK(pan == Catch::Approx(-0.2));
    CHECK_FALSE(muted);

    REQUIRE(harness.editor->setMixerStripControlStateForTesting("alice#0", -9.0, 0.75, true));
    REQUIRE(harness.editor->getMixerStripControlStateForTesting("alice#0", gain, pan, muted));
    CHECK(gain == Catch::Approx(-9.0));
    CHECK(pan == Catch::Approx(0.75));
    CHECK(muted);
}

TEST_CASE("plugin mixer ui respects hidden strips and reflects restored strip state when they reappear",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
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
          .gainDb = -12.0f,
          .pan = 0.3f,
          .muted = true,
          .active = false,
          .visible = false },
    });

    auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 1);
    CHECK(stripLabels[0] == "Local Monitor");

    harness.mixerStrips[1].visible = true;
    harness.mixerStrips[1].active = true;
    harness.editor->refreshForTesting();

    double gain = 0.0;
    double pan = 0.0;
    bool muted = false;
    REQUIRE(harness.editor->getMixerStripControlStateForTesting("alice#0", gain, pan, muted));
    CHECK(gain == Catch::Approx(-12.0));
    CHECK(pan == Catch::Approx(0.3));
    CHECK(muted);
}

TEST_CASE("plugin mixer ui removes dead default-strip controls and keeps local monitor distinct from the footer output",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
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

    CHECK(findLabelWithText(*harness.editor, "Default Gain (dB)") == nullptr);
    CHECK(findLabelWithText(*harness.editor, "Default Pan") == nullptr);
    CHECK(findToggleWithText(*harness.editor, "Default Muted") == nullptr);

    auto* masterOutputLabel = findLabelWithText(*harness.editor, "Master Output");
    REQUIRE(masterOutputLabel != nullptr);

    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 2);
    CHECK(stripLabels[0] == "Local Monitor");
    CHECK(stripLabels[1] == "alice - guitar");
    CHECK(std::none_of(stripLabels.begin(),
                       stripLabels.end(),
                       [](const juce::String& label) { return label == "Master Output"; }));
}
