#include <memory>
#include <algorithm>
#include <cmath>
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
            [this](const std::string& sourceId, std::string displayName) {
                for (auto& strip : mixerStrips)
                {
                    if (strip.sourceId == sourceId)
                    {
                        strip.displayName = std::move(displayName);
                        return true;
                    }
                }
                return false;
            },
            [](const std::string&, int) { return true; },
            [this](const std::string& sourceId, bool visible) {
                for (auto& strip : mixerStrips)
                {
                    if (strip.sourceId == sourceId)
                    {
                        strip.visible = visible;
                        strip.active = visible;
                        return true;
                    }
                }
                return false;
            },
            [this]() {
                for (auto& strip : mixerStrips)
                {
                    if (strip.kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor && ! strip.visible)
                    {
                        strip.visible = true;
                        strip.active = true;
                        return true;
                    }
                }
                return false;
            });
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

juce::Button* findButtonWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::Button>(parent, [&](const juce::Button& button) { return button.getButtonText() == text; });
}

juce::TextEditor* findTextEditorWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::TextEditor>(parent, [&](const juce::TextEditor& editor) { return editor.getText() == text; });
}

juce::Rectangle<int> getBoundsInEditor(juce::Component& editor, juce::Component& component)
{
    return editor.getLocalArea(&component, component.getLocalBounds());
}

template <typename ComponentType>
void collectVisibleComponents(juce::Component& parent, std::vector<ComponentType*>& matches)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto& child = *parent.getChildComponent(index);
        if (auto* typed = dynamic_cast<ComponentType*>(&child); typed != nullptr && typed->isVisible())
            matches.push_back(typed);

        collectVisibleComponents<ComponentType>(child, matches);
    }
}

template <typename ComponentType>
std::vector<ComponentType*> findVisibleComponents(juce::Component& parent)
{
    std::vector<ComponentType*> matches;
    collectVisibleComponents<ComponentType>(parent, matches);
    return matches;
}
} // namespace

TEST_CASE("plugin mixer ui keeps locals first in one horizontal strip field with remote groups beside them",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
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
          .active = true,
          .visible = true },
    });

    const auto groupLabels = harness.editor->getVisibleMixerGroupLabelsForTesting();
    REQUIRE(groupLabels.size() == 3);
    CHECK(groupLabels[0] == FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle);
    CHECK(groupLabels[1] == "alice");
    CHECK(groupLabels[2] == "bob");

    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 5);
    CHECK(stripLabels[0] == "Main");
    CHECK(stripLabels[1] == "Bass");
    CHECK(stripLabels[2] == "alice - guitar");
    CHECK(stripLabels[3] == "alice - vocal");
    CHECK(stripLabels[4] == "bob - bass");

    auto* localHeaderLabel = findLabelWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle);
    auto* localMainEditor = findTextEditorWithText(*harness.editor, "Main");
    auto* localBassEditor = findTextEditorWithText(*harness.editor, "Bass");
    auto* aliceGroupLabel = findLabelWithText(*harness.editor, "alice");
    auto* bobGroupLabel = findLabelWithText(*harness.editor, "bob");

    REQUIRE(localHeaderLabel != nullptr);
    REQUIRE(localMainEditor != nullptr);
    REQUIRE(localBassEditor != nullptr);
    REQUIRE(aliceGroupLabel != nullptr);
    REQUIRE(bobGroupLabel != nullptr);

    const auto localHeaderBounds = getBoundsInEditor(*harness.editor, *localHeaderLabel);
    const auto localMainBounds = getBoundsInEditor(*harness.editor, *localMainEditor);
    const auto localBassBounds = getBoundsInEditor(*harness.editor, *localBassEditor);
    const auto aliceGroupBounds = getBoundsInEditor(*harness.editor, *aliceGroupLabel);
    const auto bobGroupBounds = getBoundsInEditor(*harness.editor, *bobGroupLabel);

    CHECK(localMainBounds.getX() >= localHeaderBounds.getX());
    CHECK(localBassBounds.getX() > localMainBounds.getRight());
    CHECK(aliceGroupBounds.getX() > localBassBounds.getRight());
    CHECK(bobGroupBounds.getX() > aliceGroupBounds.getRight());
    CHECK(std::abs(aliceGroupBounds.getY() - localHeaderBounds.getY()) <= 24);
    CHECK(std::abs(bobGroupBounds.getY() - aliceGroupBounds.getY()) <= 16);
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

