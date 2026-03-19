#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

template <typename T, typename = void>
constexpr bool hasPasswordField = false;

template <typename T>
constexpr bool hasPasswordField<T, std::void_t<decltype(std::declval<T&>().password)>> = true;

template <typename T, std::enable_if_t<hasPasswordField<T>, int> = 0>
void setPasswordImpl(T& settings, std::string password)
{
    settings.password = std::move(password);
}

template <typename T, std::enable_if_t<! hasPasswordField<T>, int> = 0>
void setPasswordImpl(T&, std::string)
{
}

void setPassword(famalamajam::app::session::SessionSettings& settings, std::string password)
{
    setPasswordImpl(settings, std::move(password));
}

bool waitForLifecycleState(famalamajam::plugin::FamaLamaJamAudioProcessor& processor,
                           famalamajam::app::session::ConnectionState expectedState,
                           int attempts = 200)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        if (processor.getLifecycleSnapshot().state == expectedState)
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

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

TEST_CASE("plugin experimental transport authenticates private-room passwords", "[plugin_experimental_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setAuthRules(MiniNinjamServer::AuthRules {
        .validatePassword = true,
        .expectedPassword = "secret-room",
        .failureText = "Wrong room password",
    });
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);

    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";
    setPassword(settings, "secret-room");

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());

    REQUIRE(waitForLifecycleState(processor, famalamajam::app::session::ConnectionState::Active));
    CHECK(processor.getLifecycleSnapshot().statusMessage.find("NINJAM auth ok") != std::string::npos);

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}

TEST_CASE("plugin experimental transport surfaces explicit wrong-password failures", "[plugin_experimental_transport]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    server.setAuthRules(MiniNinjamServer::AuthRules {
        .validatePassword = true,
        .expectedPassword = "secret-room",
        .failureText = "Wrong room password",
    });
    REQUIRE(server.startServer());

    famalamajam::plugin::FamaLamaJamAudioProcessor processor(true, true);

    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";
    setPassword(settings, "not-the-room-password");

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());

    REQUIRE(waitForLifecycleState(processor, famalamajam::app::session::ConnectionState::Error));
    CHECK(processor.getLifecycleSnapshot().lastError == "Wrong room password");
    CHECK(processor.getLastStatusMessage().find("Wrong room password") != std::string::npos);

    processor.releaseResources();
    server.stopServer();
}
