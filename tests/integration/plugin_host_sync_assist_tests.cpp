#include <cstdint>

#include <catch2/catch_approx.hpp>
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

class TestPlayHead final : public juce::AudioPlayHead
{
public:
    void clear()
    {
        available_ = false;
        playing_ = false;
        hasTempo_ = false;
        tempoBpm_ = 0.0;
        hasPpqPosition_ = false;
        ppqPosition_ = 0.0;
        hasBarStartPpq_ = false;
        barStartPpq_ = 0.0;
        hasTimeSignature_ = false;
        numerator_ = 0;
        denominator_ = 0;
    }

    void setState(bool playing,
                  bool hasTempo,
                  double tempoBpm,
                  bool hasPpqPosition,
                  double ppqPosition,
                  bool hasBarStartPpq,
                  double barStartPpq,
                  bool hasTimeSignature = true,
                  int numerator = 4,
                  int denominator = 4)
    {
        available_ = true;
        playing_ = playing;
        hasTempo_ = hasTempo;
        tempoBpm_ = tempoBpm;
        hasPpqPosition_ = hasPpqPosition;
        ppqPosition_ = ppqPosition;
        hasBarStartPpq_ = hasBarStartPpq;
        barStartPpq_ = barStartPpq;
        hasTimeSignature_ = hasTimeSignature;
        numerator_ = numerator;
        denominator_ = denominator;
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        if (! available_)
            return {};

        PositionInfo position;
        position.setIsPlaying(playing_);

        if (hasTempo_)
            position.setBpm(tempoBpm_);

        if (hasPpqPosition_)
            position.setPpqPosition(ppqPosition_);

        if (hasBarStartPpq_)
            position.setPpqPositionOfLastBarStart(barStartPpq_);

        if (hasTimeSignature_)
            position.setTimeSignature(TimeSignature { numerator_, denominator_ });

        return position;
    }

private:
    bool available_ { false };
    bool playing_ { false };
    bool hasTempo_ { false };
    double tempoBpm_ { 0.0 };
    bool hasPpqPosition_ { false };
    double ppqPosition_ { 0.0 };
    bool hasBarStartPpq_ { false };
    double barStartPpq_ { 0.0 };
    bool hasTimeSignature_ { false };
    int numerator_ { 0 };
    int denominator_ { 0 };
};

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

    const auto snapshot = processor.getLifecycleSnapshot();
    REQUIRE(snapshot.state == ConnectionState::Active);
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

        const auto state = processor.getTransportUiState();
        if (state.hasServerTiming && state.beatsPerMinute > 0 && state.beatsPerInterval > 0)
            return true;

        juce::Thread::sleep(5);
    }

    return false;
}

float processSilence(FamaLamaJamAudioProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    processor.processBlock(buffer, midi);
    return juce::jmax(buffer.getRMSLevel(0, 0, buffer.getNumSamples()),
                      buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
}
} // namespace

TEST_CASE("plugin host sync assist blocks arming without timing or with tempo mismatch", "[plugin_host_sync_assist]")
{
    FamaLamaJamAudioProcessor processor(true, true);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);

    playHead.setState(false, true, 120.0, false, 0.0, false, 0.0);

    CHECK_FALSE(processor.toggleHostSyncAssistArm());

    auto assistState = processor.getHostSyncAssistUiState();
    CHECK_FALSE(assistState.armable);
    CHECK_FALSE(assistState.armed);
    CHECK(assistState.blocked);
    CHECK(assistState.blockReason == FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::MissingServerTiming);

    MiniNinjamServer server;
    REQUIRE(server.startServer());
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    playHead.setState(false, true, 119.0, false, 0.0, false, 0.0);
    fillRampBuffer(buffer);
    processor.processBlock(buffer, midi);

    CHECK_FALSE(processor.toggleHostSyncAssistArm());

    assistState = processor.getHostSyncAssistUiState();
    CHECK_FALSE(assistState.armable);
    CHECK_FALSE(assistState.armed);
    CHECK(assistState.blocked);
    CHECK(assistState.blockReason == FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::HostTempoMismatch);
    CHECK(assistState.targetBeatsPerMinute == 120);
    CHECK(assistState.targetBeatsPerInterval == 16);
}

TEST_CASE("plugin host sync assist aligns from host musical position and clears the one-shot arm", "[plugin_host_sync_assist]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);
    processor.setMetronomeEnabled(true);
    REQUIRE(processor.setMixerStripMixState(FamaLamaJamAudioProcessor::kLocalMonitorSourceId, 0.0f, 0.0f, true));

    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    playHead.setState(false, true, 120.0, true, 18.5, true, 16.0);
    fillRampBuffer(buffer);
    processor.processBlock(buffer, midi);

    REQUIRE(processor.toggleHostSyncAssistArm());

    const auto armedState = processor.getHostSyncAssistUiState();
    REQUIRE(armedState.armable);
    REQUIRE(armedState.armed);
    REQUIRE(armedState.waitingForHost);
    REQUIRE_FALSE(armedState.failed);

    const auto heldProgress = processor.getTransportUiState().intervalProgress;
    const auto heldIndex = processor.getTransportUiState().intervalIndex;

    bool remainedSilentWhileWaiting = true;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const auto rms = processSilence(processor, buffer, midi);
        const auto transport = processor.getTransportUiState();
        remainedSilentWhileWaiting = remainedSilentWhileWaiting && rms == Catch::Approx(0.0f).margin(1.0e-6f);
        REQUIRE(transport.intervalIndex == heldIndex);
        REQUIRE(transport.intervalProgress == Catch::Approx(heldProgress).margin(1.0e-6f));
    }

    CHECK(remainedSilentWhileWaiting);

    playHead.setState(true, true, 120.0, true, 18.5, true, 16.0);
    processSilence(processor, buffer, midi);

    const auto assistState = processor.getHostSyncAssistUiState();
    CHECK_FALSE(assistState.armed);
    CHECK_FALSE(assistState.waitingForHost);
    CHECK_FALSE(assistState.failed);

    const auto transport = processor.getTransportUiState();
    CHECK(transport.intervalProgress == Catch::Approx(2.5 / 16.0).margin(0.01));
    CHECK(transport.currentBeat == 3);
}

