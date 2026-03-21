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
    std::string lastSentMessage;
    FamaLamaJamAudioProcessorEditor::RoomVoteKind lastSubmittedVoteKind {
        FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm
    };
    int lastSubmittedVoteValue { 0 };
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
                  {
                      .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::VoteSystem,
                      .author = "",
                      .text = "[voting system] BPM vote passed: 124",
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
            []() { return FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState {}; },
            [](bool) { return false; },
            [this]() { return roomUi; },
            [this](std::string text) {
                ++sentMessageCount;
                lastSentMessage = std::move(text);
                return true;
            },
            [this](FamaLamaJamAudioProcessorEditor::RoomVoteKind kind, int value) {
                ++submittedVoteCount;
                lastSubmittedVoteKind = kind;
                lastSubmittedVoteValue = value;
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

juce::Label* findLabelContainingText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::Label>(parent,
                                      [&](const juce::Label& label) { return label.getText().contains(text); });
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
    auto* composerLabel = findLabelWithText(*harness.editor, "Message");
    auto* sendButton = findButtonWithText(*harness.editor, "Send");
    auto* bpmButton = findButtonWithText(*harness.editor, "Vote BPM");
    auto* bpiButton = findButtonWithText(*harness.editor, "Vote BPI");

    REQUIRE(roomLabel != nullptr);
    REQUIRE(composerLabel != nullptr);
    REQUIRE(sendButton != nullptr);
    REQUIRE(bpmButton != nullptr);
    REQUIRE(bpiButton != nullptr);

    CHECK(roomLabel->isVisible());
    CHECK(composerLabel->isVisible());
    CHECK(sendButton->isVisible());
    CHECK(bpmButton->isVisible());
    CHECK(bpiButton->isVisible());
    CHECK(harness.editor->hasRoomFeedViewportForTesting());
    REQUIRE(harness.editor->getVisibleRoomFeedForTesting().size() == 4);
    CHECK(harness.editor->getVisibleRoomFeedForTesting()[0].kind
          == FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic);
    CHECK(harness.editor->getVisibleRoomFeedForTesting()[1].subdued);
    CHECK(harness.editor->getVisibleRoomFeedForTesting()[3].kind
          == FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::VoteSystem);
}

TEST_CASE("plugin room controls ui keeps the right-hand room sidebar visible while disconnected",
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

    auto* connectedHostLabel = findLabelWithText(*connectedHarness.editor, "Host");
    auto* connectedRoomLabel = findLabelWithText(*connectedHarness.editor, "Room");
    auto* connectedComposerLabel = findLabelWithText(*connectedHarness.editor, "Message");
    auto* connectedSendButton = findButtonWithText(*connectedHarness.editor, "Send");
    auto* connectedBpmButton = findButtonWithText(*connectedHarness.editor, "Vote BPM");
    auto* connectedBpiButton = findButtonWithText(*connectedHarness.editor, "Vote BPI");
    auto* disconnectedHostLabel = findLabelWithText(*disconnectedHarness.editor, "Host");
    auto* disconnectedRoomLabel = findLabelWithText(*disconnectedHarness.editor, "Room");
    auto* disconnectedComposerLabel = findLabelWithText(*disconnectedHarness.editor, "Message");
    auto* disconnectedBpmButton = findButtonWithText(*disconnectedHarness.editor, "Vote BPM");
    auto* disconnectedBpiButton = findButtonWithText(*disconnectedHarness.editor, "Vote BPI");

    REQUIRE(connectedHostLabel != nullptr);
    REQUIRE(connectedRoomLabel != nullptr);
    REQUIRE(connectedComposerLabel != nullptr);
    REQUIRE(connectedSendButton != nullptr);
    REQUIRE(connectedBpmButton != nullptr);
    REQUIRE(connectedBpiButton != nullptr);
    REQUIRE(disconnectedHostLabel != nullptr);
    REQUIRE(disconnectedRoomLabel != nullptr);
    REQUIRE(disconnectedComposerLabel != nullptr);
    REQUIRE(disconnectedBpmButton != nullptr);
    REQUIRE(disconnectedBpiButton != nullptr);

    CHECK(connectedRoomLabel->getX() > connectedHostLabel->getRight());
    CHECK(connectedComposerLabel->isVisible());
    CHECK(connectedSendButton->isVisible());
    CHECK(connectedBpmButton->isVisible());
    CHECK(connectedBpiButton->isVisible());
    CHECK(disconnectedRoomLabel->isVisible());
    CHECK(disconnectedComposerLabel->isVisible());
    CHECK(disconnectedBpmButton->isVisible());
    CHECK(disconnectedBpiButton->isVisible());
    CHECK(disconnectedRoomLabel->getX() > disconnectedHostLabel->getRight());
    CHECK(connectedHarness.editor->isRoomComposerEnabledForTesting());
    CHECK_FALSE(disconnectedHarness.editor->isRoomComposerEnabledForTesting());
    CHECK_FALSE(disconnectedHarness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm));
    CHECK_FALSE(disconnectedHarness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi));
    CHECK(disconnectedHarness.editor->getRoomStatusTextForTesting().isEmpty());
    CHECK(disconnectedHarness.editor->hasRoomFeedViewportForTesting());

    connectedHarness.editor->setRoomComposerTextForTesting("Keep the bridge tight.");
    REQUIRE(connectedHarness.editor->submitRoomComposerForTesting(false));
    CHECK(connectedHarness.sentMessageCount == 1);
    CHECK(connectedHarness.lastSentMessage == "Keep the bridge tight.");
    CHECK(connectedHarness.editor->getRoomComposerTextForTesting().isEmpty());

    connectedHarness.editor->setRoomComposerTextForTesting("Bring the drums in on beat five.");
    REQUIRE(connectedHarness.editor->submitRoomComposerForTesting(true));
    CHECK(connectedHarness.sentMessageCount == 2);
    CHECK(connectedHarness.lastSentMessage == "Bring the drums in on beat five.");
}

