#include <cstdint>
#include <sstream>
#include <vector>

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

std::string joinIds(const std::vector<std::string>& ids)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        if (index > 0)
            stream << ", ";
        stream << ids[index];
    }

    return stream.str();
}
} // namespace

TEST_CASE("plugin remote interval alignment activates ready peers on the same boundary",
          "[plugin_remote_interval_alignment]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 }, { "bob", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const auto readyTogether = processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming
            && processor.isRemoteSourceActiveForTesting("alice#0")
            && processor.isRemoteSourceActiveForTesting("bob#0");
    });

    INFO("intervalIndex=" << processor.getTransportUiState().intervalIndex
         << " active=" << joinIds(processor.getActiveRemoteSourceIdsForTesting())
         << " queuedSources=" << processor.getQueuedRemoteSourceCountForTesting()
         << " pendingSources=" << processor.getPendingRemoteSourceCountForTesting()
         << " receivedFrames=" << processor.getTransportReceivedFramesForTesting());
    REQUIRE(readyTogether);

    const auto transport = processor.getTransportUiState();
    CHECK(transport.intervalIndex >= 1);
    CHECK(processor.isRemoteSourceActiveForTesting("alice#0"));
    CHECK(processor.isRemoteSourceActiveForTesting("bob#0"));
}

TEST_CASE("plugin remote interval alignment skips late peers instead of starting them mid-interval",
          "[plugin_remote_interval_alignment]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "ontime", 0, 0 }, { "late", 0, 10 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const auto ontimeReady = processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming
            && processor.isRemoteSourceActiveForTesting("ontime#0");
    });

    INFO("intervalIndex=" << processor.getTransportUiState().intervalIndex
         << " active=" << joinIds(processor.getActiveRemoteSourceIdsForTesting())
         << " queuedSources=" << processor.getQueuedRemoteSourceCountForTesting()
         << " pendingSources=" << processor.getPendingRemoteSourceCountForTesting()
         << " receivedFrames=" << processor.getTransportReceivedFramesForTesting());
    REQUIRE(ontimeReady);

    const auto ontimeStartIndex = processor.getTransportUiState().intervalIndex;
    CHECK_FALSE(processor.isRemoteSourceActiveForTesting("late#0"));

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.intervalIndex > ontimeStartIndex && processor.isRemoteSourceActiveForTesting("late#0");
    }));

    const auto transport = processor.getTransportUiState();
    CHECK(transport.intervalIndex > ontimeStartIndex);
    CHECK(processor.isRemoteSourceActiveForTesting("late#0"));
}

TEST_CASE("plugin remote interval alignment realigns after reconnect before resuming remote playback",
          "[plugin_remote_interval_alignment]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const auto initialAliceReady = processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming
            && processor.isRemoteSourceActiveForTesting("alice#0");
    });

    INFO("intervalIndex=" << processor.getTransportUiState().intervalIndex
         << " active=" << joinIds(processor.getActiveRemoteSourceIdsForTesting())
         << " queuedSources=" << processor.getQueuedRemoteSourceCountForTesting()
         << " pendingSources=" << processor.getPendingRemoteSourceCountForTesting()
         << " receivedFrames=" << processor.getTransportReceivedFramesForTesting());
    REQUIRE(initialAliceReady);

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionLostRetryable,
        .reason = "transport disconnected",
    });

    CHECK(processor.getLifecycleSnapshot().state == ConnectionState::Reconnecting);
    CHECK(processor.getActiveRemoteSourceCountForTesting() == 0);

    juce::Thread::sleep(25);
    REQUIRE(processor.triggerScheduledReconnectForTesting());
    REQUIRE(server.waitForAuthentication(2000));

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.hasServerTiming && transport.intervalIndex == 0;
    }));

    CHECK(processor.getActiveRemoteSourceCountForTesting() == 0);

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        const auto transport = processor.getTransportUiState();
        return transport.intervalIndex > 0 && processor.isRemoteSourceActiveForTesting("alice#0");
    }));

    CHECK(processor.isRemoteSourceActiveForTesting("alice#0"));
}
