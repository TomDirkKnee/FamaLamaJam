#include <memory>
#include <cmath>

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
    std::string diagnosticsText { "Diagnostics CPU\nresized=12\nroomRefresh=7" };
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
            },
            [this]() { return diagnosticsText; });
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

juce::TextButton* findButtonContainingText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::TextButton>(parent, [&](const juce::TextButton& button) {
        return button.getButtonText().contains(text);
    });
}

juce::Label* findLabelContainingText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::Label>(parent,
                                      [&](const juce::Label& label) { return label.getText().contains(text); });
}

juce::TextEditor* findTextEditorOnSameRowAs(juce::Component& parent, const juce::Label& label)
{
    return findComponent<juce::TextEditor>(parent, [&](const juce::TextEditor& editor) {
        return editor.isVisible()
            && editor.getX() >= label.getRight()
            && editor.getBounds().getCentreY() >= label.getY()
            && editor.getBounds().getCentreY() <= label.getBottom();
    });
}

juce::TextEditor* findTextEditorContainingText(juce::Component& parent, const juce::String& text)
{
    return findComponent<juce::TextEditor>(parent,
                                           [&](const juce::TextEditor& editor) { return editor.getText().contains(text); });
}

juce::Rectangle<int> getBoundsInEditor(juce::Component& editor, juce::Component& component)
{
    return editor.getLocalArea(&component, component.getLocalBounds());
}

juce::Viewport* findRoomSidebarViewport(juce::Component& parent)
{
    return findComponent<juce::Viewport>(parent, [&](const juce::Viewport& viewport) {
        return viewport.getX() > parent.getWidth() / 2;
    });
}

std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry> makeScrollableFeed()
{
    std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry> entries;
    for (int index = 0; index < 18; ++index)
    {
        entries.push_back({
            .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat,
            .author = "player" + std::to_string(index),
            .text = "Long wrapped room note " + std::to_string(index)
                + " keeps the sidebar busy enough to require scrolling in the compact room chat view.",
        });
    }

    return entries;
}