TEST_CASE("plugin room controls ui keeps compact vote controls near the top of the sidebar",
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

    auto* hostLabel = findLabelWithText(*harness.editor, "Host");
    auto* roomLabel = findLabelWithText(*harness.editor, "Room");
    auto* bpmButton = findButtonWithText(*harness.editor, "Vote BPM");
    auto* bpiButton = findButtonWithText(*harness.editor, "Vote BPI");
    auto* roomStatusLabel = findLabelContainingText(*harness.editor, "Room activity");
    auto* pendingLabel = findLabelWithText(*harness.editor, "BPM vote pending");
    auto* failureLabel = findLabelWithText(*harness.editor, "BPI vote failed");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(roomLabel != nullptr);
    REQUIRE(bpmButton != nullptr);
    REQUIRE(bpiButton != nullptr);
    REQUIRE(roomStatusLabel != nullptr);
    REQUIRE(pendingLabel != nullptr);
    REQUIRE(failureLabel != nullptr);

    CHECK(roomLabel->getX() > hostLabel->getRight());
    CHECK(bpmButton->getX() > hostLabel->getRight());
    CHECK(bpiButton->getX() > hostLabel->getRight());
    CHECK(bpmButton->getY() < roomStatusLabel->getY());
    CHECK(bpiButton->getY() < roomStatusLabel->getY());
    CHECK(bpmButton->getBottom() < pendingLabel->getY());
    CHECK(bpiButton->getBottom() < failureLabel->getY());
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
          == "BPM vote pending");
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "BPI vote failed");
    CHECK(harness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm));
    CHECK(harness.editor->isRoomVoteEnabledForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi));
}

TEST_CASE("plugin room controls ui submits validated direct votes and clears inline feedback after room updates",
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

    harness.roomUi.bpmVote = {};
    harness.roomUi.bpiVote = {};
    harness.editor->refreshForTesting();

    harness.editor->setRoomVoteValueForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm, 124);
    REQUIRE(harness.editor->submitRoomVoteForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm));
    CHECK(harness.submittedVoteCount == 1);
    CHECK(harness.lastSubmittedVoteKind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm);
    CHECK(harness.lastSubmittedVoteValue == 124);

    harness.roomUi.bpmVote = {
        .pending = true,
        .requestedValue = 124,
        .statusText = "BPM vote pending",
    };
    harness.editor->refreshForTesting();
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
          == "BPM vote pending");

    harness.roomUi.bpmVote = {
        .requestedValue = 124,
        .statusText = "Room BPM updated",
    };
    harness.editor->refreshForTesting();
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
          == "Vote BPM 40-400");

    harness.editor->setRoomVoteValueForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi, 99);
    CHECK_FALSE(harness.editor->submitRoomVoteForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi));
    CHECK(harness.submittedVoteCount == 1);
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "Enter a BPI between 2 and 64.");

    harness.editor->setRoomVoteValueForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi, 12);
    REQUIRE(harness.editor->submitRoomVoteForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi));
    CHECK(harness.submittedVoteCount == 2);
    CHECK(harness.lastSubmittedVoteKind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi);
    CHECK(harness.lastSubmittedVoteValue == 12);

    harness.roomUi.bpiVote = {
        .failed = true,
        .requestedValue = 12,
        .statusText = "BPI vote failed",
    };
    harness.editor->refreshForTesting();
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "BPI vote failed");

    for (int index = 0; index < 24; ++index)
        harness.editor->refreshForTesting();

    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "Vote BPI 2-64");
}

TEST_CASE("plugin room controls ui places the room workflow in a fixed right sidebar instead of above the mixer",
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

    auto* hostLabel = findLabelWithText(*harness.editor, "Host");
    auto* roomLabel = findLabelWithText(*harness.editor, "Room");
    auto* mixerLabel = findLabelWithText(*harness.editor, "Mixer");

    REQUIRE(hostLabel != nullptr);
    REQUIRE(roomLabel != nullptr);
    REQUIRE(mixerLabel != nullptr);

    CHECK(roomLabel->getX() > hostLabel->getRight());
    CHECK(roomLabel->getX() > mixerLabel->getX());
    CHECK_FALSE(harness.editor->isRoomSectionAboveMixerForTesting());
    CHECK(findButtonWithText(*harness.editor, "Room Tab") == nullptr);
    CHECK(findButtonWithText(*harness.editor, "Open Chat") == nullptr);
}