TEST_CASE("plugin mixer ui keeps remote voice peers in the normal mixer with orange explanatory status",
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
          .sourceId = "voice-user#1",
          .groupId = "voice-user",
          .groupLabel = "voice-user",
          .displayName = "voice-user - voice",
          .subtitle = "Voice chat",
          .voiceMode = true,
          .statusText = "Receiving voice chat audio",
          .active = true,
          .visible = true },
    });

    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(stripLabels.size() == 2);
    CHECK(stripLabels[1] == "voice-user - voice");
    CHECK(harness.editor->getMixerStripStatusTextForTesting("voice-user#1") == "Receiving voice chat audio");
    CHECK(harness.editor->getMixerStripStatusColourForTesting("voice-user#1")
          == juce::Colour::fromRGB(230, 181, 120));
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting("voice-user#1").isEmpty());
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting("voice-user#1").isEmpty());
}

TEST_CASE("plugin mixer ui keeps add remove and collapse on the local group header instead of header transmit voice toggles",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Main",
          .subtitle = "Live monitor",
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Bass",
          .subtitle = "Local Send 2",
          .active = true,
          .visible = true,
          .editableName = true },
    });

    auto* localHeaderLabel = findLabelWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle);
    auto* headerRemoveButton = findButtonWithText(*harness.editor, "-");
    auto* headerAddButton = findButtonWithText(*harness.editor, "+");
    auto* headerCollapseButton = findButtonWithText(*harness.editor, "Collapse");

    REQUIRE(localHeaderLabel != nullptr);
    REQUIRE(headerRemoveButton != nullptr);
    REQUIRE(headerAddButton != nullptr);
    REQUIRE(headerCollapseButton != nullptr);
    REQUIRE(findTextEditorWithText(*harness.editor, "Bass") != nullptr);

    CHECK(findToggleWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kLocalHeaderTransmitLabel) == nullptr);
    CHECK(findToggleWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kLocalHeaderVoiceLabel) == nullptr);
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId) == "TX");
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId) == "INT");
    CHECK(harness.editor->getMixerStripTransmitButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId) == "TX");
    CHECK(harness.editor->getMixerStripVoiceButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId) == "INT");
    CHECK(harness.editor->getMixerStripRemoveButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId).isEmpty());
    CHECK(harness.editor->getMixerStripRemoveButtonTextForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId).isEmpty());

    const auto localHeaderBounds = getBoundsInEditor(*harness.editor, *localHeaderLabel);
    const auto removeBounds = getBoundsInEditor(*harness.editor, *headerRemoveButton);
    const auto addBounds = getBoundsInEditor(*harness.editor, *headerAddButton);
    const auto collapseBounds = getBoundsInEditor(*harness.editor, *headerCollapseButton);

    CHECK(removeBounds.getCentreY() >= localHeaderBounds.getY());
    CHECK(removeBounds.getCentreY() <= localHeaderBounds.getBottom());
    CHECK(removeBounds.getWidth() <= 20);
    CHECK(addBounds.getX() > removeBounds.getRight());
    CHECK(addBounds.getWidth() <= 20);
    CHECK(collapseBounds.getX() > addBounds.getRight());
    CHECK(collapseBounds.getWidth() <= 56);
}

