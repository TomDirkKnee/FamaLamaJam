#include <functional>
#include <type_traits>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

template <typename T, typename = void>
constexpr bool hasTransmitStateField = false;

template <typename T>
constexpr bool hasTransmitStateField<T, std::void_t<decltype(std::declval<T&>().transmitState)>> = true;

template <typename T, typename = void>
constexpr bool hasSoloedField = false;

template <typename T>
constexpr bool hasSoloedField<T, std::void_t<decltype(std::declval<T&>().soloed)>> = true;

template <typename T, typename = void>
constexpr bool hasUnsupportedVoiceModeField = false;

template <typename T>
constexpr bool hasUnsupportedVoiceModeField<T,
                                            std::void_t<decltype(std::declval<T&>().unsupportedVoiceMode)>> = true;

template <typename T, typename = void>
constexpr bool hasStatusTextField = false;

template <typename T>
constexpr bool hasStatusTextField<T, std::void_t<decltype(std::declval<T&>().statusText)>> = true;

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
                  int attempts = 4000)
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

TEST_CASE("plugin transmit controls reserves processor-owned local strip lifecycle contracts",
          "[plugin_transmit_controls]")
{
    CHECK(hasTransmitStateField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);
    CHECK(hasStatusTextField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);
}

TEST_CASE("plugin transmit controls keeps solo separate from destructive mute state", "[plugin_transmit_controls]")
{
    CHECK(hasSoloedField<FamaLamaJamAudioProcessor::MixerStripMixState>);
    CHECK_FALSE(std::is_same_v<decltype(FamaLamaJamAudioProcessor::MixerStripMixState::muted), bool&>);
}

TEST_CASE("plugin transmit controls reserves unsupported-voice visibility alongside transmit state",
          "[plugin_transmit_controls]")
{
    CHECK(hasUnsupportedVoiceModeField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);
    CHECK(hasTransmitStateField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);
}

TEST_CASE("plugin transmit controls expects local strip snapshots to carry reconnect and upload gating semantics",
          "[plugin_transmit_controls]")
{
    FamaLamaJamAudioProcessor processor(true, true);
    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;

    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(hasTransmitStateField<decltype(localSnapshot)>);
    CHECK(hasStatusTextField<decltype(localSnapshot)>);
    CHECK(hasSoloedField<decltype(localSnapshot.mix)>);
}

TEST_CASE("plugin transmit controls toggles the local strip between active, disabled, and warmup states",
          "[plugin_transmit_controls]")
{
    MiniNinjamServer server;
    server.setInitialTiming(400, 1);
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(processUntil(processor, buffer, midi, [&]() {
        return processor.getTransmitState() == FamaLamaJamAudioProcessor::TransmitState::Active;
    }));

    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(localSnapshot.transmitState == FamaLamaJamAudioProcessor::TransmitState::Active);
    CHECK(localSnapshot.statusText == "Transmitting");
    CHECK(localSnapshot.fullStatusText == "Transmitting");

    CHECK_FALSE(processor.toggleTransmitEnabled());
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(localSnapshot.transmitState == FamaLamaJamAudioProcessor::TransmitState::Disabled);
    CHECK(localSnapshot.statusText == "TX off");
    CHECK(localSnapshot.fullStatusText == "Not transmitting");

    CHECK(processor.toggleTransmitEnabled());
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(localSnapshot.transmitState == FamaLamaJamAudioProcessor::TransmitState::WarmingUp);
    CHECK(localSnapshot.statusText == "Warming up");
    CHECK(localSnapshot.fullStatusText == "Getting ready to transmit");

    REQUIRE(processor.requestDisconnect());
    processor.releaseResources();
    server.stopServer();
}
