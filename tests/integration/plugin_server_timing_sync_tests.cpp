#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

void connectProcessor(famalamajam::plugin::FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));

    const auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == famalamajam::app::session::ConnectionState::Active);
}

bool waitForAuthoritativeTiming(famalamajam::plugin::FamaLamaJamAudioProcessor& processor,
                                juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& midi)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        const auto state = processor.getTransportUiState();
        if (state.hasServerTiming && state.beatsPerMinute > 0 && state.beatsPerInterval > 0)
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

famalamajam::plugin::FamaLamaJamAudioProcessor::TransportUiState processSamples(
    famalamajam::plugin::FamaLamaJamAudioProcessor& processor,
    int sampleCount)
{
    juce::MidiBuffer midi;
    famalamajam::plugin::FamaLamaJamAudioProcessor::TransportUiState state;

    int remaining = sampleCount;
    int blockSize = 257;
    while (remaining > 0)
    {
        const auto currentBlock = juce::jmin(blockSize, remaining);
        juce::AudioBuffer<float> buffer(2, currentBlock);
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);
        state = processor.getTransportUiState();
        remaining -= currentBlock;
        blockSize = blockSize == 257 ? 509 : 257;
    }

    return state;
}

bool waitForBpm(famalamajam::plugin::FamaLamaJamAudioProcessor& processor,
                int targetBpm,
                int targetBpi,
                int maxAttempts = 2000)
{
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        const auto state = processSamples(processor, 257);
        if (state.hasServerTiming && state.beatsPerMinute == targetBpm && state.beatsPerInterval == targetBpi)
            return true;
    }

    return false;
}
} // namespace

TEST_CASE("plugin server timing sync exposes authoritative timing after connect", "[plugin_server_timing_sync]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    connectProcessor(processor, server);
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    const auto transport = processor.getTransportUiState();
    CHECK(transport.connected);
    CHECK(transport.hasServerTiming);
    CHECK(transport.beatsPerMinute == 120);
    CHECK(transport.beatsPerInterval == 16);

    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin server timing sync clears transport timing on disconnect", "[plugin_server_timing_sync]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    connectProcessor(processor, server);
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));
    REQUIRE(processor.requestDisconnect());

    const auto transport = processor.getTransportUiState();
    CHECK_FALSE(transport.connected);
    CHECK_FALSE(transport.hasServerTiming);
    CHECK(transport.currentBeat == 0);
    CHECK(transport.intervalProgress == 0.0f);

    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin server timing sync defers bpm changes until the next interval boundary", "[plugin_server_timing_sync]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    connectProcessor(processor, server);
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    const auto initialState = processor.getTransportUiState();
    REQUIRE(initialState.beatsPerMinute == 120);
    REQUIRE(initialState.beatsPerInterval == 16);

    const auto initialIntervalSamples = static_cast<int>(std::llround((60.0 * 16.0 / 120.0) * 48000.0));
    const auto changedIntervalSamples = static_cast<int>(std::llround((60.0 * 8.0 / 90.0) * 48000.0));

    auto state = processSamples(processor, initialIntervalSamples / 3);
    REQUIRE(state.intervalIndex == 0);
    REQUIRE(state.intervalProgress > 0.25f);
    REQUIRE(state.intervalProgress < 0.4f);

    server.enqueueConfigChange(90, 8);

    state = processSamples(processor, initialIntervalSamples / 3);
    CHECK(state.beatsPerMinute == 120);
    CHECK(state.beatsPerInterval == 16);
    CHECK(state.intervalIndex == 0);

    REQUIRE(waitForBpm(processor, 90, 8));
    state = processor.getTransportUiState();

    CHECK(state.intervalIndex >= 1);
    CHECK(state.beatsPerMinute == 90);
    CHECK(state.beatsPerInterval == 8);
    CHECK(state.currentBeat == 1);

    const auto progressedState = processSamples(processor, changedIntervalSamples / 4);
    CHECK(progressedState.intervalIndex == state.intervalIndex);
    CHECK(progressedState.intervalProgress > 0.2f);

    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin server timing sync clears timing on retryable loss", "[plugin_server_timing_sync]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    connectProcessor(processor, server);
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    processSamples(processor, 4096);
    processor.handleConnectionEvent(famalamajam::app::session::ConnectionEvent {
        .type = famalamajam::app::session::ConnectionEventType::ConnectionLostRetryable,
        .reason = "timing dropped",
    });

    const auto transport = processor.getTransportUiState();
    CHECK_FALSE(transport.hasServerTiming);
    CHECK(transport.currentBeat == 0);
    CHECK(transport.intervalProgress == 0.0f);
    CHECK(transport.intervalIndex == 0);

    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin server timing sync stays sample-accurate over long runs", "[plugin_server_timing_sync]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    connectProcessor(processor, server);
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    constexpr int sampleRate = 48000;
    const auto intervalSamples = static_cast<int>(std::llround((60.0 * 16.0 / 120.0) * sampleRate));
    const auto beatSamples = static_cast<int>(std::llround((60.0 / 120.0) * sampleRate));
    const auto totalSamples = intervalSamples * 3 + beatSamples * 2 + 137;

    const auto state = processSamples(processor, totalSamples);
    const auto expectedIndex = static_cast<std::uint64_t>(totalSamples / intervalSamples);
    const auto expectedRemainder = totalSamples % intervalSamples;
    const auto expectedProgress = static_cast<float>(expectedRemainder) / static_cast<float>(intervalSamples);
    const auto expectedBeat = juce::jlimit(1, 16, expectedRemainder / beatSamples + 1);

    CHECK(state.intervalIndex == expectedIndex);
    CHECK(std::abs(state.intervalProgress - expectedProgress) < 0.005f);
    CHECK(state.currentBeat == expectedBeat);

    processor.releaseResources();
    server.stopServer();
}