TEST_CASE("plugin mixer ui reveals the next hidden local slot and removes it from the local header without losing prior state",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Main",
          .subtitle = "Live monitor",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Bass",
          .subtitle = "Local Send 2",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
          .active = false,
          .visible = false,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend3SourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Keys",
          .subtitle = "Local Send 3",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
          .active = false,
          .visible = false,
          .editableName = true },
    });

    auto* addButton = findButtonWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kAddLocalChannelLabel);
    auto* removeButton = findButtonWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kRemoveLocalChannelLabel);
    REQUIRE(addButton != nullptr);
    REQUIRE(removeButton != nullptr);
    REQUIRE(addButton->onClick != nullptr);
    addButton->onClick();

    auto visibleStrips = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(visibleStrips.size() == 2);
    CHECK(visibleStrips[1] == "Bass");

    REQUIRE(harness.editor->clickMixerStripRemoveForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId));
    visibleStrips = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(visibleStrips.size() == 1);
    CHECK(visibleStrips[0] == "Main");

    addButton->onClick();
    visibleStrips = harness.editor->getVisibleMixerStripLabelsForTesting();
    REQUIRE(visibleStrips.size() == 2);
    CHECK(visibleStrips[1] == "Bass");
}

TEST_CASE("plugin mixer ui expects inline remote output routing choices on remote strips", "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = "local",
          .groupLabel = "Local Sends",
          .displayName = "Main",
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
          .visible = true,
          .outputAssignmentIndex = 2,
          .outputAssignmentLabels = {
              FamaLamaJamAudioProcessorEditor::kMainOutputLabel,
              "Remote Out 1",
              "Remote Out 2",
              "Remote Out 3",
              "Remote Out 4",
              "Remote Out 5",
              "Remote Out 6",
              "Remote Out 7",
          } },
    });

    auto* outputSelector = findComponent<juce::ComboBox>(*harness.editor, [](const juce::ComboBox& comboBox) {
        for (int itemIndex = 1; itemIndex <= comboBox.getNumItems(); ++itemIndex)
        {
            if (comboBox.getItemText(itemIndex) == "Remote Out 2")
                return true;
        }

        return false;
    });

    REQUIRE(outputSelector != nullptr);
    CHECK(outputSelector->getNumItems() == 8);
    CHECK(outputSelector->getItemText(0) == FamaLamaJamAudioProcessorEditor::kMainOutputLabel);
    CHECK(outputSelector->getItemText(1) == "Remote Out 1");
    CHECK(outputSelector->getItemText(2) == "Remote Out 2");
    CHECK(outputSelector->getItemText(7) == "Remote Out 7");

    const auto selectorBounds = getBoundsInEditor(*harness.editor, *outputSelector);
    CHECK(selectorBounds.getWidth() <= 40);
    CHECK(selectorBounds.getHeight() <= 38);
}

TEST_CASE("plugin mixer ui expects narrow strip anatomy with vertical faders and compact pan pots",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = "local",
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Main",
          .subtitle = "Live monitor",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
          .groupId = "local",
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Bass",
          .subtitle = "Local Send 2",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::WarmingUp,
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
          .visible = true,
          .outputAssignmentIndex = 0,
          .outputAssignmentLabels = { FamaLamaJamAudioProcessorEditor::kMainOutputLabel, "Remote Out 1" } },
    });

    const auto sliders = findVisibleComponents<juce::Slider>(*harness.editor);
    int verticalStripSliderCount = 0;
    int rotaryPanPotCount = 0;
    for (const auto* slider : sliders)
    {
        if (slider->getSliderStyle() == juce::Slider::LinearVertical)
            ++verticalStripSliderCount;
        if (slider->getSliderStyle() == juce::Slider::RotaryVerticalDrag)
            ++rotaryPanPotCount;
    }

    CHECK(verticalStripSliderCount == 3);
    CHECK(rotaryPanPotCount == 3);
}