int getViewportMaxScrollY(const juce::Viewport& viewport)
{
    const auto* viewed = viewport.getViewedComponent();
    if (viewed == nullptr)
        return 0;

    return juce::jmax(0, viewed->getHeight() - viewport.getHeight());
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

    auto* roomLabel = findLabelWithText(*harness.editor, "Room Chat");
    auto* composerLabel = findLabelWithText(*harness.editor, "Message");
    auto* sendButton = findButtonWithText(*harness.editor, "Send");
    auto* voteButton = findButtonWithText(*harness.editor, "Vote");
    auto* bpmButton = findButtonWithText(*harness.editor, "Vote BPM");
    auto* bpiButton = findButtonWithText(*harness.editor, "Vote BPI");

    REQUIRE(roomLabel != nullptr);
    REQUIRE(composerLabel != nullptr);
    REQUIRE(sendButton != nullptr);
    REQUIRE(voteButton != nullptr);

    CHECK(roomLabel->isVisible());
    CHECK(composerLabel->isVisible());
    CHECK(sendButton->isVisible());
    CHECK(voteButton->isVisible());
    CHECK(bpmButton == nullptr);
    CHECK(bpiButton == nullptr);
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
    auto* connectedRoomLabel = findLabelWithText(*connectedHarness.editor, "Room Chat");
    auto* connectedComposerLabel = findLabelWithText(*connectedHarness.editor, "Message");
    auto* connectedSendButton = findButtonWithText(*connectedHarness.editor, "Send");
    auto* connectedVoteButton = findButtonWithText(*connectedHarness.editor, "Vote");
    auto* disconnectedHostLabel = findLabelWithText(*disconnectedHarness.editor, "Host");
    auto* disconnectedRoomLabel = findLabelWithText(*disconnectedHarness.editor, "Room Chat");
    auto* disconnectedComposerLabel = findLabelWithText(*disconnectedHarness.editor, "Message");
    auto* disconnectedVoteButton = findButtonWithText(*disconnectedHarness.editor, "Vote");

    REQUIRE(connectedHostLabel != nullptr);
    REQUIRE(connectedRoomLabel != nullptr);
    REQUIRE(connectedComposerLabel != nullptr);
    REQUIRE(connectedSendButton != nullptr);
    REQUIRE(connectedVoteButton != nullptr);
    REQUIRE(disconnectedHostLabel != nullptr);
    REQUIRE(disconnectedRoomLabel != nullptr);
    REQUIRE(disconnectedComposerLabel != nullptr);
    REQUIRE(disconnectedVoteButton != nullptr);

    CHECK(connectedRoomLabel->getX() > connectedHostLabel->getRight());
    CHECK(connectedComposerLabel->isVisible());
    CHECK(connectedSendButton->isVisible());
    CHECK(connectedVoteButton->isVisible());
    CHECK(disconnectedRoomLabel->isVisible());
    CHECK(disconnectedComposerLabel->isVisible());
    CHECK(disconnectedVoteButton->isVisible());
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
    harness.roomUi.bpmVote = {};
    harness.roomUi.bpiVote = {};
    harness.editor->refreshForTesting();

    auto* hostLabel = findLabelWithText(*harness.editor, "Host");
    auto* roomLabel = findLabelWithText(*harness.editor, "Room Chat");
    auto* bpmLabel = findLabelWithText(*harness.editor, "BPM");
    auto* bpiLabel = findLabelWithText(*harness.editor, "BPI");
    auto* voteButton = findButtonWithText(*harness.editor, "Vote");
    auto* roomStatusLabel = findLabelContainingText(*harness.editor, "Room activity");
    auto* bpmEditor = bpmLabel == nullptr ? nullptr : findTextEditorOnSameRowAs(*harness.editor, *bpmLabel);
    auto* bpiEditor = bpiLabel == nullptr ? nullptr : findTextEditorOnSameRowAs(*harness.editor, *bpiLabel);

    REQUIRE(hostLabel != nullptr);
    REQUIRE(roomLabel != nullptr);
    REQUIRE(bpmLabel != nullptr);
    REQUIRE(bpiLabel != nullptr);
    REQUIRE(voteButton != nullptr);
    REQUIRE(roomStatusLabel != nullptr);
    REQUIRE(bpmEditor != nullptr);
    REQUIRE(bpiEditor != nullptr);

    CHECK(roomLabel->getX() > hostLabel->getRight());
    CHECK(voteButton->getX() > hostLabel->getRight());
    CHECK(voteButton->getY() < roomStatusLabel->getY());
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
          .isEmpty());
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          .isEmpty());

    harness.editor->setRoomVoteValueForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm, 124);
    CHECK(bpmEditor->getText() == "124");
    CHECK(bpiEditor->getText().isEmpty());

    harness.editor->setRoomVoteValueForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi, 12);
    CHECK(bpiEditor->getText() == "12");
    CHECK(bpmEditor->getText().isEmpty());
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

    auto* bpmLabel = findLabelWithText(*harness.editor, "BPM");
    auto* bpiLabel = findLabelWithText(*harness.editor, "BPI");
    auto* bpmEditor = bpmLabel == nullptr ? nullptr : findTextEditorOnSameRowAs(*harness.editor, *bpmLabel);
    auto* bpiEditor = bpiLabel == nullptr ? nullptr : findTextEditorOnSameRowAs(*harness.editor, *bpiLabel);
    auto* voteButton = findButtonWithText(*harness.editor, "Vote");

    REQUIRE(bpmEditor != nullptr);
    REQUIRE(bpiEditor != nullptr);
    REQUIRE(voteButton != nullptr);

    bpmEditor->setText("124");
    REQUIRE(voteButton->onClick != nullptr);
    voteButton->onClick();
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
          .isEmpty());

    bpiEditor->setText("99");
    voteButton->onClick();
    CHECK(harness.submittedVoteCount == 1);
    CHECK(harness.editor->getRoomVoteStatusTextForTesting(FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpi)
          == "Enter a BPI between 2 and 64.");

    bpiEditor->setText("12");
    voteButton->onClick();
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
          .isEmpty());
}

TEST_CASE("plugin room controls ui keeps diagnostics hidden until requested and lets them take over the sidebar",
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

    auto* roomLabel = findLabelWithText(*harness.editor, "Room Chat");
    auto* diagnosticsButton = findButtonWithText(*harness.editor, "Show Diagnostics");
    auto* roomViewport = findRoomSidebarViewport(*harness.editor);

    REQUIRE(roomLabel != nullptr);
    REQUIRE(diagnosticsButton != nullptr);
    REQUIRE(roomViewport != nullptr);

    CHECK_FALSE(harness.editor->isDiagnosticsExpandedForTesting());
    CHECK(roomViewport->isVisible());
    CHECK(diagnosticsButton->getY() <= roomLabel->getY());

    harness.editor->clickDiagnosticsToggleForTesting();

    auto* diagnosticsEditor = findTextEditorContainingText(*harness.editor, "Diagnostics CPU");
    REQUIRE(diagnosticsEditor != nullptr);

    CHECK(harness.editor->isDiagnosticsExpandedForTesting());
    CHECK(diagnosticsEditor->isVisible());
    CHECK_FALSE(roomViewport->isVisible());
}

