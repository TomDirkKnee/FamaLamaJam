#include <cstdint>
#include <functional>

#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

juce::AudioProcessor::BusesLayout makeProofLayout(const juce::AudioChannelSet& auxInput,
                                                  const juce::AudioChannelSet& auxOutput)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.inputBuses.add(auxInput);
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(auxOutput);
    return layout;
}

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

bool waitForAuthoritativeTiming(FamaLamaJamAudioProcessor& processor,
                                juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& midi,
                                int maxAttempts = 400)
{
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (processor.getTransportUiState().hasServerTiming)
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

juce::AudioBuffer<float> makeConstantBuffer(int channels, int samples, float value)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    buffer.clear();

    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, value);

    return buffer;
}

float channelRms(const juce::AudioBuffer<float>& buffer, int channel)
{
    return buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
}

bool processUntil(FamaLamaJamAudioProcessor& processor,
                  juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi,
                  const std::function<bool(const juce::AudioBuffer<float>&)>& predicate,
                  int attempts = 400,
                  int sleepMs = 2)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);

        if (predicate(buffer))
            return true;

        juce::Thread::sleep(sleepMs);
    }

    return false;
}
} // namespace

TEST_CASE("plugin host multi io routing declares one proof aux input and one proof aux output pair",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);

    REQUIRE(processor.getBusCount(true) == 2);
    REQUIRE(processor.getBusCount(false) == 2);

    auto* mainInput = processor.getBus(true, FamaLamaJamAudioProcessor::HostRoutingProof::kMainInputBusIndex);
    auto* auxInput = processor.getBus(true, FamaLamaJamAudioProcessor::HostRoutingProof::kAuxInputBusIndex);
    auto* mainOutput = processor.getBus(false, FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
    auto* auxOutput = processor.getBus(false, FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);

    REQUIRE(mainInput != nullptr);
    REQUIRE(auxInput != nullptr);
    REQUIRE(mainOutput != nullptr);
    REQUIRE(auxOutput != nullptr);

    CHECK(mainInput->isEnabledByDefault());
    CHECK_FALSE(auxInput->isEnabledByDefault());
    CHECK(mainOutput->isEnabledByDefault());
    CHECK_FALSE(auxOutput->isEnabledByDefault());
    CHECK(auxInput->getName() == FamaLamaJamAudioProcessor::kHostRoutingProofAuxInputBusName);
    CHECK(auxOutput->getName() == FamaLamaJamAudioProcessor::kHostRoutingProofAuxOutputBusName);
    CHECK(auxInput->getCurrentLayout() == juce::AudioChannelSet::stereo());
    CHECK(auxOutput->getCurrentLayout() == juce::AudioChannelSet::stereo());
}

TEST_CASE("plugin host multi io routing layout gate accepts only stereo proof buses",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);

    CHECK(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::disabled(), juce::AudioChannelSet::disabled())));
    CHECK(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));

    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono())));
    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::create5point1())));
}

TEST_CASE("plugin host multi io routing proof distinguishes main-path and aux-input activity",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(processor.getBusCount(true) == 2);
    REQUIRE(processor.getBusCount(false) == 2);
    REQUIRE(processor.getTotalNumInputChannels() == 4);
    REQUIRE(processor.getTotalNumOutputChannels() == 4);

    juce::AudioBuffer<float> buffer(4, 512);
    juce::MidiBuffer midi;

    buffer.clear();
    for (int channel = 2; channel < 4; ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, 0.35f);

    processor.processBlock(buffer, midi);

    const auto proof = processor.getHostRoutingProofForTesting();
    CHECK_FALSE(proof.mainPathActive);
    CHECK(proof.auxInputActive);
}

TEST_CASE("plugin host multi io routing routes one selected remote source to the proof output pair",
          "[plugin_host_multi_io_routing]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    REQUIRE(processor.getBusCount(false) == 2);
    REQUIRE(processor.getTotalNumOutputChannels() == 4);

    juce::AudioBuffer<float> timingBuffer(processor.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, timingBuffer, midi));

    processor.selectHostRoutingProofSourceForTesting("routed-user#0");
    processor.registerRemoteSourceForTesting("routed-user#0", "routed-user", "Keys", 0u);
    processor.registerRemoteSourceForTesting("main-user#0", "main-user", "Keys", 0u);
    processor.injectDecodedRemoteIntervalForTesting("routed-user#0", makeConstantBuffer(2, 24000, 0.30f), 48000.0);
    processor.injectDecodedRemoteIntervalForTesting("main-user#0", makeConstantBuffer(2, 24000, 0.10f), 48000.0);

    juce::AudioBuffer<float> outputBuffer(processor.getTotalNumOutputChannels(), 512);
    const auto heardIsolatedRoute = processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>& output) {
        return channelRms(output, 0) > 1.0e-4f
            && channelRms(output, 1) > 1.0e-4f
            && channelRms(output, 2) > 1.0e-4f
            && channelRms(output, 3) > 1.0e-4f;
    });

    REQUIRE(heardIsolatedRoute);

    const auto proof = processor.getHostRoutingProofForTesting();
    CHECK(proof.selectedRoutedSourceId == "routed-user#0");
    CHECK(channelRms(outputBuffer, 2) > channelRms(outputBuffer, 0));
    CHECK(channelRms(outputBuffer, 3) > channelRms(outputBuffer, 1));
}
