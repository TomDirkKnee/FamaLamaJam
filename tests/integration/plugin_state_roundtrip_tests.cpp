#include <juce_core/juce_core.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;

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

template <typename T, std::enable_if_t<hasPasswordField<T>, int> = 0>
std::string readPasswordImpl(const T& settings)
{
    return settings.password;
}

template <typename T, std::enable_if_t<! hasPasswordField<T>, int> = 0>
std::string readPasswordImpl(const T&)
{
    return {};
}

std::string readPassword(const famalamajam::app::session::SessionSettings& settings)
{
    return readPasswordImpl(settings);
}

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server, double sampleRate, int blockSize)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "roundtrip_user";

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

TEST_CASE("plugin_state_roundtrip restores saved settings in fresh instance", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor source;

    auto draft = famalamajam::app::session::SessionSettings {
        .serverHost = "ninjam.example.org",
        .serverPort = 2050,
        .username = "roundtrip_user",
        .defaultChannelGainDb = -4.0f,
        .defaultChannelPan = 0.35f,
        .defaultChannelMuted = true,
    };
    setPassword(draft, "secret-room");

    famalamajam::app::session::SessionSettingsValidationResult validation;
    REQUIRE(source.applySettingsFromUi(draft, &validation));
    source.setMetronomeEnabled(true);

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    const auto active = restored.getActiveSettings();
    CHECK(active.serverHost == "ninjam.example.org");
    CHECK(active.serverPort == 2050);
    CHECK(active.username == "roundtrip_user");
    CHECK(active.defaultChannelGainDb == Catch::Approx(-4.0f));
    CHECK(active.defaultChannelPan == Catch::Approx(0.35f));
    CHECK(active.defaultChannelMuted == true);
    CHECK(hasPasswordField<famalamajam::app::session::SessionSettings>);
    CHECK(readPassword(active) == "secret-room");
    CHECK(restored.isMetronomeEnabled());
    CHECK(restored.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(restored.isSessionConnected());
    CHECK(restored.getLastStatusMessage() == "Settings restored");
}

TEST_CASE("plugin_state_roundtrip preserves remembered server history while clearing transient discovery state",
          "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor source;

    auto settings = source.getActiveSettings();
    settings.username = "history_user";

    settings.serverHost = "alpha.example.org";
    settings.serverPort = 2050;
    REQUIRE(source.applySettingsFromUi(settings));
    REQUIRE(source.requestConnect());
    source.handleConnectionEvent(ConnectionEvent { .type = ConnectionEventType::Connected });
    REQUIRE(source.requestDisconnect());

    settings.serverHost = "beta.example.org";
    settings.serverPort = 2051;
    REQUIRE(source.applySettingsFromUi(settings));
    REQUIRE(source.requestConnect());
    source.handleConnectionEvent(ConnectionEvent { .type = ConnectionEventType::Connected });
    REQUIRE(source.requestDisconnect());

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    const auto discovery = restored.getServerDiscoveryUiState();
    REQUIRE(discovery.combinedEntries.size() == 2);
    CHECK(discovery.combinedEntries[0].host == "beta.example.org");
    CHECK(discovery.combinedEntries[0].port == 2051);
    CHECK(discovery.combinedEntries[1].host == "alpha.example.org");
    CHECK(discovery.combinedEntries[1].port == 2050);
    CHECK_FALSE(discovery.fetchInProgress);
    CHECK_FALSE(discovery.hasStalePublicData);
    CHECK(discovery.statusText.empty());
    CHECK(restored.getLastStatusMessage() == "Settings restored");
}

TEST_CASE("plugin_state_roundtrip restores disconnected startup with brief last-error context", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor source;

    REQUIRE(source.requestConnect());
    source.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::ConnectionFailed,
        .reason = "auth token expired",
    });
    REQUIRE(source.getLifecycleSnapshot().state == ConnectionState::Error);

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    CHECK(restored.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(restored.isSessionConnected());
    CHECK(restored.getLastStatusMessage().find("Last error: auth token expired") != std::string::npos);

    REQUIRE(restored.requestConnect());
    CHECK(restored.getLastStatusMessage() == "Connecting");
}

TEST_CASE("plugin_state_roundtrip falls back safely for invalid state blob", "[plugin_state_roundtrip]")
{
    FamaLamaJamAudioProcessor processor;

    const unsigned char invalid[] {0xAA, 0x00, 0x11};
    processor.setStateInformation(invalid, static_cast<int>(sizeof(invalid)));

    const auto active = processor.getActiveSettings();
    const auto defaults = famalamajam::app::session::makeDefaultSessionSettings();

    CHECK(active == defaults);
    CHECK(processor.getLastStatusMessage().find("Defaults restored") != std::string::npos);
    CHECK_FALSE(processor.isSessionConnected());
}

TEST_CASE("plugin_state_roundtrip restores saved mixer state without reviving live runtime", "[plugin_state_roundtrip]")
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

    REQUIRE(source.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, -3.0f, 0.25f, true));
    REQUIRE(source.setMixerStripMixState("alice#0", -7.0f, -0.35f, true));
    source.setMetronomeEnabled(true);
    auto settings = source.getActiveSettings();
    setPassword(settings, "saved-room-password");
    REQUIRE(source.applySettingsFromUi(settings));

    juce::MemoryBlock state;
    source.getStateInformation(state);

    FamaLamaJamAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    FamaLamaJamAudioProcessor::MixerStripSnapshot localSnapshot;
    FamaLamaJamAudioProcessor::MixerStripSnapshot remoteSnapshot;
    REQUIRE(restored.getMixerStripSnapshot(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, localSnapshot));
    REQUIRE(restored.getMixerStripSnapshot("alice#0", remoteSnapshot));

    CHECK(restored.getLifecycleSnapshot().state == ConnectionState::Idle);
    CHECK_FALSE(restored.isSessionConnected());
    CHECK(restored.getScheduledReconnectDelayMs() == 0);
    CHECK_FALSE(restored.getTransportUiState().hasServerTiming);
    CHECK(restored.getActiveRemoteSourceCountForTesting() == 0);
    CHECK(restored.getQueuedRemoteSourceCountForTesting() == 0);
    CHECK(restored.getPendingRemoteSourceCountForTesting() == 0);
    CHECK(restored.isMetronomeEnabled());
    CHECK(restored.getLastStatusMessage() == "Settings restored");
    CHECK(readPassword(restored.getActiveSettings()) == "saved-room-password");

    CHECK(localSnapshot.mix.gainDb == Catch::Approx(-3.0f));
    CHECK(localSnapshot.mix.pan == Catch::Approx(0.25f));
    CHECK(localSnapshot.mix.muted);
    CHECK(localSnapshot.descriptor.active);
    CHECK(localSnapshot.descriptor.visible);

    CHECK(remoteSnapshot.mix.gainDb == Catch::Approx(-7.0f));
    CHECK(remoteSnapshot.mix.pan == Catch::Approx(-0.35f));
    CHECK(remoteSnapshot.mix.muted);
    CHECK(remoteSnapshot.descriptor.kind == FamaLamaJamAudioProcessor::MixerStripKind::RemoteDelayed);
    CHECK_FALSE(remoteSnapshot.descriptor.active);
    CHECK_FALSE(remoteSnapshot.descriptor.visible);
}