TEST_CASE("plugin host sync assist fails visibly on timing loss or missing host musical position", "[plugin_host_sync_assist]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);

    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    SECTION("timing loss clears the armed state")
    {
        playHead.setState(false, true, 120.0, true, 8.0, true, 4.0);
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        REQUIRE(processor.toggleHostSyncAssistArm());

        processor.handleConnectionEvent(ConnectionEvent {
            .type = ConnectionEventType::ConnectionLostRetryable,
            .reason = "timing dropped",
        });

        const auto assistState = processor.getHostSyncAssistUiState();
        CHECK_FALSE(assistState.armed);
        CHECK_FALSE(assistState.waitingForHost);
        CHECK(assistState.failed);
        CHECK(assistState.failureReason == FamaLamaJamAudioProcessor::HostSyncAssistFailureReason::TimingLost);
        CHECK_FALSE(processor.getTransportUiState().hasServerTiming);
    }

    SECTION("missing host musical position fails instead of falling back")
    {
        playHead.setState(false, true, 120.0, false, 0.0, false, 0.0);
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);

        REQUIRE(processor.toggleHostSyncAssistArm());

        const auto heldProgress = processor.getTransportUiState().intervalProgress;
        playHead.setState(true, true, 120.0, false, 0.0, false, 0.0);
        processSilence(processor, buffer, midi);

        const auto assistState = processor.getHostSyncAssistUiState();
        CHECK_FALSE(assistState.armed);
        CHECK_FALSE(assistState.waitingForHost);
        CHECK(assistState.failed);
        CHECK(assistState.failureReason
              == FamaLamaJamAudioProcessor::HostSyncAssistFailureReason::MissingHostMusicalPosition);
        CHECK(processor.getTransportUiState().intervalProgress == Catch::Approx(heldProgress).margin(1.0e-6f));
    }
}

TEST_CASE("plugin host sync assist stays one-shot after a successful aligned start", "[plugin_host_sync_assist]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);

    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    playHead.setState(false, true, 120.0, true, 20.0, true, 16.0);
    fillRampBuffer(buffer);
    processor.processBlock(buffer, midi);

    REQUIRE(processor.toggleHostSyncAssistArm());

    playHead.setState(true, true, 120.0, true, 20.0, true, 16.0);
    processSilence(processor, buffer, midi);

    auto assistState = processor.getHostSyncAssistUiState();
    REQUIRE_FALSE(assistState.armed);
    REQUIRE_FALSE(assistState.waitingForHost);
    REQUIRE_FALSE(assistState.failed);

    playHead.setState(false, true, 120.0, true, 20.0, true, 16.0);
    processSilence(processor, buffer, midi);
    playHead.setState(true, true, 120.0, true, 24.0, true, 20.0);
    processSilence(processor, buffer, midi);

    assistState = processor.getHostSyncAssistUiState();
    CHECK_FALSE(assistState.armed);
    CHECK_FALSE(assistState.waitingForHost);
    CHECK_FALSE(assistState.failed);
}

TEST_CASE("plugin host sync assist requires explicit re-arm after a failed start", "[plugin_host_sync_assist]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);

    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    REQUIRE(waitForAuthoritativeTiming(processor, buffer, midi));

    playHead.setState(false, true, 120.0, false, 0.0, false, 0.0);
    fillRampBuffer(buffer);
    processor.processBlock(buffer, midi);

    REQUIRE(processor.toggleHostSyncAssistArm());

    playHead.setState(true, true, 120.0, false, 0.0, false, 0.0);
    processSilence(processor, buffer, midi);

    auto assistState = processor.getHostSyncAssistUiState();
    REQUIRE(assistState.failed);
    REQUIRE_FALSE(assistState.armed);

    const auto failedProgress = processor.getTransportUiState().intervalProgress;

    playHead.setState(false, true, 120.0, true, 4.0, true, 0.0);
    processSilence(processor, buffer, midi);
    playHead.setState(true, true, 120.0, true, 4.0, true, 0.0);
    processSilence(processor, buffer, midi);

    assistState = processor.getHostSyncAssistUiState();
    CHECK(assistState.failed);
    CHECK(processor.getTransportUiState().intervalProgress == Catch::Approx(failedProgress).margin(1.0e-6f));

    playHead.setState(false, true, 120.0, true, 4.0, true, 0.0);
    processSilence(processor, buffer, midi);
    REQUIRE(processor.toggleHostSyncAssistArm());

    playHead.setState(true, true, 120.0, true, 4.0, true, 0.0);
    processSilence(processor, buffer, midi);

    assistState = processor.getHostSyncAssistUiState();
    CHECK_FALSE(assistState.armed);
    CHECK_FALSE(assistState.waitingForHost);
    CHECK_FALSE(assistState.failed);
}
