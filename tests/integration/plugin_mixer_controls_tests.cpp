#include <functional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
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

float captureAudibleRms(FamaLamaJamAudioProcessor& processor,
                        juce::AudioBuffer<float>& buffer,
                        juce::MidiBuffer& midi,
                        bool feedInput,
                        int attempts = 128)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        if (feedInput)
            fillRampBuffer(buffer);
        else
            buffer.clear();

        processor.processBlock(buffer, midi);

        const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto right = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        const auto rms = juce::jmax(left, right);
        if (rms > 1.0e-6f)
            return rms;

        juce::Thread::sleep(1);
    }

    return 0.0f;
}

juce::AudioBuffer<float> makeProcessBuffer(FamaLamaJamAudioProcessor& processor, int samples)
{
    return juce::AudioBuffer<float>(juce::jmax(processor.getTotalNumInputChannels(),
                                               processor.getTotalNumOutputChannels()),
                                    samples);
}
} // namespace

TEST_CASE("plugin mixer controls applies remote gain pan and mute per user-plus-channel strip",
          "[plugin_mixer_controls]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 }, { "bob", 1, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    auto buffer = makeProcessBuffer(processor, 512);
    juce::MidiBuffer midi;

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.isRemoteSourceActiveForTesting("alice#0")
            && processor.isRemoteSourceActiveForTesting("bob#1");
    }));

    REQUIRE(processor.setMixerStripMixState("alice#0", 0.0f, -1.0f, false));
    REQUIRE(processor.setMixerStripMixState("bob#1", 0.0f, 1.0f, false));

    bool heardSeparatedSides = false;
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
        const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto right = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        if (left > 1.0e-4f && right > 1.0e-4f)
        {
            heardSeparatedSides = true;
            break;
        }
    }
    REQUIRE(heardSeparatedSides);

    REQUIRE(processor.setMixerStripMixState("alice#0", 0.0f, -1.0f, true));

    bool mutedOnlyTargetedStrip = false;
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
        const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto right = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        if (right > 1.0e-4f && left < right * 0.25f)
        {
            mutedOnlyTargetedStrip = true;
            break;
        }
    }
    CHECK(mutedOnlyTargetedStrip);
}

TEST_CASE("plugin mixer controls exposes dedicated local monitor strip meters and independent mix state",
          "[plugin_mixer_controls]")
{
    FamaLamaJamAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(localSnapshot.descriptor.kind == FamaLamaJamAudioProcessor::MixerStripKind::LocalMonitor);
    CHECK(localSnapshot.descriptor.visible);

    REQUIRE(processor.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, -6.0f, 0.5f, false));

    auto buffer = makeProcessBuffer(processor, 512);
    fillRampBuffer(buffer);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(localSnapshot.mix.gainDb == Catch::Approx(-6.0f));
    CHECK(localSnapshot.mix.pan == Catch::Approx(0.5f));
    CHECK(localSnapshot.meter.left > 0.0f);
    CHECK(localSnapshot.meter.right > 0.0f);
}

TEST_CASE("plugin mixer controls persists mix state across restore and reconnect for the same source identity",
          "[plugin_mixer_controls]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor source(true, true);
    connectProcessor(source, server);

    auto buffer = makeProcessBuffer(source, 512);
    juce::MidiBuffer midi;

    REQUIRE(processUntil(source, buffer, midi, [&]() { return source.isRemoteSourceActiveForTesting("alice#0"); }));

    REQUIRE(source.setMixerStripMixState("alice#0", -9.0f, 0.25f, true));
    REQUIRE(source.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, -3.0f, -0.25f, false));

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE(source.requestDisconnect());
    source.releaseResources();

    FamaLamaJamAudioProcessor restored(true, true);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    FamaLamaJamAudioProcessor::MixerStripSnapshot snapshot;
    REQUIRE(restored.getMixerStripSnapshot("alice#0", snapshot));
    CHECK(snapshot.mix.gainDb == Catch::Approx(-9.0f));
    CHECK(snapshot.mix.pan == Catch::Approx(0.25f));
    CHECK(snapshot.mix.muted);
    CHECK_FALSE(snapshot.descriptor.active);

    REQUIRE(restored.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, snapshot));
    CHECK(snapshot.mix.gainDb == Catch::Approx(-3.0f));
    CHECK(snapshot.mix.pan == Catch::Approx(-0.25f));
    CHECK_FALSE(snapshot.mix.muted);

    connectProcessor(restored, server);
    REQUIRE(processUntil(restored, buffer, midi, [&]() { return restored.isRemoteSourceActiveForTesting("alice#0"); }));

    REQUIRE(restored.getMixerStripSnapshot("alice#0", snapshot));
    CHECK(snapshot.mix.gainDb == Catch::Approx(-9.0f));
    CHECK(snapshot.mix.pan == Catch::Approx(0.25f));
    CHECK(snapshot.mix.muted);
}

TEST_CASE("plugin mixer controls master output scales both mixed playback and metronome-only output",
          "[plugin_mixer_controls]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setRemotePeers({ { "alice", 0, 0 } });
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);
    processor.setMetronomeEnabled(true);

    auto buffer = makeProcessBuffer(processor, 512);
    juce::MidiBuffer midi;

    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransportUiState().hasServerTiming && processor.isRemoteSourceActiveForTesting("alice#0");
    }));

    processor.setMasterOutputGainDb(0.0f);
    const auto unityMixedRms = captureAudibleRms(processor, buffer, midi, true);
    REQUIRE(unityMixedRms > 1.0e-4f);

    processor.setMasterOutputGainDb(-12.0f);
    const auto attenuatedMixedRms = captureAudibleRms(processor, buffer, midi, true);
    REQUIRE(attenuatedMixedRms > 1.0e-4f);
    CHECK(attenuatedMixedRms < unityMixedRms * 0.4f);

    REQUIRE(processor.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, 0.0f, 0.0f, true));
    REQUIRE(processor.setMixerStripMixState("alice#0", 0.0f, 0.0f, true));

    processor.setMasterOutputGainDb(0.0f);
    const auto unityMetronomeRms = captureAudibleRms(processor, buffer, midi, false);
    REQUIRE(unityMetronomeRms > 1.0e-6f);

    processor.setMasterOutputGainDb(-12.0f);
    const auto attenuatedMetronomeRms = captureAudibleRms(processor, buffer, midi, false);
    REQUIRE(attenuatedMetronomeRms > 1.0e-6f);
    CHECK(attenuatedMetronomeRms < unityMetronomeRms * 0.4f);
}
