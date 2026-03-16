#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

void fillNoiseBuffer(juce::AudioBuffer<float>& buffer, juce::Random& random)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, random.nextFloat() * 2.0f - 1.0f);
    }
}
} // namespace

TEST_CASE("plugin experimental transport performs ninjam auth and codec roundtrip", "[plugin_experimental_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);
    CHECK(processor.isExperimentalStreamingEnabledForTesting());

    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));

    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());

    const auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == famalamajam::app::session::ConnectionState::Active);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int attempt = 0; attempt < 200; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (processor.getLastCodecPayloadBytesForTesting() > 0
            && processor.getLastDecodedSamplesForTesting() > 0
            && processor.getTransportSentFramesForTesting() > 0
            && processor.getTransportReceivedFramesForTesting() > 0)
        {
            break;
        }

        juce::Thread::sleep(5);
    }

    CHECK(processor.getLastCodecPayloadBytesForTesting() > 0);
    CHECK(processor.getLastDecodedSamplesForTesting() > 0);
    CHECK(processor.getTransportSentFramesForTesting() > 0);
    CHECK(processor.getTransportReceivedFramesForTesting() > 0);

    bool heardRemoteMix = false;

    for (int attempt = 0; attempt < 120; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);

        const auto leftRms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto rightRms = buffer.getRMSLevel(1, 0, buffer.getNumSamples());

        if (leftRms > 1.0e-4f || rightRms > 1.0e-4f)
        {
            heardRemoteMix = true;
            break;
        }

        juce::Thread::sleep(5);
    }

    CHECK(heardRemoteMix);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin experimental transport keeps large interval uploads connected", "[plugin_experimental_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(120, 4);
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);

    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));

    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    juce::Random random(12345);

    bool encodedLargeInterval = false;
    bool sentUpload = false;

    for (int attempt = 0; attempt < 400; ++attempt)
    {
        fillNoiseBuffer(buffer, random);
        processor.processBlock(buffer, midi);

        if (processor.getLastCodecPayloadBytesForTesting() > 16384)
            encodedLargeInterval = true;

        if (processor.getTransportSentFramesForTesting() > 0)
        {
            sentUpload = true;
            break;
        }

        juce::Thread::sleep(5);
    }

    REQUIRE(encodedLargeInterval);
    REQUIRE(sentUpload);
    CHECK(processor.getLifecycleSnapshot().state == famalamajam::app::session::ConnectionState::Active);

    buffer.clear();
    processor.processBlock(buffer, midi);
    CHECK(processor.getLifecycleSnapshot().state == famalamajam::app::session::ConnectionState::Active);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}