TEST_CASE("plugin mixer ui expects taller Ableton-style strips with an integrated meter-fader spine and larger pan pots",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Main",
          .subtitle = "Live monitor",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
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
          .visible = true,
          .outputAssignmentIndex = 0,
          .outputAssignmentLabels = { FamaLamaJamAudioProcessorEditor::kMainOutputLabel, "Remote Out 1" } },
    });

    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting mainLayout;
    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting remoteLayout;
    REQUIRE(harness.editor->getMixerStripLayoutSnapshotForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId,
                                                                  mainLayout));
    REQUIRE(harness.editor->getMixerStripLayoutSnapshotForTesting("alice#0", remoteLayout));

    const auto mixerViewportBounds = harness.editor->getMixerViewportBoundsForTesting();
    CHECK(mainLayout.stripBounds.getHeight() >= (mixerViewportBounds.getHeight() * 2) / 3);
    CHECK(remoteLayout.stripBounds.getHeight() >= (mixerViewportBounds.getHeight() * 2) / 3);

    CHECK(mainLayout.gainBounds.contains(mainLayout.meterBounds.getCentre()));
    CHECK(remoteLayout.gainBounds.contains(remoteLayout.meterBounds.getCentre()));

    CHECK(mainLayout.panBounds.getWidth() >= 40);
    CHECK(mainLayout.panBounds.getHeight() >= 40);
    CHECK(remoteLayout.panBounds.getWidth() >= 40);
    CHECK(remoteLayout.panBounds.getHeight() >= 40);
}

TEST_CASE("plugin mixer ui expects compact local M S TX and INT controls to hug the strip spine",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Main",
          .subtitle = "Live monitor",
          .transmitState = FamaLamaJamAudioProcessorEditor::TransmitState::Active,
          .active = true,
          .visible = true,
          .editableName = true },
    });

    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting layout;
    REQUIRE(harness.editor->getMixerStripLayoutSnapshotForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId,
                                                                  layout));

    CHECK(layout.muteBounds.getWidth() <= 28);
    CHECK(layout.soloBounds.getWidth() <= 28);
    CHECK(layout.transmitBounds.getWidth() <= 28);
    CHECK(layout.voiceBounds.getWidth() <= 28);

    CHECK(layout.muteBounds.getX() - layout.gainBounds.getRight() <= 14);
    CHECK(layout.soloBounds.getX() - layout.gainBounds.getRight() <= 14);
    CHECK(layout.transmitBounds.getX() - layout.gainBounds.getRight() <= 14);
    CHECK(layout.voiceBounds.getX() - layout.gainBounds.getRight() <= 14);
}

TEST_CASE("plugin mixer ui collapses locals into visible mini strips without holding remote groups open",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = "local",
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Main",
          .subtitle = "Live monitor",
          .meterLeft = 0.48f,
          .meterRight = 0.44f,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
          .groupId = "local",
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Bass",
          .subtitle = "Local Send 2",
          .meterLeft = 0.31f,
          .meterRight = 0.28f,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
          .sourceId = "alice#0",
          .groupId = "alice",
          .groupLabel = "alice",
          .displayName = "alice - guitar",
          .subtitle = "guitar",
          .meterLeft = 0.65f,
          .meterRight = 0.61f,
          .active = true,
          .visible = true },
    });

    auto* collapseButton = findButtonWithText(*harness.editor, "Collapse");
    auto* aliceGroupLabel = findLabelWithText(*harness.editor, "alice");

    REQUIRE(collapseButton != nullptr);
    REQUIRE(collapseButton->onClick != nullptr);
    REQUIRE(aliceGroupLabel != nullptr);

    const auto aliceBoundsBefore = getBoundsInEditor(*harness.editor, *aliceGroupLabel);
    collapseButton->onClick();

    auto* localMainEditor = findTextEditorWithText(*harness.editor, "Main");
    auto* localBassEditor = findTextEditorWithText(*harness.editor, "Bass");
    REQUIRE(localMainEditor != nullptr);
    REQUIRE(localBassEditor != nullptr);

    CHECK_FALSE(localMainEditor->isVisible());
    CHECK_FALSE(localBassEditor->isVisible());

    const auto aliceBoundsAfter = getBoundsInEditor(*harness.editor, *aliceGroupLabel);
    CHECK(aliceBoundsAfter.getX() < aliceBoundsBefore.getX());

    const auto visibleMeters = findVisibleComponents<famalamajam::plugin::StereoMeterComponent>(*harness.editor);
    int localMeterCount = 0;
    int tallestLocalMeter = 0;
    for (auto* meter : visibleMeters)
    {
        const auto meterBounds = getBoundsInEditor(*harness.editor, *meter);
        if (meterBounds.getRight() < aliceBoundsAfter.getX())
        {
            ++localMeterCount;
            tallestLocalMeter = std::max(tallestLocalMeter, meterBounds.getHeight());
            CHECK(meterBounds.getWidth() <= 32);
        }
    }

    CHECK(localMeterCount == 2);
    CHECK(tallestLocalMeter >= 192);
}

