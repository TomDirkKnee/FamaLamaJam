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
                                                  const juce::AudioChannelSet& auxOutput,
                                                  const juce::AudioChannelSet& auxOutput2 = juce::AudioChannelSet::disabled())
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.inputBuses.add(auxInput);
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(auxOutput);
    layout.outputBuses.add(auxOutput2);
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

juce::AudioBuffer<float> getOutputBus(FamaLamaJamAudioProcessor& processor,
                                      juce::AudioBuffer<float>& buffer,
                                      int busIndex)
{
    return processor.getBusBuffer(buffer, false, busIndex);
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

TEST_CASE("plugin host multi io routing declares the fixed local and remote routing buses",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);

    REQUIRE(processor.getBusCount(true) == 2);
    REQUIRE(processor.getBusCount(false) == 3);

    auto* mainInput = processor.getBus(true, FamaLamaJamAudioProcessor::HostRoutingProof::kMainInputBusIndex);
    auto* auxInput = processor.getBus(true, FamaLamaJamAudioProcessor::HostRoutingProof::kAuxInputBusIndex);
    auto* mainOutput = processor.getBus(false, FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
    auto* auxOutput = processor.getBus(false, FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
    auto* auxOutput2 = processor.getBus(false, 2);

    REQUIRE(mainInput != nullptr);
    REQUIRE(auxInput != nullptr);
    REQUIRE(mainOutput != nullptr);
    REQUIRE(auxOutput != nullptr);
    REQUIRE(auxOutput2 != nullptr);

    CHECK(auxInput->getName() == FamaLamaJamAudioProcessor::kHostRoutingProofAuxInputBusName);
    CHECK(auxOutput->getName() == FamaLamaJamAudioProcessor::kHostRoutingProofAuxOutputBusName);
    CHECK(auxOutput2->getName() == FamaLamaJamAudioProcessor::kFixedRoutedOutputBus2Name);
    CHECK(auxInput->getCurrentLayout() == juce::AudioChannelSet::stereo());
    CHECK(auxOutput->getCurrentLayout() == juce::AudioChannelSet::stereo());
    CHECK(auxOutput2->getCurrentLayout() == juce::AudioChannelSet::stereo());
}

TEST_CASE("plugin host multi io routing locks two fixed local slots and three host-facing output labels",
          "[plugin_host_multi_io_routing]")
{
    REQUIRE(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots.size() == 2);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[0].sourceId)
          == FamaLamaJamAudioProcessor::kLocalMainSourceId);
    CHECK(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[0].inputBusIndex == 0);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[0].fallbackLabel) == "Main");
    CHECK(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[0].visibleByDefault);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[1].sourceId)
          == FamaLamaJamAudioProcessor::kLocalSend2SourceId);
    CHECK(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[1].inputBusIndex == 1);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[1].fallbackLabel) == "Local Send 2");
    CHECK_FALSE(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[1].visibleByDefault);

    REQUIRE(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes.size() == 3);
    CHECK(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[0].outputBusIndex == 0);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[0].hostLabel) == "FLJ Main Output");
    CHECK(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[1].outputBusIndex == 1);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[1].hostLabel) == "Remote Out 1");
    CHECK(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[2].outputBusIndex == 2);
    CHECK(std::string(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes[2].hostLabel) == "Remote Out 2");

    FamaLamaJamAudioProcessor processor(true, true);
    CHECK(processor.getBusCount(true) == static_cast<int>(FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots.size()));
    CHECK(processor.getBusCount(false) == static_cast<int>(FamaLamaJamAudioProcessor::kFixedRemoteOutputRoutes.size()));
}

TEST_CASE("plugin host multi io routing layout gate accepts only stereo proof buses",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);

    CHECK(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::disabled(),
                        juce::AudioChannelSet::disabled(),
                        juce::AudioChannelSet::disabled())));
    CHECK(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::stereo())));

    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::mono(),
                        juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::stereo())));
    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::mono(),
                        juce::AudioChannelSet::stereo())));
    CHECK_FALSE(processor.isBusesLayoutSupported(
        makeProofLayout(juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::stereo(),
                        juce::AudioChannelSet::create5point1())));
}