TEST_CASE("plugin room controls ui only auto-scrolls the room feed when the reader is already near the bottom",
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

    harness.roomUi.visibleFeed = makeScrollableFeed();
    harness.editor->refreshForTesting();

    auto* roomViewport = findRoomSidebarViewport(*harness.editor);
    REQUIRE(roomViewport != nullptr);

    const auto initialMaxY = getViewportMaxScrollY(*roomViewport);
    REQUIRE(initialMaxY > 0);

    roomViewport->setViewPosition(0, juce::jmax(0, initialMaxY - 6));
    harness.roomUi.visibleFeed.push_back({
        .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat,
        .author = "latecomer",
        .text = "Fresh update lands while the user is already following the bottom of the room chat feed.",
    });
    harness.editor->refreshForTesting();

    const auto bottomAfterRefresh = getViewportMaxScrollY(*roomViewport);
    CHECK(roomViewport->getViewPositionY() >= juce::jmax(0, bottomAfterRefresh - 4));

    roomViewport->setViewPosition(0, 0);
    harness.roomUi.visibleFeed.push_back({
        .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat,
        .author = "another-user",
        .text = "A later room update should not yank the view back down once the user has scrolled upward to read.",
    });
    harness.editor->refreshForTesting();

    CHECK(roomViewport->getViewPositionY() == 0);
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
    auto* roomLabel = findLabelWithText(*harness.editor, "Room Chat");
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

TEST_CASE("plugin room controls ui keeps the sidebar beside a shared local-first strip plane", "[plugin_room_controls_ui]")
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

    harness.mixerStrips = {
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
          .sourceId = "bob#0",
          .groupId = "bob",
          .groupLabel = "bob",
          .displayName = "bob - bass",
          .subtitle = "bass",
          .active = true,
          .visible = true },
    };
    harness.editor->refreshForTesting();

    auto* roomViewport = findRoomSidebarViewport(*harness.editor);

    REQUIRE(roomViewport != nullptr);

    const auto sidebarBounds = getBoundsInEditor(*harness.editor, *roomViewport);
    const auto stripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();

    REQUIRE(stripLabels.size() == 4);
    CHECK(stripLabels[0] == "Main");
    CHECK(stripLabels[1] == "Bass");
    CHECK(stripLabels[2] == "alice - guitar");
    CHECK(stripLabels[3] == "bob - bass");
    CHECK_FALSE(harness.editor->isRoomSectionAboveMixerForTesting());
    CHECK(sidebarBounds.getX() > harness.editor->getWidth() / 2);

    harness.editor->setSize(980, 720);
    harness.editor->resized();

    roomViewport = findRoomSidebarViewport(*harness.editor);
    REQUIRE(roomViewport != nullptr);

    const auto resizedSidebarBounds = getBoundsInEditor(*harness.editor, *roomViewport);
    const auto resizedStripLabels = harness.editor->getVisibleMixerStripLabelsForTesting();

    REQUIRE(resizedStripLabels.size() == 4);
    CHECK(resizedStripLabels[0] == "Main");
    CHECK(resizedStripLabels[1] == "Bass");
    CHECK(resizedStripLabels[2] == "alice - guitar");
    CHECK(resizedStripLabels[3] == "bob - bass");
    CHECK(resizedSidebarBounds.getX() > harness.editor->getWidth() / 2);
}

TEST_CASE("plugin room controls ui keeps room chat readable beside the expanded settings shell",
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
    auto* passwordLabel = findLabelWithText(*harness.editor, "Password");
    auto* roomLabel = findLabelWithText(*harness.editor, "Room Chat");
    auto* sidebarViewport = findRoomSidebarViewport(*harness.editor);

    REQUIRE(hostLabel != nullptr);
    REQUIRE(passwordLabel != nullptr);
    REQUIRE(roomLabel != nullptr);
    REQUIRE(sidebarViewport != nullptr);

    auto* hostEditor = findTextEditorOnSameRowAs(*harness.editor, *hostLabel);
    auto* passwordEditor = findTextEditorOnSameRowAs(*harness.editor, *passwordLabel);

    REQUIRE(hostEditor != nullptr);
    REQUIRE(passwordEditor != nullptr);

    const auto hostBounds = getBoundsInEditor(*harness.editor, *hostLabel);
    const auto passwordEditorBounds = getBoundsInEditor(*harness.editor, *passwordEditor);
    const auto roomBounds = getBoundsInEditor(*harness.editor, *roomLabel);
    const auto sidebarBounds = getBoundsInEditor(*harness.editor, *sidebarViewport);

    CHECK(roomBounds.getX() > hostBounds.getRight());
    CHECK(roomBounds.getX() > passwordEditorBounds.getRight());
    CHECK(roomBounds.getX() - passwordEditorBounds.getRight() <= 20);
    CHECK(roomBounds.getY() + 20 < hostBounds.getY());
    CHECK(sidebarBounds.getWidth() >= 240);
    CHECK(sidebarBounds.getHeight() >= harness.editor->getHeight() / 2 + 40);
}