TEST_CASE("plugin mixer ui collapses locals into tighter mini strips while preserving + and - header affordances",
          "[plugin_mixer_ui]")
{
    EditorHarness harness({
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalMainSourceId,
          .groupId = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Main",
          .subtitle = "Live monitor",
          .meterLeft = 0.48f,
          .meterRight = 0.44f,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor,
          .sourceId = FamaLamaJamAudioProcessor::kLocalSend2SourceId,
          .groupId = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .groupLabel = FamaLamaJamAudioProcessorEditor::kLocalHeaderTitle,
          .displayName = "Bass",
          .subtitle = "Local Send 2",
          .meterLeft = 0.31f,
          .meterRight = 0.28f,
          .active = true,
          .visible = true,
          .editableName = true },
        { .kind = FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
          .sourceId = "alice#0",
          .groupId = "alice",
          .groupLabel = "alice",
          .displayName = "alice - guitar",
          .subtitle = "guitar",
          .meterLeft = 0.65f,
          .meterRight = 0.61f,
          .active = true,
          .visible = true },
    });

    auto* collapseButton = findButtonWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kCollapseLocalChannelLabel);
    auto* addButton = findButtonWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kAddLocalChannelLabel);
    auto* removeButton = findButtonWithText(*harness.editor, FamaLamaJamAudioProcessorEditor::kRemoveLocalChannelLabel);
    auto* aliceGroupLabel = findLabelWithText(*harness.editor, "alice");
    REQUIRE(collapseButton != nullptr);
    REQUIRE(addButton != nullptr);
    REQUIRE(removeButton != nullptr);
    REQUIRE(aliceGroupLabel != nullptr);
    REQUIRE(collapseButton->onClick != nullptr);

    const auto aliceBoundsBefore = getBoundsInEditor(*harness.editor, *aliceGroupLabel);
    collapseButton->onClick();

    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting mainLayout;
    FamaLamaJamAudioProcessorEditor::MixerStripLayoutSnapshotForTesting sendLayout;
    REQUIRE(harness.editor->getMixerStripLayoutSnapshotForTesting(FamaLamaJamAudioProcessor::kLocalMainSourceId,
                                                                  mainLayout));
    REQUIRE(harness.editor->getMixerStripLayoutSnapshotForTesting(FamaLamaJamAudioProcessor::kLocalSend2SourceId,
                                                                  sendLayout));

    const auto headerLayout = harness.editor->getLocalHeaderLayoutSnapshotForTesting();
    const auto aliceBoundsAfter = getBoundsInEditor(*harness.editor, *aliceGroupLabel);

    CHECK(addButton->isVisible());
    CHECK(removeButton->isVisible());
    CHECK(collapseButton->isVisible());
    CHECK(collapseButton->getButtonText() == FamaLamaJamAudioProcessorEditor::kExpandLocalChannelLabel);
    CHECK(headerLayout.labelBounds.getWidth() <= 36);
    CHECK(mainLayout.stripBounds.getWidth() <= 28);
    CHECK(sendLayout.stripBounds.getWidth() <= 28);
    CHECK(mainLayout.meterBounds.getHeight() >= 192);
    CHECK(sendLayout.meterBounds.getHeight() >= 192);
    CHECK(aliceBoundsBefore.getX() - aliceBoundsAfter.getX() >= 70);
}