TEST_CASE("plugin host multi io routing proof distinguishes main-path and aux-input activity",
          "[plugin_host_multi_io_routing]")
{
    FamaLamaJamAudioProcessor processor(true, true);
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(processor.getBusCount(true) == 2);
    REQUIRE(processor.getBusCount(false) == 3);
    REQUIRE(processor.getTotalNumInputChannels() == 4);
    REQUIRE(processor.getTotalNumOutputChannels() == 6);

    juce::AudioBuffer<float> buffer(4, 512);
    juce::MidiBuffer midi;

    buffer.clear();
    auto auxInput = processor.getBusBuffer(buffer, true, FamaLamaJamAudioProcessor::HostRoutingProof::kAuxInputBusIndex);
    for (int channel = 0; channel < auxInput.getNumChannels(); ++channel)
        for (int sample = 0; sample < auxInput.getNumSamples(); ++sample)
            auxInput.setSample(channel, sample, 0.35f);

    processor.processBlock(buffer, midi);

    const auto proof = processor.getHostRoutingProofForTesting();
    CHECK_FALSE(proof.mainPathActive);
    CHECK(proof.auxInputActive);
}

TEST_CASE("plugin host multi io routing expects per-slot channel metadata and channel-indexed uploads",
          "[plugin_host_multi_io_routing][plugin_voice_mode_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    MiniNinjamServer::ClientChannelInfo mainChannelInfo;
    REQUIRE(server.waitForCapturedClientChannelInfo(2000, mainChannelInfo));
    CHECK(mainChannelInfo.channelIndex == 0);
    CHECK(mainChannelInfo.channelName == FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[0].fallbackLabel);

    MiniNinjamServer::ClientChannelInfo extraChannelInfo;
    const auto capturedExtraChannelInfo = server.waitForCapturedClientChannelInfo(250, extraChannelInfo);
    CHECK(capturedExtraChannelInfo);
    if (capturedExtraChannelInfo)
    {
        CHECK(extraChannelInfo.channelIndex == 1);
        CHECK(extraChannelInfo.channelName == FamaLamaJamAudioProcessor::kFixedLocalRoutingSlots[1].fallbackLabel);
    }

    juce::AudioBuffer<float> buffer(processor.getTotalNumInputChannels(), 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    bool capturedChannelZero = false;
    bool capturedChannelOne = false;
    for (int attempt = 0; attempt < 800 && (! capturedChannelZero || ! capturedChannelOne); ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);
        MiniNinjamServer::UploadIntervalBegin uploadBegin;
        while (server.waitForCapturedUploadBegin(2, uploadBegin))
        {
            capturedChannelZero = capturedChannelZero || uploadBegin.channelIndex == 0;
            capturedChannelOne = capturedChannelOne || uploadBegin.channelIndex == 1;
        }
    }

    CHECK(capturedChannelZero);
    CHECK(capturedChannelOne);
}

