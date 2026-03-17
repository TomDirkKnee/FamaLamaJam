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
    FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState hostSyncAssist;
    FamaLamaJamAudioProcessorEditor::RoomUiState roomUi;
    std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> mixerStrips;
    bool metronomeEnabled { false };
    int sentMessageCount { 0 };
    int submittedVoteCount { 0 };
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness(ConnectionLifecycleSnapshot lifecycleSnapshot,
                  FamaLamaJamAudioProcessorEditor::TransportUiState transportState)
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(std::move(lifecycleSnapshot))
        , transport(std::move(transportState))
        , roomUi({
              .connected = transport.connected,
              .topic = "Tonight: lock the groove before the chorus.",
              .visibleFeed = {
                  {
                      .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic,
                      .author = "",
                      .text = "Tonight: lock the groove before the chorus.",
                  },
                  {
                      .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Presence,
                      .author = "alice",
                      .text = "joined the room",
                      .subdued = true,
                  },
                  {
                      .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat,
                      .author = "bob",
                      .text = "Let's keep the verse lighter.",
                  },
              },
              .bpmVote = {
                  .pending = true,
                  .requestedValue = 124,
                  .statusText = "BPM vote pending",
              },
              .bpiVote = {
                  .failed = true,
                  .requestedValue = 12,
                  .statusText = "BPI vote failed",
              },
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
            [this]() { return roomUi; },
            [this](std::string) {
                ++sentMessageCount;
                return true;
            },
            [this](FamaLamaJamAudioProcessorEditor::RoomVoteKind, int) {
                ++submittedVoteCount;
                return true;
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

juce::TextButton* findButtonWithText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::TextButton>(parent,
                                           [&](const juce::TextButton& button) { return button.getButtonText() == text; });
}
} // namespace

TEST_CASE("plugin room controls ui exposes one mixed room section in the current page", "[plugin_room_controls_ui]")
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
                              .currentBeat = 4,
                              .intervalProgress = 0.5f,
                              .intervalIndex = 2,
                          });

    auto* roomLabel = findLabelWithText(*harness.editor, "Room");
    auto* topicLabel = findLabelWithText(*harness.editor, "Current Topic");
    auto* composerLabel = findLabelWithText(*harness.editor, "Message");
    auto* sendButton = findButtonWithText(*harness.editor, "Send");
    auto* bpmButton = findButtonWithText(*harness.editor, "Vote BPM");
    auto* bpiButton = findButtonWithText(*harness.editor, "Vote BPI");

    REQUIRE(roomLabel != nullptr);
    REQUIRE(topicLabel != nullptr);
    REQUIRE(composerLabel != nullptr);
    REQUIRE(sendButton != nullptr);
    REQUIRE(bpmButton != nullptr);
    REQUIRE(bpiButton != nullptr);

    CHECK(roomLabel->isVisible());
    CHECK(topicLabel->isVisible());
    CHECK(composerLabel->isVisible());
    CHECK(sendButton->isVisible());
    CHECK(bpmButton->isVisible());
    CHECK(bpiButton->isVisible());
    CHECK(harness.editor->getRoomTopicTextForTesting() == "Tonight: lock the groove before the chorus.");
    CHECK(harness.editor->getVisibleRoomFeedForTesting().size() == 3);
}

TEST_CASE("plugin room controls ui keeps the composer visible and makes disconnected state explicit",
          "[plugin_room_controls_ui]")
{
    EditorHarness connectedHarness(ConnectionLifecycleSnapshot {
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
                                       .currentBeat = 4,
                                       .intervalProgress = 0.5f,
                                       .intervalIndex = 2,
                                   });

    EditorHarness disconnectedHarness(ConnectionLifecycleSnapshot {
                                          .state = ConnectionState::Idle,
                                          .statusMessage = "Ready",
                                      },
                                      FamaLamaJamAudioProcessorEditor::TransportUiState {
                                          .connected = false,
                                          .hasServerTiming = false,
                                          .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected,
                                          .metronomeAvailable = false,
                                      });

    auto* connectedComposerLabel = findLabelWithText(*connectedHarness.editor, "Message");
    auto* connectedSendButton = findButtonWithText(*connectedHarness.editor, "Send");
    auto* disconnectedRoomStatus =
        findLabelWithText(*disconnectedHarness.editor, "Connect to a room to chat and vote.");

    REQUIRE(connectedComposerLabel != nullptr);
    REQUIRE(connectedSendButton != nullptr);
    REQUIRE(disconnectedRoomStatus != nullptr);

    CHECK(connectedComposerLabel->isVisible());
    CHECK(connectedSendButton->isVisible());
    CHECK(disconnectedRoomStatus->isVisible());
    CHECK(connectedHarness.editor->isRoomComposerEnabledForTesting());
    CHECK_FALSE(disconnectedHarness.editor->isRoomComposerEnabledForTesting());
    CHECK(disconnectedHarness.editor->getRoomStatusTextForTesting() == "Connect to a room to chat and vote.");
}

TEST_CASE("plugin room controls ui keeps direct numeric vote controls near the transport area",
          "[plugin_room_controls_ui]")
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
                              .currentBeat = 4,
                              .intervalProgress = 0.5f,
                              .intervalIndex = 2,
                          });

    auto* transportLabel = findLabelWithText(*harness.editor, "Beat 4/16 | 120 BPM | 50%");
    auto* bpmButton = findButtonWithText(*harness.editor, "Vote BPM");
    auto* bpiButton = findButtonWithText(*harness.editor, "Vote BPI");
    auto* pendingLabel = findLabelWithText(*harness.editor, "BPM vote pending");
    auto* failureLabel = findLabelWithText(*harness.editor, "BPI vote failed");

    REQUIRE(transportLabel != nullptr);
    REQUIRE(bpmButton != nullptr);
    REQUIRE(bpiButton != nullptr);
    REQUIRE(pendingLabel != nullptr);
    REQUIRE(failureLabel != nullptr);

    CHECK(bpmButton->getY() >= transportLabel->getY());
    CHECK(bpmButton->getBottom() < pendingLabel->getY());
    CHECK(bpiButton->getY() >= transportLabel->getY());
    CHECK(bpiButton->getBottom() < failureLabel->getY());
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
          == "BPM vote pending");
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "BPI vote failed");
    CHECK(harness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm));
    CHECK(harness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi));
}

TEST_CASE("plugin room controls ui keeps the room section above the mixer without tabs or popouts",
          "[plugin_room_controls_ui]")
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
                              .currentBeat = 4,
                              .intervalProgress = 0.5f,
                              .intervalIndex = 2,
                          });

    auto* roomLabel = findLabelWithText(*harness.editor, "Room");
    auto* mixerLabel = findLabelWithText(*harness.editor, "Mixer");

    REQUIRE(roomLabel != nullptr);
    REQUIRE(mixerLabel != nullptr);

    CHECK(roomLabel->getBottom() < mixerLabel->getY());
    CHECK(harness.editor->isRoomSectionAboveMixerForTesting());
    CHECK(findButtonWithText(*harness.editor, "Room Tab") == nullptr);
    CHECK(findButtonWithText(*harness.editor, "Open Chat") == nullptr);
}
