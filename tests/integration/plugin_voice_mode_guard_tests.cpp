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
constexpr bool hasChannelFlagsField = false;

template <typename T>
constexpr bool hasChannelFlagsField<T, std::void_t<decltype(std::declval<T&>().channelFlags)>> = true;

template <typename T, typename = void>
constexpr bool hasUnsupportedVoiceModeField = false;

template <typename T>
constexpr bool hasUnsupportedVoiceModeField<T,
                                            std::void_t<decltype(std::declval<T&>().unsupportedVoiceMode)>> = true;

bool waitUntil(FamaLamaJamAudioProcessor& processor,
               juce::AudioBuffer<float>& buffer,
               juce::MidiBuffer& midi,
               const std::function<bool()>& predicate,
               int attempts = 600)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        if (predicate())
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

float captureRemoteAudibleRms(FamaLamaJamAudioProcessor& processor,
                              juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midi,
                              int attempts = 120)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);

        const auto left = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto right = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        const auto rms = juce::jmax(left, right);
        if (rms > 1.0e-6f)
            return rms;

        juce::Thread::sleep(5);
    }

    return 0.0f;
}
} // namespace

TEST_CASE("plugin voice mode guard harness can advertise per-peer channel flags", "[plugin_voice_mode_guard]")
{
    CHECK(hasChannelFlagsField<MiniNinjamServer::RemotePeer>);

    MiniNinjamServer::RemotePeer peer;
    peer.channelFlags = 2;
    CHECK(peer.channelFlags == 2);
}

TEST_CASE("plugin voice mode guard reserves explicit unsupported state in normal mixer snapshots",
          "[plugin_voice_mode_guard]")
{
    CHECK(hasUnsupportedVoiceModeField<FamaLamaJamAudioProcessor::MixerStripSnapshot>);

    FamaLamaJamAudioProcessor processor;
    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    REQUIRE(processor.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    CHECK(hasUnsupportedVoiceModeField<decltype(localSnapshot)>);
}