TEST_CASE("plugin host multi io routing routes one selected remote source to the proof output pair",
          "[plugin_host_multi_io_routing]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    REQUIRE(processor.getBusCount(false) == 3);
    REQUIRE(processor.getTotalNumOutputChannels() == 6);

    juce::AudioBuffer<float> timingBuffer(processor.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, timingBuffer, midi));

    processor.selectHostRoutingProofSourceForTesting("routed-user#0");
    processor.registerRemoteSourceForTesting("routed-user#0", "routed-user", "Keys", 0u);
    processor.registerRemoteSourceForTesting("main-user#0", "main-user", "Keys", 0u);
    processor.injectDecodedRemoteIntervalForTesting("routed-user#0", makeConstantBuffer(2, 24000, 0.30f), 48000.0);
    processor.injectDecodedRemoteIntervalForTesting("main-user#0", makeConstantBuffer(2, 24000, 0.10f), 48000.0);

    juce::AudioBuffer<float> outputBuffer(processor.getTotalNumOutputChannels(), 512);
    const auto sourcesActivated = processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>&) {
        return processor.isRemoteSourceActiveForTesting("routed-user#0")
            && processor.isRemoteSourceActiveForTesting("main-user#0");
    });
    REQUIRE(sourcesActivated);

    const auto heardIsolatedRoute = processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>& output) {
        auto mainOutput = getOutputBus(processor,
                                       const_cast<juce::AudioBuffer<float>&>(output),
                                       FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
        auto auxOutput = getOutputBus(processor,
                                      const_cast<juce::AudioBuffer<float>&>(output),
                                      FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
        return channelRms(mainOutput, 0) > 1.0e-4f
            && channelRms(mainOutput, 1) > 1.0e-4f
            && channelRms(auxOutput, 0) > 1.0e-4f
            && channelRms(auxOutput, 1) > 1.0e-4f;
    });

    REQUIRE(heardIsolatedRoute);

    const auto proof = processor.getHostRoutingProofForTesting();
    auto mainOutput = getOutputBus(processor,
                                   outputBuffer,
                                   FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
    auto auxOutput = getOutputBus(processor,
                                  outputBuffer,
                                  FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
    CHECK(proof.selectedRoutedSourceId == "routed-user#0");
    CHECK(channelRms(auxOutput, 0) > channelRms(mainOutput, 0));
    CHECK(channelRms(auxOutput, 1) > channelRms(mainOutput, 1));
}

TEST_CASE("plugin host multi io routing clears the proof output when no source remains assigned",
          "[plugin_host_multi_io_routing]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> timingBuffer(processor.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, timingBuffer, midi));

    processor.selectHostRoutingProofSourceForTesting("routed-user#0");
    processor.registerRemoteSourceForTesting("routed-user#0", "routed-user", "Keys", 0u);
    processor.registerRemoteSourceForTesting("main-user#0", "main-user", "Keys", 0u);
    processor.injectDecodedRemoteIntervalForTesting("routed-user#0", makeConstantBuffer(2, 24000, 0.30f), 48000.0);
    processor.injectDecodedRemoteIntervalForTesting("main-user#0", makeConstantBuffer(2, 24000, 0.10f), 48000.0);

    juce::AudioBuffer<float> outputBuffer(processor.getTotalNumOutputChannels(), 512);
    REQUIRE(processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>&) {
        return processor.isRemoteSourceActiveForTesting("routed-user#0")
            && processor.isRemoteSourceActiveForTesting("main-user#0");
    }));

    REQUIRE(processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>& output) {
        auto mainOutput = getOutputBus(processor,
                                       const_cast<juce::AudioBuffer<float>&>(output),
                                       FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
        auto auxOutput = getOutputBus(processor,
                                      const_cast<juce::AudioBuffer<float>&>(output),
                                      FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
        return channelRms(mainOutput, 0) > 1.0e-4f && channelRms(auxOutput, 0) > 1.0e-4f;
    }));

    processor.selectHostRoutingProofSourceForTesting({});

    const auto auxCleared = processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>& output) {
        auto mainOutput = getOutputBus(processor,
                                       const_cast<juce::AudioBuffer<float>&>(output),
                                       FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
        auto auxOutput = getOutputBus(processor,
                                      const_cast<juce::AudioBuffer<float>&>(output),
                                      FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
        return channelRms(mainOutput, 0) > 1.0e-4f
            && channelRms(mainOutput, 1) > 1.0e-4f
            && channelRms(auxOutput, 0) < 1.0e-6f
            && channelRms(auxOutput, 1) < 1.0e-6f;
    });

    REQUIRE(auxCleared);
}

TEST_CASE("plugin host multi io routing applies the selected route to decoded voice chunks too",
          "[plugin_host_multi_io_routing][plugin_voice_mode_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> timingBuffer(processor.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, timingBuffer, midi));

    processor.selectHostRoutingProofSourceForTesting("voice-user#1");
    processor.registerRemoteSourceForTesting("voice-user#1", "voice-user", "Voice", 0x2u);
    processor.registerRemoteSourceForTesting("main-user#0", "main-user", "Voice", 0x2u);
    processor.injectDecodedRemoteVoiceForTesting("voice-user#1", makeConstantBuffer(2, 2048, 0.24f), 48000.0);
    processor.injectDecodedRemoteVoiceForTesting("main-user#0", makeConstantBuffer(2, 2048, 0.08f), 48000.0);

    juce::AudioBuffer<float> outputBuffer(processor.getTotalNumOutputChannels(), 512);
    const auto heardRoutedVoice = processUntil(processor, outputBuffer, midi, [&](const juce::AudioBuffer<float>& output) {
        auto mainOutput = getOutputBus(processor,
                                       const_cast<juce::AudioBuffer<float>&>(output),
                                       FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
        auto auxOutput = getOutputBus(processor,
                                      const_cast<juce::AudioBuffer<float>&>(output),
                                      FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
        return channelRms(mainOutput, 0) > 1.0e-4f
            && channelRms(mainOutput, 1) > 1.0e-4f
            && channelRms(auxOutput, 0) > 1.0e-4f
            && channelRms(auxOutput, 1) > 1.0e-4f;
    });

    REQUIRE(heardRoutedVoice);

    auto mainOutput = getOutputBus(processor,
                                   outputBuffer,
                                   FamaLamaJamAudioProcessor::HostRoutingProof::kMainOutputBusIndex);
    auto auxOutput = getOutputBus(processor,
                                  outputBuffer,
                                  FamaLamaJamAudioProcessor::HostRoutingProof::kRoutedOutputBusIndex);
    CHECK(channelRms(auxOutput, 0) > channelRms(mainOutput, 0));
    CHECK(channelRms(auxOutput, 1) > channelRms(mainOutput, 1));
}
