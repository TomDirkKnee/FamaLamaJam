#include <functional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server, double sampleRate, int blockSize)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "lifecycle_user";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(sampleRate, blockSize);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool()>& predicate,
                  int attempts = 6000)
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
} // namespace

TEST_CASE("plugin_host_lifecycle restored duplicate instance stays idle with persisted mix state",
          "[plugin_host_lifecycle]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor source(true, true);
    connectProcessor(source, server, 48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(source, buffer, midi, [&]() {
        return source.getTransportUiState().hasServerTiming && source.isRemoteSourceActiveForTesting("alice#0");
    }));

    REQUIRE(source.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, -2.5f, 0.4f, false));
    REQUIRE(source.setMixerStripMixState("alice#0", -6.5f, -0.2f, true));
    source.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "socket timeout",
    });
    REQUIRE(source.getLifecycleSnapshot().state == ConnectionState::Reconnecting);
    REQUIRE(source.getScheduledReconnectDelayMs() > 0);

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor duplicate(true, true);
    duplicate.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    FamaLamaJamAudioProcessor::MixerStripSnapshot remoteSnapshot;
    REQUIRE(duplicate.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    REQUIRE(duplicate.getMixerStripSnapshot("alice#0", remoteSnapshot));

    CHECK(duplicate.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(duplicate.isSessionConnected());
    CHECK(duplicate.getScheduledReconnectDelayMs() == 0);
    CHECK_FALSE(duplicate.getLifecycleSnapshot().hasPendingRetry());
    CHECK_FALSE(duplicate.getTransportUiState().hasServerTiming);
    CHECK(duplicate.getActiveRemoteSourceCountForTesting() == 0);
    CHECK(duplicate.getQueuedRemoteSourceCountForTesting() == 0);
    CHECK(duplicate.getPendingRemoteSourceCountForTesting() == 0);
    CHECK(localSnapshot.mix.gainDb == Catch::Approx(-2.5f));
    CHECK(localSnapshot.mix.pan == Catch::Approx(0.4f));
    CHECK_FALSE(localSnapshot.mix.muted);
    CHECK(remoteSnapshot.mix.gainDb == Catch::Approx(-6.5f));
    CHECK(remoteSnapshot.mix.pan == Catch::Approx(-0.2f));
    CHECK(remoteSnapshot.mix.muted);
}

TEST_CASE("plugin_host_lifecycle connected release and reprepare clears stale runtime before recovery",
          "[plugin_host_lifecycle]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, 48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().hasServerTiming
            && processor.isRemoteSourceActiveForTesting("alice#0")
            && processor.getTransportSentFramesForTesting() > 0
            && processor.getTransportReceivedFramesForTesting() > 0;
    }));

    processor.releaseResources();

    FamaLamaJamAudioProcessor::MixerStripSnapshot remoteSnapshot;
    REQUIRE(processor.getMixerStripSnapshot("alice#0", remoteSnapshot));

    CHECK_FALSE(processor.getTransportUiState().hasServerTiming);
    CHECK(processor.getActiveRemoteSourceCountForTesting() == 0);
    CHECK(processor.getQueuedRemoteSourceCountForTesting() == 0);
    CHECK(processor.getPendingRemoteSourceCountForTesting() == 0);
    CHECK(processor.getLastCodecPayloadBytesForTesting() == 0);
    CHECK(processor.getLastDecodedSamplesForTesting() == 0);
    CHECK_FALSE(remoteSnapshot.descriptor.active);
    CHECK_FALSE(remoteSnapshot.descriptor.visible);

    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().hasServerTiming
            && processor.isRemoteSourceActiveForTesting("alice#0")
            && processor.getLastCodecPayloadBytesForTesting() > 0
            && processor.getLastDecodedSamplesForTesting() > 0;
    }));
}

TEST_CASE("plugin_host_lifecycle host release cancels pending reconnect timers", "[plugin_host_lifecycle]")
{
    FamaLamaJamAudioProcessor processor;

    REQUIRE(processor.requestConnect());
    processor.handleConnectionEvent(ConnectionEvent { .type = ConnectionEventType::Connected });
    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "network timeout",
    });

    REQUIRE(processor.getLifecycleSnapshot().state == ConnectionState::Reconnecting);
    REQUIRE(processor.getScheduledReconnectDelayMs() > 0);

    processor.releaseResources();

    const auto snapshot = processor.getLifecycleSnapshot();
    CHECK(snapshot.state == ConnectionState::Idle);
    CHECK(snapshot.canConnect());
    CHECK_FALSE(snapshot.canDisconnect());
    CHECK(snapshot.nextRetryDelayMs == 0);
    CHECK(processor.getScheduledReconnectDelayMs() == 0);
    CHECK_FALSE(processor.triggerScheduledReconnectForTesting());
}

TEST_CASE("plugin_host_lifecycle hides remote strips immediately when a peer disconnects", "[plugin_host_lifecycle]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server, 48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        FamaLamaJamAudioProcessor::MixerStripSnapshot remoteSnapshot;
        return processor.getTransportUiState().hasServerTiming
            && processor.getMixerStripSnapshot("alice#0", remoteSnapshot)
            && remoteSnapshot.descriptor.visible;
    }));

    server.enqueueUserInfoChange("alice", "room", 0, false);

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        FamaLamaJamAudioProcessor::MixerStripSnapshot remoteSnapshot;
        return processor.getMixerStripSnapshot("alice#0", remoteSnapshot)
            && ! remoteSnapshot.descriptor.active
            && ! remoteSnapshot.descriptor.visible;
    }));
}
