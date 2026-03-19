#include <functional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettingsController.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::app::session::ConnectionLifecycleSnapshot;
using famalamajam::app::session::ConnectionState;
using famalamajam::app::session::SessionSettings;
using famalamajam::app::session::SessionSettingsController;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::plugin::FamaLamaJamAudioProcessorEditor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

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
    std::unique_ptr<FamaLamaJamAudioProcessorEditor> editor;

    EditorHarness()
        : settings(famalamajam::app::session::makeDefaultSessionSettings())
        , lifecycle(ConnectionLifecycleSnapshot {
              .state = ConnectionState::Active,
              .statusMessage = "Connected. Start playing when the beat appears.",
          })
        , transport(FamaLamaJamAudioProcessorEditor::TransportUiState {
              .connected = true,
              .hasServerTiming = true,
              .syncHealth = FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy,
              .metronomeAvailable = true,
              .beatsPerMinute = 120,
              .beatsPerInterval = 16,
              .currentBeat = 4,
              .intervalProgress = 0.5f,
              .intervalIndex = 2,
          })
        , roomUi({
              .connected = true,
              .topic = "Tonight: lock the groove before the chorus.",
              .visibleFeed = {
                  {
                      .kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic,
                      .author = "",
                      .text = "Tonight: lock the groove before the chorus.",
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
                .meterLeft = 0.6f,
                .meterRight = 0.4f,
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
                    if (strip.sourceId != sourceId)
                        continue;

                    strip.gainDb = gain;
                    strip.pan = pan;
                    strip.muted = muted;
                    return true;
                }

                return false;
            },
            [this]() { return metronomeEnabled; },
            [this](bool enabled) { metronomeEnabled = enabled; },
            []() { return FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState {}; },
            [](bool) { return false; },
            [this]() { return roomUi; },
            [](std::string) { return true; },
            [](FamaLamaJamAudioProcessorEditor::RoomVoteKind, int) { return true; });
    }
};

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool()>& predicate,
                  int attempts = 5000)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (predicate())
            return true;

        juce::Thread::sleep(1);
    }

    return false;
}

std::string makeJamTabaComparisonNote(const FamaLamaJamAudioProcessorEditor::CpuDiagnosticSnapshot& editorSnapshot,
                                      const FamaLamaJamAudioProcessor::CpuDiagnosticSnapshot& processorSnapshot)
{
    if (editorSnapshot.roomFeedRebuildCalls > 1
        && editorSnapshot.mixerStripUpdateCalls > editorSnapshot.mixerStripWidgetBuildCount
        && processorSnapshot.remoteMixSourceVisits == 0)
    {
        return "JamTaba likely avoids unconditional room-feed rebuilds and unchanged mixer refresh passes, so this "
               "looks code-driven in the editor path before any residual machine-sensitive risk.";
    }

    if (processorSnapshot.remoteMixSourceVisits > 0 && processorSnapshot.remoteMixChannelWrites > 0)
    {
        return "JamTaba comparison suggests the processor hot path is active too, so any remaining spike risk is mixed "
               "between code-driven mix work and machine sensitivity.";
    }

    return "JamTaba comparison stays inconclusive until the diagnostics show either editor churn or processor hot-path "
           "work clearly.";
}
} // namespace

TEST_CASE("plugin pre layout cpu diagnostics keeps room feed rebuild work dirty-checked when feed state is unchanged",
          "[plugin_pre_layout_cpu_diagnostics]")
{
    EditorHarness harness;

    harness.editor->resetCpuDiagnosticSnapshotForTesting();

    for (int refresh = 0; refresh < 6; ++refresh)
        harness.editor->refreshForTesting();

    const auto snapshot = harness.editor->getCpuDiagnosticSnapshotForTesting();
    REQUIRE(snapshot.roomRefreshCalls == 6);
    CHECK(snapshot.roomFeedRebuildCalls == 0);
    CHECK(snapshot.resizedCalls == 0);
}

TEST_CASE("plugin pre layout cpu diagnostics keeps unchanged mixer strips off the old unconditional refresh cadence",
          "[plugin_pre_layout_cpu_diagnostics]")
{
    EditorHarness harness;

    harness.editor->resetCpuDiagnosticSnapshotForTesting();

    for (int refresh = 0; refresh < 5; ++refresh)
        harness.editor->refreshForTesting();

    const auto snapshot = harness.editor->getCpuDiagnosticSnapshotForTesting();
    REQUIRE(snapshot.mixerRefreshCalls == 5);
    CHECK(snapshot.mixerStripWidgetBuildCount == 0);
    CHECK(snapshot.mixerStripUpdateCalls == 0);
}

TEST_CASE("plugin pre layout cpu diagnostics separates processor hot-path counters from non-audio polling",
          "[plugin_pre_layout_cpu_diagnostics]")
{
    FamaLamaJamAudioProcessor processor(true, true);
    processor.prepareToPlay(48000.0, 512);
    processor.resetCpuDiagnosticSnapshotForTesting();

    for (int read = 0; read < 8; ++read)
    {
        (void) processor.getLifecycleSnapshot();
        (void) processor.getRoomUiState();
        (void) processor.getTransportUiState();
    }

    auto snapshot = processor.getCpuDiagnosticSnapshotForTesting();
    CHECK(snapshot.processBlockCalls == 0);
    CHECK(snapshot.remoteMixSourceVisits == 0);
    CHECK(snapshot.remoteMixChannelWrites == 0);

    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(processor, buffer, midi, [&]() { return processor.isRemoteSourceActiveForTesting("alice#0"); }));

    processor.resetCpuDiagnosticSnapshotForTesting();
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getCpuDiagnosticSnapshotForTesting().remoteMixChannelWrites > 0;
    }, 64));

    snapshot = processor.getCpuDiagnosticSnapshotForTesting();
    CHECK(snapshot.processBlockCalls > 0);
    CHECK(snapshot.meterResetCalls == snapshot.processBlockCalls);
    CHECK(snapshot.remoteMixSourceVisits > 0);
    CHECK(snapshot.remoteMixChannelWrites > 0);
}

TEST_CASE("plugin pre layout cpu diagnostics frames a JamTaba-informed diagnosis from the measured counters",
          "[plugin_pre_layout_cpu_diagnostics]")
{
    const auto note = makeJamTabaComparisonNote(
        FamaLamaJamAudioProcessorEditor::CpuDiagnosticSnapshot {
            .roomFeedRebuildCalls = 6,
            .mixerStripUpdateCalls = 10,
            .mixerStripWidgetBuildCount = 0,
        },
        FamaLamaJamAudioProcessor::CpuDiagnosticSnapshot {});

    CHECK(note.find("JamTaba") != std::string::npos);
    CHECK(note.find("room-feed") != std::string::npos);
    CHECK(note.find("code-driven") != std::string::npos);
}
