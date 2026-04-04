#include "FamaLamaJamAudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <unordered_set>

#include "audio/AudioBufferResampler.h"
#include "infra/net/PublicServerDiscoveryClient.h"
#include "infra/state/RememberedServerStore.h"
#include "infra/state/SessionSettingsSerializer.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"

namespace famalamajam::plugin
{
namespace
{
const juce::Identifier kPluginStateType("famalamajam.plugin.state");
const juce::Identifier kPluginStateSchemaVersion("schemaVersion");
const juce::Identifier kPluginStateLastErrorContext("lastErrorContext");
const juce::Identifier kPluginStateMetronomeEnabled("metronomeEnabled");
const juce::Identifier kPluginStateMetronomeGainDb("metronomeGainDb");
const juce::Identifier kPluginStateMasterOutputGainDb("masterOutputGainDb");
const juce::Identifier kPluginStateStemCaptureEnabled("stemCaptureEnabled");
const juce::Identifier kPluginStateStemCaptureOutputDirectory("stemCaptureOutputDirectory");
const juce::Identifier kPluginStateMixerState("mixerState");
const juce::Identifier kPluginStateMixerStrip("mixerStrip");
const juce::Identifier kPluginStateMixerStripKind("kind");
const juce::Identifier kPluginStateMixerStripSourceId("sourceId");
const juce::Identifier kPluginStateMixerStripGroupId("groupId");
const juce::Identifier kPluginStateMixerStripUserName("userName");
const juce::Identifier kPluginStateMixerStripChannelName("channelName");
const juce::Identifier kPluginStateMixerStripDisplayName("displayName");
const juce::Identifier kPluginStateMixerStripChannelIndex("channelIndex");
const juce::Identifier kPluginStateMixerStripGainDb("gainDb");
const juce::Identifier kPluginStateMixerStripPan("pan");
const juce::Identifier kPluginStateMixerStripMuted("muted");
const juce::Identifier kPluginStateMixerStripSoloed("soloed");
const juce::Identifier kPluginStateRememberedServerHistory("rememberedServerHistory");
const juce::Identifier kPluginStateRememberedServer("rememberedServer");
const juce::Identifier kPluginStateRememberedServerHost("host");
const juce::Identifier kPluginStateRememberedServerPort("port");
const juce::Identifier kPluginStateRememberedServerUsername("username");
const juce::Identifier kPluginStateRememberedServerPassword("password");
const juce::Identifier kSessionSettingsStateType("famalamajam.session.settings");
constexpr int kPluginStateSchema = 1;
constexpr std::size_t kMaxLastErrorContextLength = 64;
constexpr int kConnectTimeoutMs = 3000;
constexpr std::size_t kMaxRememberedServers = 12;
constexpr float kMinMetronomeGainDb = -60.0f;
constexpr float kMaxMetronomeGainDb = 12.0f;

[[nodiscard]] std::string shortenLastErrorContext(const std::string& context)
{
    if (context.size() <= kMaxLastErrorContextLength)
        return context;

    return context.substr(0, kMaxLastErrorContextLength - 3) + "...";
}

[[nodiscard]] std::string makeRestoreStatusMessage(bool usedFallback, const std::string& lastErrorContext)
{
    if (usedFallback)
        return "State invalid. Defaults restored.";

    if (lastErrorContext.empty())
        return "Settings restored";

    return "Settings restored. Last error: " + lastErrorContext;
}

[[nodiscard]] juce::ValueTree parseValueTree(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return {};

    return juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
}

[[nodiscard]] std::string makeConnectionFailureReason(const app::session::SessionSettings& settings)
{
    std::ostringstream stream;
    stream << "unable to reach " << settings.serverHost << ":" << settings.serverPort;
    return stream.str();
}

[[nodiscard]] std::string makeProtocolUsername(const std::string& username, const std::string& password)
{
    if (! password.empty())
        return username;

    if (username.rfind("anonymous", 0) == 0)
        return username;

    return "anonymous:" + username;
}

[[nodiscard]] bool isTruthyEnvironmentValue(const juce::String& value)
{
    const auto normalized = value.trim().toLowerCase();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

[[nodiscard]] bool isExperimentalStreamingEnabledByDefault()
{
    return isTruthyEnvironmentValue(
        juce::SystemStats::getEnvironmentVariable("FAMALAMAJAM_EXPERIMENTAL_STREAMING", juce::String()));
}

constexpr std::size_t kMaxQueuedIntervalsPerSource = 8;
constexpr std::size_t kMaxQueuedVoiceChunksPerSource = 32;
constexpr int kLocalVoiceChunkSamples = 2048;
constexpr int kPreparedTransmitIntervals = 2;
constexpr int kMinBpm = 20;
constexpr int kMaxBpm = 400;
constexpr int kMinBpi = 1;
constexpr int kMaxBpi = 256;
constexpr float kMetronomeClickDurationMs = 18.0f;
constexpr float kMetronomeDownbeatFrequencyHz = 1760.0f;
constexpr float kMetronomeBeatFrequencyHz = 1320.0f;
constexpr float kMetronomeDownbeatGain = 0.18f;
constexpr float kMetronomeBeatGain = 0.12f;
constexpr double kHostTempoMatchToleranceBpm = 0.05;
constexpr std::uint64_t kInactiveStripRetentionIntervals = 2;
constexpr float kMinMixerGainDb = -60.0f;
constexpr float kMaxMixerGainDb = 12.0f;
constexpr std::size_t kMaxRoomFeedEntries = 128;

[[nodiscard]] std::string trimRoomText(std::string text)
{
    return juce::String(text).trim().toStdString();
}

[[nodiscard]] std::string makeDiscoveryEntryLabel(std::string_view host, std::uint16_t port)
{
    return std::string(host) + ":" + std::to_string(port);
}

[[nodiscard]] std::string makePublicDiscoveryEntryLabel(const app::session::ParsedPublicServerEntry& entry)
{
    auto label = makeDiscoveryEntryLabel(entry.host, entry.port);

    if (! entry.infoText.empty())
        label += " - " + entry.infoText;

    if (entry.connectedUsers >= 0 && entry.maxUsers >= 0)
        label += " (" + std::to_string(entry.connectedUsers) + "/" + std::to_string(entry.maxUsers) + " users)";
    else if (entry.connectedUsers >= 0)
        label += " (" + std::to_string(entry.connectedUsers) + " users)";
    else if (! entry.usersText.empty())
        label += " (" + entry.usersText + ")";

    return label;
}

[[nodiscard]] juce::String toLowerString(const std::string& text)
{
    return juce::String(text).toLowerCase();
}

[[nodiscard]] bool isVoteSystemText(const std::string& text)
{
    const auto normalized = toLowerString(text);
    return normalized.contains("[voting system]")
        || (normalized.contains("vote") && (normalized.contains("bpm") || normalized.contains("bpi")));
}

[[nodiscard]] bool voteTextMatchesRequest(const std::string& text,
                                          FamaLamaJamAudioProcessor::RoomVoteKind kind,
                                          int requestedValue)
{
    if (requestedValue <= 0)
        return false;

    const auto normalized = toLowerString(text);
    const auto kindToken = kind == FamaLamaJamAudioProcessor::RoomVoteKind::Bpm ? "bpm" : "bpi";
    return normalized.contains(kindToken) && normalized.contains(std::to_string(requestedValue));
}

[[nodiscard]] bool voteTextLooksFailed(const std::string& text)
{
    const auto normalized = toLowerString(text);
    return normalized.contains("fail")
        || normalized.contains("denied")
        || normalized.contains("reject")
        || normalized.contains("not allowed")
        || normalized.contains("permission");
}

[[nodiscard]] std::string makeVoteCommandText(FamaLamaJamAudioProcessor::RoomVoteKind kind, int value)
{
    return kind == FamaLamaJamAudioProcessor::RoomVoteKind::Bpm
        ? "!vote bpm " + std::to_string(value)
        : "!vote bpi " + std::to_string(value);
}

[[nodiscard]] int computeIntervalSamplesFromServerTiming(double sampleRate, int bpm, int bpi)
{
    if (sampleRate <= 0.0)
        return 0;

    if (bpm < kMinBpm || bpm > kMaxBpm || bpi < kMinBpi || bpi > kMaxBpi)
        return 0;

    const auto intervalSeconds = (60.0 * static_cast<double>(bpi)) / static_cast<double>(bpm);
    const auto intervalSamples = static_cast<int>(std::llround(intervalSeconds * sampleRate));
    return juce::jmax(1, intervalSamples);
}

[[nodiscard]] int computeBeatSamplesFromServerTiming(double sampleRate, int bpm)
{
    if (sampleRate <= 0.0 || bpm < kMinBpm || bpm > kMaxBpm)
        return 0;

    const auto beatSeconds = 60.0 / static_cast<double>(bpm);
    const auto beatSamples = static_cast<int>(std::llround(beatSeconds * sampleRate));
    return juce::jmax(1, beatSamples);
}

[[nodiscard]] FamaLamaJamAudioProcessor::SyncHealth computeSyncHealth(const app::session::ConnectionLifecycleSnapshot& snapshot,
                                                                      bool hasServerTiming)
{
    switch (snapshot.state)
    {
        case app::session::ConnectionState::Idle:
            return FamaLamaJamAudioProcessor::SyncHealth::Disconnected;

        case app::session::ConnectionState::Connecting:
            return FamaLamaJamAudioProcessor::SyncHealth::WaitingForTiming;

        case app::session::ConnectionState::Active:
            return hasServerTiming ? FamaLamaJamAudioProcessor::SyncHealth::Healthy
                                   : FamaLamaJamAudioProcessor::SyncHealth::WaitingForTiming;

        case app::session::ConnectionState::Reconnecting:
            return FamaLamaJamAudioProcessor::SyncHealth::Reconnecting;

        case app::session::ConnectionState::Error:
            return FamaLamaJamAudioProcessor::SyncHealth::TimingLost;
    }

    return FamaLamaJamAudioProcessor::SyncHealth::Disconnected;
}

[[nodiscard]] FamaLamaJamAudioProcessor::HostTransportSnapshot captureHostTransportSnapshot(const juce::AudioPlayHead* playHead)
{
    FamaLamaJamAudioProcessor::HostTransportSnapshot snapshot;

    if (playHead == nullptr)
        return snapshot;

    const auto position = playHead->getPosition();
    if (! position.hasValue())
        return snapshot;

    snapshot.available = true;
    snapshot.playing = position->getIsPlaying();

    if (const auto bpm = position->getBpm())
    {
        snapshot.hasTempo = true;
        snapshot.tempoBpm = *bpm;
    }

    if (const auto ppqPosition = position->getPpqPosition())
    {
        snapshot.hasPpqPosition = true;
        snapshot.ppqPosition = *ppqPosition;
    }

    if (const auto barStart = position->getPpqPositionOfLastBarStart())
    {
        snapshot.hasBarStartPpqPosition = true;
        snapshot.ppqPositionOfLastBarStart = *barStart;
    }

    if (const auto timeSignature = position->getTimeSignature())
    {
        snapshot.hasTimeSignature = true;
        snapshot.timeSigNumerator = timeSignature->numerator;
        snapshot.timeSigDenominator = timeSignature->denominator;
    }

    return snapshot;
}

[[nodiscard]] bool hostTempoMatchesRoom(const FamaLamaJamAudioProcessor::HostTransportSnapshot& hostTransport, int roomBpm)
{
    return hostTransport.hasTempo
        && roomBpm > 0
        && std::abs(hostTransport.tempoBpm - static_cast<double>(roomBpm)) <= kHostTempoMatchToleranceBpm;
}

[[nodiscard]] FamaLamaJamAudioProcessor::HostSyncAssistBlockReason computeHostSyncAssistBlockReason(
    bool hasServerTiming,
    int roomBpm,
    const FamaLamaJamAudioProcessor::HostTransportSnapshot& hostTransport)
{
    if (! hasServerTiming || roomBpm <= 0)
        return FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::MissingServerTiming;

    if (! hostTransport.hasTempo)
        return FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::MissingHostTempo;

    if (! hostTempoMatchesRoom(hostTransport, roomBpm))
        return FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::HostTempoMismatch;

    return FamaLamaJamAudioProcessor::HostSyncAssistBlockReason::None;
}

[[nodiscard]] int computeHostSyncAssistStartOffsetSamples(
    const FamaLamaJamAudioProcessor::AuthoritativeTimingState& timing,
    const FamaLamaJamAudioProcessor::HostTransportSnapshot& hostTransport)
{
    if (! timing.hasTiming || timing.activeIntervalSamples <= 0 || timing.activeBeatSamples <= 0)
        return -1;

    if (! hostTransport.hasPpqPosition || ! hostTransport.hasBarStartPpqPosition)
        return -1;

    const auto beatsIntoBar = hostTransport.ppqPosition - hostTransport.ppqPositionOfLastBarStart;
    if (! std::isfinite(beatsIntoBar) || beatsIntoBar < -1.0e-6)
        return -1;

    const auto intervalBeats = static_cast<double>(juce::jmax(1, timing.activeBpi));
    auto beatsIntoInterval = std::fmod(hostTransport.ppqPositionOfLastBarStart + beatsIntoBar, intervalBeats);
    if (beatsIntoInterval < 0.0)
        beatsIntoInterval += intervalBeats;

    const auto rawOffsetSamples = static_cast<int>(std::llround(beatsIntoInterval
                                                                * static_cast<double>(timing.activeBeatSamples)));
    if (rawOffsetSamples <= 0)
        return 0;

    return rawOffsetSamples % timing.activeIntervalSamples;
}

void activateTimingConfig(FamaLamaJamAudioProcessor::AuthoritativeTimingState& timing,
                          int bpm,
                          int bpi,
                          int intervalSamples,
                          int beatSamples)
{
    timing.hasTiming = intervalSamples > 0 && beatSamples > 0;
    timing.activeBpm = bpm;
    timing.activeBpi = bpi;
    timing.activeIntervalSamples = intervalSamples;
    timing.activeBeatSamples = beatSamples;
    timing.pendingBpm = 0;
    timing.pendingBpi = 0;
    timing.pendingIntervalSamples = 0;
    timing.pendingBeatSamples = 0;
    timing.hasPendingTiming = false;
}

void stageTimingConfig(FamaLamaJamAudioProcessor::AuthoritativeTimingState& timing,
                       int bpm,
                       int bpi,
                       int intervalSamples,
                       int beatSamples)
{
    timing.pendingBpm = bpm;
    timing.pendingBpi = bpi;
    timing.pendingIntervalSamples = intervalSamples;
    timing.pendingBeatSamples = beatSamples;
    timing.hasPendingTiming = intervalSamples > 0 && beatSamples > 0;
}

void queueCompletedRemoteInterval(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    const std::string& sourceId,
    juce::AudioBuffer<float> completedInterval,
    std::uint64_t targetIntervalIndex)
{
    if (completedInterval.getNumChannels() <= 0 || completedInterval.getNumSamples() <= 0)
        return;

    auto& queuedIntervals = queuedBySource[sourceId];
    queuedIntervals.push_back(FamaLamaJamAudioProcessor::RemoteQueuedInterval {
        .audio = std::move(completedInterval),
        .targetIntervalIndex = targetIntervalIndex,
    });

    while (queuedIntervals.size() > kMaxQueuedIntervalsPerSource)
        queuedIntervals.pop_front();
}

void queueDecodedRemoteInterval(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    std::unordered_map<std::string, std::uint64_t>& nextTargetBoundaryBySource,
    std::unordered_map<std::string, FamaLamaJamAudioProcessor::RemoteReceiveDiagnosticRuntime>& diagnosticsBySource,
    const FamaLamaJamAudioProcessor::AuthoritativeTimingState& timing,
    const std::string& sourceId,
    const juce::AudioBuffer<float>& decoded)
{
    if (! timing.hasTiming || timing.activeIntervalSamples <= 0)
        return;

    if (decoded.getNumChannels() <= 0 || decoded.getNumSamples() <= 0)
        return;

    juce::AudioBuffer<float> normalized(decoded.getNumChannels(), timing.activeIntervalSamples);
    normalized.clear();

    const auto copiedSamples = juce::jmin(decoded.getNumSamples(), timing.activeIntervalSamples);
    for (int channel = 0; channel < decoded.getNumChannels(); ++channel)
        normalized.copyFrom(channel, 0, decoded, channel, 0, copiedSamples);

    const auto earliestPlayableBoundary = timing.intervalIndex + 1;
    auto& nextTargetBoundary = nextTargetBoundaryBySource[sourceId];
    auto& diagnostics = diagnosticsBySource[sourceId];

    if (nextTargetBoundary < earliestPlayableBoundary)
    {
        if (nextTargetBoundary != 0)
        {
            ++diagnostics.lateDrops;
            diagnostics.lastCopiedSamples = copiedSamples;
            diagnostics.lastDropTargetBoundary = nextTargetBoundary;
            diagnostics.lastDropCurrentBoundary = earliestPlayableBoundary;
            diagnostics.lastQueuedBoundary = earliestPlayableBoundary;
            queueCompletedRemoteInterval(queuedBySource, sourceId, std::move(normalized), earliestPlayableBoundary);
            nextTargetBoundary = earliestPlayableBoundary + 1;
            return;
        }

        nextTargetBoundary = earliestPlayableBoundary;
    }

    diagnostics.lastCopiedSamples = copiedSamples;
    diagnostics.lastQueuedBoundary = nextTargetBoundary;
    queueCompletedRemoteInterval(queuedBySource, sourceId, std::move(normalized), nextTargetBoundary);
    ++nextTargetBoundary;
}

void activateRemoteIntervalsForBoundary(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    std::unordered_map<std::string, juce::AudioBuffer<float>>& activeBySource,
    std::unordered_map<std::string, std::uint64_t>& activationBoundaryBySource,
    std::uint64_t boundaryIntervalIndex)
{
    activeBySource.clear();

    for (auto sourceIt = queuedBySource.begin(); sourceIt != queuedBySource.end();)
    {
        auto& queuedIntervals = sourceIt->second;

        while (! queuedIntervals.empty() && queuedIntervals.front().targetIntervalIndex < boundaryIntervalIndex)
            queuedIntervals.pop_front();

        if (! queuedIntervals.empty() && queuedIntervals.front().targetIntervalIndex == boundaryIntervalIndex)
        {
            activeBySource.emplace(sourceIt->first, std::move(queuedIntervals.front().audio));
            activationBoundaryBySource[sourceIt->first] = boundaryIntervalIndex;
            queuedIntervals.pop_front();
        }

        if (queuedIntervals.empty())
            sourceIt = queuedBySource.erase(sourceIt);
        else
            ++sourceIt;
    }
}

[[nodiscard]] FamaLamaJamAudioProcessor::MixerStripMixState normalizeMixState(
    FamaLamaJamAudioProcessor::MixerStripMixState mix)
{
    mix.gainDb = juce::jlimit(kMinMixerGainDb, kMaxMixerGainDb, mix.gainDb);
    mix.pan = juce::jlimit(-1.0f, 1.0f, mix.pan);
    return mix;
}

[[nodiscard]] float normalizeMasterOutputGainDb(float gainDb)
{
    return juce::jlimit(kMinMixerGainDb, kMaxMixerGainDb, gainDb);
}

[[nodiscard]] float normalizeMetronomeGainDb(float gainDb)
{
    return juce::jlimit(kMinMetronomeGainDb, kMaxMetronomeGainDb, gainDb);
}

[[nodiscard]] FamaLamaJamAudioProcessor::MixerStripMixState makeUnityMixerStripMixState()
{
    return FamaLamaJamAudioProcessor::MixerStripMixState {};
}

[[nodiscard]] bool isVoiceMode(std::uint8_t channelFlags)
{
    return (channelFlags & 0x2u) != 0;
}

[[nodiscard]] bool isLocalVoiceMode(FamaLamaJamAudioProcessor::LocalChannelMode mode)
{
    return mode == FamaLamaJamAudioProcessor::LocalChannelMode::Voice;
}

[[nodiscard]] std::string makeLocalTransmitStatus(FamaLamaJamAudioProcessor::LocalChannelMode mode,
                                                  FamaLamaJamAudioProcessor::TransmitState state,
                                                  bool voiceTransitionPending)
{
    if (isLocalVoiceMode(mode))
    {
        if (state == FamaLamaJamAudioProcessor::TransmitState::Disabled)
            return "Voice mode ready";
        if (voiceTransitionPending)
            return "Switching to voice mode...";
        return "Voice chat: low quality, near realtime";
    }

    switch (state)
    {
        case FamaLamaJamAudioProcessor::TransmitState::Disabled:
            return "Not transmitting";
        case FamaLamaJamAudioProcessor::TransmitState::WarmingUp:
            return "Getting ready to transmit";
        case FamaLamaJamAudioProcessor::TransmitState::Active:
            return "Transmitting";
    }

    return {};
}

void queueDecodedRemoteVoiceChunk(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteVoiceChunk>>& queuedBySource,
    const std::string& sourceId,
    juce::AudioBuffer<float> decoded)
{
    if (decoded.getNumChannels() <= 0 || decoded.getNumSamples() <= 0)
        return;

    auto& queuedChunks = queuedBySource[sourceId];
    queuedChunks.push_back(FamaLamaJamAudioProcessor::RemoteVoiceChunk {
        .audio = std::move(decoded),
        .playbackPosition = 0,
    });

    while (queuedChunks.size() > kMaxQueuedVoiceChunksPerSource)
        queuedChunks.pop_front();
}

void computePanGains(float pan, float& left, float& right, float& other)
{
    const auto clampedPan = juce::jlimit(-1.0f, 1.0f, pan);
    const auto panAngle = (clampedPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    left = std::cos(panAngle);
    right = std::sin(panAngle);
    other = 0.5f * (left + right);
}

void clearBufferAndMeter(juce::AudioBuffer<float>& buffer, FamaLamaJamAudioProcessor::MixerStripMeter& meter)
{
    buffer.clear();
    meter = {};
}

void applyMixToBuffer(juce::AudioBuffer<float>& buffer,
                      const FamaLamaJamAudioProcessor::MixerStripMixState& mix,
                      FamaLamaJamAudioProcessor::MixerStripMeter& meter)
{
    if (mix.muted)
    {
        clearBufferAndMeter(buffer, meter);
        return;
    }

    const auto gainLinear = juce::Decibels::decibelsToGain(mix.gainDb);
    float panLeft = 1.0f;
    float panRight = 1.0f;
    float panOther = 1.0f;
    computePanGains(mix.pan, panLeft, panRight, panOther);

    meter = {};

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float channelGain = gainLinear;
        if (buffer.getNumChannels() > 1)
        {
            if (channel == 0)
                channelGain *= panLeft;
            else if (channel == 1)
                channelGain *= panRight;
            else
                channelGain *= panOther;
        }

        auto* samples = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            samples[sample] *= channelGain;
            const auto magnitude = std::abs(samples[sample]);
            if (channel == 0)
                meter.left = juce::jmax(meter.left, magnitude);
            else if (channel == 1)
                meter.right = juce::jmax(meter.right, magnitude);
        }
    }

    if (buffer.getNumChannels() == 1)
        meter.right = meter.left;
}

[[nodiscard]] bool isStereoOrDisabled(const juce::AudioChannelSet& layout)
{
    return layout == juce::AudioChannelSet::disabled() || layout == juce::AudioChannelSet::stereo();
}

[[nodiscard]] bool bufferHasSignal(const juce::AudioBuffer<float>& buffer, float epsilon = 1.0e-5f)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax(buffer.getReadPointer(channel),
                                                                      buffer.getNumSamples());
        if (std::abs(range.getStart()) > epsilon || std::abs(range.getEnd()) > epsilon)
            return true;
    }

    return false;
}

void copyBuffer(juce::AudioBuffer<float>& destination, const juce::AudioBuffer<float>& source)
{
    if (source.getNumChannels() <= 0 || source.getNumSamples() <= 0)
    {
        destination.setSize(0, 0);
        return;
    }

    destination.setSize(source.getNumChannels(), source.getNumSamples(), false, true, true);
    destination.makeCopyOf(source, true);
}

void mixSourceSampleToOutput(const juce::AudioBuffer<float>& sourceBuffer,
                             int sourceSample,
                             juce::AudioBuffer<float>& targetBuffer,
                             int targetSample,
                             const FamaLamaJamAudioProcessor::MixerStripMixState& mix,
                             FamaLamaJamAudioProcessor::MixerStripMeter& meter,
                             FamaLamaJamAudioProcessor::CpuDiagnosticSnapshot& diagnostics)
{
    if (sourceSample < 0 || targetSample < 0 || sourceSample >= sourceBuffer.getNumSamples()
        || targetSample >= targetBuffer.getNumSamples())
        return;

    const auto sourceChannels = sourceBuffer.getNumChannels();
    const auto targetChannels = targetBuffer.getNumChannels();
    if (sourceChannels <= 0 || targetChannels <= 0)
        return;

    ++diagnostics.remoteMixSourceVisits;

    float panLeft = 1.0f;
    float panRight = 1.0f;
    float panOther = 1.0f;
    computePanGains(mix.pan, panLeft, panRight, panOther);
    const auto gainLinear = juce::Decibels::decibelsToGain(mix.gainDb);

    for (int channel = 0; channel < targetChannels; ++channel)
    {
        const auto sourceChannel = sourceChannels == 1 ? 0 : juce::jmin(channel, sourceChannels - 1);
        float channelGain = gainLinear * panOther;
        if (targetChannels > 1)
        {
            if (channel == 0)
                channelGain = gainLinear * panLeft;
            else if (channel == 1)
                channelGain = gainLinear * panRight;
        }

        const auto mixedSample = sourceBuffer.getSample(sourceChannel, sourceSample) * channelGain;
        targetBuffer.addSample(channel, targetSample, mixedSample);
        ++diagnostics.remoteMixChannelWrites;

        const auto magnitude = std::abs(mixedSample);
        if (channel == 0)
            meter.left = juce::jmax(meter.left, magnitude);
        else if (channel == 1)
            meter.right = juce::jmax(meter.right, magnitude);
    }
}

template <typename StripMap>
[[nodiscard]] bool anySoloActive(const StripMap& stripsBySource)
{
    return std::any_of(stripsBySource.begin(),
                       stripsBySource.end(),
                       [](const auto& entry) { return entry.second.snapshot.mix.soloed; });
}

[[nodiscard]] bool isStripAudible(const FamaLamaJamAudioProcessor::MixerStripMixState& mix, bool soloActive)
{
    if (mix.muted)
        return false;

    return ! soloActive || mix.soloed;
}
} // namespace
FamaLamaJamAudioProcessor::FamaLamaJamAudioProcessor(bool enableLiveTransport, bool enableExperimentalStreaming)
    : FamaLamaJamAudioProcessor(
          famalamajam::infra::state::makeRememberedServerStore(), enableLiveTransport, enableExperimentalStreaming)
{
}

FamaLamaJamAudioProcessor::FamaLamaJamAudioProcessor(
    std::unique_ptr<famalamajam::infra::state::RememberedServerStore> rememberedServerStore,
    bool enableLiveTransport,
    bool enableExperimentalStreaming)
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                            .withInput(kHostRoutingProofAuxInputBusName,
                                                       juce::AudioChannelSet::stereo(),
                                                       true)
                                            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                            .withOutput(kHostRoutingProofAuxOutputBusName,
                                                        juce::AudioChannelSet::stereo(),
                                                        true))
    , settingsController_(settingsStore_)
    , liveTransportEnabled_(enableLiveTransport)
    , experimentalStreamingEnabled_(enableExperimentalStreaming)
    , publicServerDiscoveryClient_(infra::net::makePublicServerDiscoveryClient())
    , rememberedServerStore_(std::move(rememberedServerStore))
    , stemCaptureWriter_(std::make_unique<audio::StemCaptureWriter>())
{
    enableAllBuses();
    loadRememberedServersFromStore();
    ensureLocalMonitorMixerStrip();
}

FamaLamaJamAudioProcessor::~FamaLamaJamAudioProcessor()
{
    stemCaptureWriter_->stop();
    codecStreamBridge_.stop();
    clearReconnectTimer();
    closeLiveSocket();
}

const juce::String FamaLamaJamAudioProcessor::getName() const { return "FamaLamaJam"; }
bool FamaLamaJamAudioProcessor::acceptsMidi() const { return false; }
bool FamaLamaJamAudioProcessor::producesMidi() const { return false; }
bool FamaLamaJamAudioProcessor::isMidiEffect() const { return false; }
double FamaLamaJamAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int FamaLamaJamAudioProcessor::getNumPrograms() { return 1; }
int FamaLamaJamAudioProcessor::getCurrentProgram() { return 0; }
void FamaLamaJamAudioProcessor::setCurrentProgram(int) {}
const juce::String FamaLamaJamAudioProcessor::getProgramName(int) { return {}; }
void FamaLamaJamAudioProcessor::changeProgramName(int, const juce::String&) {}

void FamaLamaJamAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const bool sampleRateChanged = sampleRate > 0.0 && ! juce::approximatelyEqual(currentSampleRate_, sampleRate);
    const bool blockSizeChanged = samplesPerBlock > 0 && currentSamplesPerBlock_ != samplesPerBlock;

    if (sampleRate > 0.0)
        currentSampleRate_ = sampleRate;
    if (samplesPerBlock > 0)
        currentSamplesPerBlock_ = samplesPerBlock;

    if (sampleRateChanged || blockSizeChanged)
    {
        stemCaptureWriter_->stop();
        stemCaptureWriter_->start();
        codecStreamBridge_.stop();
        if (isSessionConnected())
            codecStreamBridge_.start();

        localUploadIntervalBuffer_.setSize(0, 0);
        localVoiceUploadBuffer_.setSize(0, 0);
        hostRoutingProofAuxSendBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        localVoiceUploadWritePosition_ = 0;
        lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
        lastDecodedSamples_.store(0, std::memory_order_relaxed);
        remoteQueuedIntervalsBySource_.clear();
        remoteVoiceChunksBySource_.clear();
        nextRemoteTargetBoundaryBySource_.clear();
        remoteActiveIntervalBySource_.clear();
        lastRemoteActivationBoundaryBySource_.clear();
        remoteReceiveDiagnosticsBySource_.clear();
        knownRemoteSourcesById_.clear();
        clearAuthoritativeTiming();
        resetMixerStripMeters();
        updateRemoteMixerStripActivity();
        return;
    }

    if (isSessionConnected())
        codecStreamBridge_.start();
}

void FamaLamaJamAudioProcessor::releaseResources()
{
    const auto snapshot = lifecycleController_.getSnapshot();

    if (snapshot.hasPendingRetry())
        applyLifecycleTransition(lifecycleController_.resetToIdle());

    stemCaptureWriter_->stop();
    codecStreamBridge_.stop();
    localUploadIntervalBuffer_.setSize(0, 0);
    localVoiceUploadBuffer_.setSize(0, 0);
    hostRoutingProofAuxSendBuffer_.setSize(0, 0);
    localUploadIntervalWritePosition_ = 0;
    localVoiceUploadWritePosition_ = 0;
    lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
    lastDecodedSamples_.store(0, std::memory_order_relaxed);
    remoteQueuedIntervalsBySource_.clear();
    remoteVoiceChunksBySource_.clear();
    nextRemoteTargetBoundaryBySource_.clear();
    remoteActiveIntervalBySource_.clear();
    lastRemoteActivationBoundaryBySource_.clear();
    remoteReceiveDiagnosticsBySource_.clear();
    knownRemoteSourcesById_.clear();
    clearAuthoritativeTiming();
    clearRoomUiState(false);
    resetMixerStripMeters();
    updateRemoteMixerStripActivity();
}

bool FamaLamaJamAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 2 || layouts.outputBuses.size() != 2)
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && isStereoOrDisabled(layouts.getChannelSet(true, HostRoutingProof::kAuxInputBusIndex))
        && isStereoOrDisabled(layouts.getChannelSet(false, HostRoutingProof::kRoutedOutputBusIndex));
}

void FamaLamaJamAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    ++cpuDiagnosticSnapshot_.processBlockCalls;
    ensureLocalMonitorMixerStrip();
    resetMixerStripMeters();
    const auto expectedAggregateChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    const bool hasFullBusLayout = buffer.getNumChannels() >= expectedAggregateChannels;
    auto mainInput = hasFullBusLayout ? getBusBuffer(buffer, true, HostRoutingProof::kMainInputBusIndex) : buffer;
    auto auxInput = hasFullBusLayout && getBusCount(true) > HostRoutingProof::kAuxInputBusIndex
        ? getBusBuffer(buffer, true, HostRoutingProof::kAuxInputBusIndex)
        : juce::AudioBuffer<float>();
    auto mainOutput = hasFullBusLayout ? getBusBuffer(buffer, false, HostRoutingProof::kMainOutputBusIndex) : buffer;
    auto auxOutput = hasFullBusLayout && getBusCount(false) > HostRoutingProof::kRoutedOutputBusIndex
        ? getBusBuffer(buffer, false, HostRoutingProof::kRoutedOutputBusIndex)
        : juce::AudioBuffer<float>();

    hostRoutingProof_.mainPathActive = bufferHasSignal(mainInput);
    hostRoutingProof_.auxInputActive = bufferHasSignal(auxInput);
    hostRoutingProof_.selectedRoutedSourceId = hostRoutingProofRoute_.sourceId;
    copyBuffer(hostRoutingProofAuxSendBuffer_, auxInput);

    if (auxOutput.getNumChannels() > 0)
        auxOutput.clear();

    auto clearTransportUi = [this]() {
        currentBeatForUi_.store(0, std::memory_order_relaxed);
        intervalProgressForUi_.store(0.0f, std::memory_order_relaxed);
        intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);
    };

    auto computeTransmitState = [this]() {
        if (! transmitEnabled_)
            return TransmitState::Disabled;

        if (! isSessionConnected())
            return TransmitState::Disabled;

        if (! authoritativeTiming_.hasTiming || transmitWarmupIntervalsRemaining_ > 0)
            return TransmitState::WarmingUp;

        return TransmitState::Active;
    };

    auto applyLocalMonitorToOutput = [this, &mainOutput, &computeTransmitState]() {
        auto& localStrip = mixerStripsBySourceId_[kLocalMonitorSourceId];
        const auto soloActive = anySoloActive(mixerStripsBySourceId_);
        if (isStripAudible(localStrip.snapshot.mix, soloActive))
            applyMixToBuffer(mainOutput, localStrip.snapshot.mix, localStrip.snapshot.meter);
        else
            clearBufferAndMeter(mainOutput, localStrip.snapshot.meter);

        localStrip.snapshot.descriptor.active = true;
        localStrip.snapshot.descriptor.visible = true;
        localStrip.snapshot.descriptor.inactiveIntervals = 0;
        localStrip.snapshot.transmitState = computeTransmitState();
        localStrip.snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
        localStrip.snapshot.unsupportedVoiceMode = false;
        localStrip.snapshot.statusText = makeLocalTransmitStatus(localChannelMode_,
                                                                 localStrip.snapshot.transmitState,
                                                                 localVoiceTransitionPending_);
        localStrip.lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex;
        localStrip.lastAudioIntervalIndex = authoritativeTiming_.intervalIndex;
    };

    auto applyMasterOutputToBuffer = [this, &mainOutput]() {
        const auto gainDb = masterOutputGainDb_.load(std::memory_order_relaxed);
        if (juce::approximatelyEqual(gainDb, 0.0f))
            return;

        mainOutput.applyGain(juce::Decibels::decibelsToGain(gainDb));
    };

    const auto hostTransport = captureHostTransportSnapshot(getPlayHead());

    auto storeHostTransportSnapshot = [this](const HostTransportSnapshot& snapshot) {
        hostTransportAvailableForUi_.store(snapshot.available, std::memory_order_relaxed);
        hostTransportPlayingForUi_.store(snapshot.playing, std::memory_order_relaxed);
        hostTransportHasTempoForUi_.store(snapshot.hasTempo, std::memory_order_relaxed);
        hostTransportTempoBpmForUi_.store(snapshot.tempoBpm, std::memory_order_relaxed);
        hostTransportHasPpqPositionForUi_.store(snapshot.hasPpqPosition, std::memory_order_relaxed);
        hostTransportPpqPositionForUi_.store(snapshot.ppqPosition, std::memory_order_relaxed);
        hostTransportHasBarStartPpqPositionForUi_.store(snapshot.hasBarStartPpqPosition, std::memory_order_relaxed);
        hostTransportBarStartPpqPositionForUi_.store(snapshot.ppqPositionOfLastBarStart, std::memory_order_relaxed);
        hostTransportHasTimeSignatureForUi_.store(snapshot.hasTimeSignature, std::memory_order_relaxed);
        hostTransportTimeSigNumeratorForUi_.store(snapshot.timeSigNumerator, std::memory_order_relaxed);
        hostTransportTimeSigDenominatorForUi_.store(snapshot.timeSigDenominator, std::memory_order_relaxed);
    };

    auto updateHostSyncAssistEligibility = [this, &hostTransport]() {
        const bool hasTiming = authoritativeTiming_.hasTiming && authoritativeTiming_.activeBpm > 0
            && authoritativeTiming_.activeBpi > 0;
        const auto targetBpm = hasTiming ? authoritativeTiming_.activeBpm : 0;
        const auto targetBpi = hasTiming ? authoritativeTiming_.activeBpi : 0;

        hostSyncAssistTargetBpmForUi_.store(targetBpm, std::memory_order_relaxed);
        hostSyncAssistTargetBpiForUi_.store(targetBpi, std::memory_order_relaxed);
        hostSyncAssistBlockReason_.store(computeHostSyncAssistBlockReason(hasTiming, targetBpm, hostTransport),
                                         std::memory_order_relaxed);
    };

    auto finishProcessBlock = [this, &hostTransport]() {
        updateRemoteMixerStripActivity();
        previousHostTransportPlaying_ = hostTransport.playing;
    };

    storeHostTransportSnapshot(hostTransport);

    if (! isSessionConnected() || currentSampleRate_ <= 0.0)
    {
        applyLocalMonitorToOutput();
        applyMasterOutputToBuffer();
        clearTransportUi();
        updateHostSyncAssistEligibility();
        finishProcessBlock();
        return;
    }

    if (experimentalStreamingEnabled_ && ! framedTransport_.isRunning())
    {
        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::ConnectionLostRetryable,
            .reason = "transport disconnected",
        });
        applyLocalMonitorToOutput();
        applyMasterOutputToBuffer();
        clearTransportUi();
        updateHostSyncAssistEligibility();
        finishProcessBlock();
        return;
    }

    if (experimentalStreamingEnabled_)
    {
        net::FramedSocketTransport::ServerTimingConfig timingConfig;
        if (framedTransport_.getServerTimingConfig(timingConfig))
        {
            const auto serverIntervalSamples = computeIntervalSamplesFromServerTiming(currentSampleRate_,
                                                                                      timingConfig.beatsPerMinute,
                                                                                      timingConfig.beatsPerInterval);
            const auto serverBeatSamples = computeBeatSamplesFromServerTiming(currentSampleRate_, timingConfig.beatsPerMinute);

            if (serverIntervalSamples > 0 && serverBeatSamples > 0)
            {
                const bool matchesActive = authoritativeTiming_.hasTiming
                    && authoritativeTiming_.activeBpm == timingConfig.beatsPerMinute
                    && authoritativeTiming_.activeBpi == timingConfig.beatsPerInterval
                    && authoritativeTiming_.activeIntervalSamples == serverIntervalSamples
                    && authoritativeTiming_.activeBeatSamples == serverBeatSamples;
                const bool matchesPending = authoritativeTiming_.hasPendingTiming
                    && authoritativeTiming_.pendingBpm == timingConfig.beatsPerMinute
                    && authoritativeTiming_.pendingBpi == timingConfig.beatsPerInterval
                    && authoritativeTiming_.pendingIntervalSamples == serverIntervalSamples
                    && authoritativeTiming_.pendingBeatSamples == serverBeatSamples;

                if (! authoritativeTiming_.hasTiming)
                {
                    activateTimingConfig(authoritativeTiming_,
                                         timingConfig.beatsPerMinute,
                                         timingConfig.beatsPerInterval,
                                         serverIntervalSamples,
                                         serverBeatSamples);
                    authoritativeTiming_.samplesIntoInterval = 0;
                    transmitWarmupIntervalsRemaining_ = experimentalStreamingEnabled_ ? kPreparedTransmitIntervals : 0;
                }
                else if (! matchesActive && ! matchesPending)
                {
                    stageTimingConfig(authoritativeTiming_,
                                      timingConfig.beatsPerMinute,
                                      timingConfig.beatsPerInterval,
                                      serverIntervalSamples,
                                      serverBeatSamples);
                }
            }
        }

    }

    if (experimentalStreamingEnabled_)
    {
        if (isLocalVoiceMode(localChannelMode_))
        {
            if (! transmitEnabled_)
            {
                localUploadIntervalBuffer_.setSize(0, 0);
                localVoiceUploadBuffer_.setSize(0, 0);
                localUploadIntervalWritePosition_ = 0;
                localVoiceUploadWritePosition_ = 0;
            }
            else
            {
                if (localVoiceUploadBuffer_.getNumChannels() != 1
                    || localVoiceUploadBuffer_.getNumSamples() != kLocalVoiceChunkSamples)
                {
                    localVoiceUploadBuffer_.setSize(1, kLocalVoiceChunkSamples, false, true, true);
                    localVoiceUploadBuffer_.clear();
                    localVoiceUploadWritePosition_ = 0;
                }

                int sourceOffset = 0;
                while (sourceOffset < mainInput.getNumSamples())
                {
                    const auto writableSamples = juce::jmin(mainInput.getNumSamples() - sourceOffset,
                                                            kLocalVoiceChunkSamples - localVoiceUploadWritePosition_);

                    if (mainInput.getNumChannels() == 1)
                    {
                        localVoiceUploadBuffer_.copyFrom(0,
                                                         localVoiceUploadWritePosition_,
                                                         mainInput,
                                                         0,
                                                         sourceOffset,
                                                         writableSamples);
                    }
                    else
                    {
                        for (int sample = 0; sample < writableSamples; ++sample)
                        {
                            float summed = 0.0f;
                            for (int channel = 0; channel < mainInput.getNumChannels(); ++channel)
                                summed += mainInput.getSample(channel, sourceOffset + sample);

                            localVoiceUploadBuffer_.setSample(
                                0,
                                localVoiceUploadWritePosition_ + sample,
                                summed / static_cast<float>(juce::jmax(1, mainInput.getNumChannels())));
                        }
                    }

                    localVoiceUploadWritePosition_ += writableSamples;
                    sourceOffset += writableSamples;
                    localVoiceTransitionPending_ = false;

                    if (localVoiceUploadWritePosition_ >= kLocalVoiceChunkSamples)
                    {
                        codecStreamBridge_.submitInput(localVoiceUploadBuffer_, currentSampleRate_);
                        localVoiceUploadBuffer_.clear();
                        localVoiceUploadWritePosition_ = 0;
                    }
                }

                localUploadIntervalWritePosition_ = localVoiceUploadWritePosition_;
            }
        }
        else if (authoritativeTiming_.hasTiming && authoritativeTiming_.activeIntervalSamples > 0)
        {
            localVoiceUploadBuffer_.setSize(0, 0);
            localVoiceUploadWritePosition_ = 0;
            if (! transmitEnabled_ || transmitWarmupIntervalsRemaining_ > 0)
            {
                localUploadIntervalBuffer_.setSize(0, 0);
                localUploadIntervalWritePosition_ = 0;
            }
            else
            {
                if (localUploadIntervalBuffer_.getNumChannels() != mainInput.getNumChannels()
                    || localUploadIntervalBuffer_.getNumSamples() != authoritativeTiming_.activeIntervalSamples)
                {
                    localUploadIntervalBuffer_.setSize(mainInput.getNumChannels(),
                                                       authoritativeTiming_.activeIntervalSamples,
                                                       false,
                                                       true,
                                                       true);
                    localUploadIntervalBuffer_.clear();
                }

                auto intervalWritePosition = juce::jlimit(0,
                                                          authoritativeTiming_.activeIntervalSamples - 1,
                                                          authoritativeTiming_.samplesIntoInterval);
                int sourceOffset = 0;
                while (sourceOffset < mainInput.getNumSamples())
                {
                    const auto writableSamples = juce::jmin(mainInput.getNumSamples() - sourceOffset,
                                                            authoritativeTiming_.activeIntervalSamples
                                                                - intervalWritePosition);

                    for (int channel = 0; channel < mainInput.getNumChannels(); ++channel)
                    {
                        localUploadIntervalBuffer_.copyFrom(channel,
                                                            intervalWritePosition,
                                                            mainInput,
                                                            channel,
                                                            sourceOffset,
                                                            writableSamples);
                    }

                    intervalWritePosition += writableSamples;
                    sourceOffset += writableSamples;

                    if (intervalWritePosition >= authoritativeTiming_.activeIntervalSamples)
                    {
                        {
                            const std::scoped_lock lock(stemCaptureMutex_);
                            if (stemCaptureState_.activeRecording
                                && ! stemCaptureState_.suppressLocalIntervalStemUntilBoundary)
                            {
                                captureLocalIntervalStem(localUploadIntervalBuffer_);
                            }
                        }
                        codecStreamBridge_.submitInput(localUploadIntervalBuffer_, currentSampleRate_);
                        localUploadIntervalBuffer_.clear();
                        intervalWritePosition = 0;
                    }
                }

                localUploadIntervalWritePosition_ = intervalWritePosition;
            }
        }
    }
    else
    {
        codecStreamBridge_.submitInput(mainInput, currentSampleRate_);
    }

    applyLocalMonitorToOutput();

    juce::MemoryBlock encoded;
    if (codecStreamBridge_.popEncoded(encoded))
    {
        lastCodecPayloadBytes_.store(encoded.getSize(), std::memory_order_relaxed);

        if (experimentalStreamingEnabled_ && transmitEnabled_)
        {
            framedTransport_.enqueueOutbound(encoded);
            if (isLocalVoiceMode(localChannelMode_))
                localVoiceTransitionPending_ = false;
        }
    }

    if (experimentalStreamingEnabled_)
    {
        net::FramedSocketTransport::RemoteSourceActivityUpdate sourceActivityUpdate;
        int sourceUpdatesConsumed = 0;

        while (sourceUpdatesConsumed < 16 && framedTransport_.popRemoteSourceActivityUpdate(sourceActivityUpdate))
        {
            if (sourceActivityUpdate.active)
                syncRemoteMixerStrip(sourceActivityUpdate.sourceInfo, false);
            else
                markRemoteMixerStripInactive(sourceActivityUpdate.sourceInfo.sourceId);

            ++sourceUpdatesConsumed;
        }

        net::FramedSocketTransport::InboundFrame inboundFrame;
        int framesConsumed = 0;

        while (framesConsumed < 8 && framedTransport_.popInboundFrame(inboundFrame))
        {
            syncRemoteMixerStrip(inboundFrame.sourceInfo, true);
            const auto sourceId = inboundFrame.sourceId.empty() ? std::string("unknown") : inboundFrame.sourceId;
            auto& diagnostics = remoteReceiveDiagnosticsBySource_[sourceId];
            diagnostics.lastEncodedBytes = inboundFrame.payload.getSize();
            diagnostics.lastInboundIntervalIndex = authoritativeTiming_.intervalIndex;
            codecStreamBridge_.submitInboundEncoded(inboundFrame.sourceId,
                                                    inboundFrame.payload.getData(),
                                                    inboundFrame.payload.getSize());
            ++framesConsumed;
        }
    }

    audio::CodecStreamBridge::DecodedFrame decodedFrame;
    int decodedFrames = 0;

    while (decodedFrames < 8 && codecStreamBridge_.popDecodedFrame(decodedFrame))
    {
        const auto decodedAtHostRate = audio::resampleAudioBuffer(decodedFrame.audio,
                                                                  decodedFrame.sampleRate,
                                                                  currentSampleRate_);

        lastDecodedSamples_.store(decodedAtHostRate.getNumSamples(), std::memory_order_relaxed);

        const auto sourceId = decodedFrame.sourceId.empty() ? std::string("unknown") : decodedFrame.sourceId;
        auto& diagnostics = remoteReceiveDiagnosticsBySource_[sourceId];
        diagnostics.lastDecodedSamples = decodedAtHostRate.getNumSamples();
        diagnostics.lastDecodedSampleRate = decodedFrame.sampleRate;
        const auto knownSourceIt = knownRemoteSourcesById_.find(sourceId);
        const auto sourceIsVoice = knownSourceIt != knownRemoteSourcesById_.end()
            && isVoiceMode(knownSourceIt->second.channelFlags);

        if (sourceIsVoice)
        {
            diagnostics.lastCopiedSamples = decodedAtHostRate.getNumSamples();
            diagnostics.lastQueuedBoundary = authoritativeTiming_.intervalIndex;
            {
                const std::scoped_lock lock(stemCaptureMutex_);
                retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, false, currentSampleRate_));
                retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, true, currentSampleRate_));
            }
            queueDecodedRemoteVoiceChunk(remoteVoiceChunksBySource_, sourceId, std::move(decodedAtHostRate));
        }
        else
        {
            {
                const std::scoped_lock lock(stemCaptureMutex_);
                if (stemCaptureState_.activeRecording)
                    captureRemoteStem(sourceId, decodedAtHostRate, false);
            }
            queueDecodedRemoteInterval(remoteQueuedIntervalsBySource_,
                                       nextRemoteTargetBoundaryBySource_,
                                       remoteReceiveDiagnosticsBySource_,
                                       authoritativeTiming_,
                                       sourceId,
                                       decodedAtHostRate);
        }

        ++cpuDiagnosticSnapshot_.remoteFramesDecoded;

        ++decodedFrames;
    }

    updateHostSyncAssistEligibility();

    const auto hostPlayTransition = ! previousHostTransportPlaying_ && hostTransport.playing;
    const bool assistArmed = hostSyncAssistArmed_.load(std::memory_order_relaxed);
    const bool assistFailed = hostSyncAssistFailed_.load(std::memory_order_relaxed);

    if (assistArmed && hostPlayTransition)
    {
        const auto startOffsetSamples = computeHostSyncAssistStartOffsetSamples(authoritativeTiming_, hostTransport);
        if (startOffsetSamples < 0)
        {
            hostSyncAssistArmed_.store(false, std::memory_order_relaxed);
            hostSyncAssistWaitingForHost_.store(false, std::memory_order_relaxed);
            hostSyncAssistFailed_.store(true, std::memory_order_relaxed);
            hostSyncAssistFailureReason_.store(HostSyncAssistFailureReason::MissingHostMusicalPosition,
                                               std::memory_order_relaxed);
            applyMasterOutputToBuffer();
            finishProcessBlock();
            return;
        }

        authoritativeTiming_.samplesIntoInterval = startOffsetSamples;
        hostSyncAssistArmed_.store(false, std::memory_order_relaxed);
        hostSyncAssistWaitingForHost_.store(false, std::memory_order_relaxed);
        hostSyncAssistFailed_.store(false, std::memory_order_relaxed);
        hostSyncAssistFailureReason_.store(HostSyncAssistFailureReason::None, std::memory_order_relaxed);
    }
    else if (assistArmed || assistFailed)
    {
        hostSyncAssistWaitingForHost_.store(assistArmed, std::memory_order_relaxed);

        if (authoritativeTiming_.hasTiming)
        {
            hasServerTimingForUi_.store(true, std::memory_order_relaxed);
            serverBpmForUi_.store(authoritativeTiming_.activeBpm, std::memory_order_relaxed);
            beatsPerIntervalForUi_.store(authoritativeTiming_.activeBpi, std::memory_order_relaxed);
            intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);
        }

        applyMasterOutputToBuffer();
        finishProcessBlock();
        return;
    }

    if (authoritativeTiming_.hasTiming && authoritativeTiming_.activeIntervalSamples > 0)
    {
        const auto mainOutputChannels = mainOutput.getNumChannels();
        const auto outputSamples = buffer.getNumSamples();
        const auto beatsPerInterval = juce::jmax(1, authoritativeTiming_.activeBpi);
        const auto clickDurationSamples = juce::jmax(1,
                                                     static_cast<int>((kMetronomeClickDurationMs / 1000.0f)
                                                                      * static_cast<float>(currentSampleRate_)));
        const bool renderMetronome = metronomeEnabled_.load(std::memory_order_relaxed)
            && authoritativeTiming_.activeBeatSamples > 0;
        const bool routedOutputAvailable = auxOutput.getNumChannels() > 0;

        hasServerTimingForUi_.store(true, std::memory_order_relaxed);
        serverBpmForUi_.store(authoritativeTiming_.activeBpm, std::memory_order_relaxed);
        beatsPerIntervalForUi_.store(authoritativeTiming_.activeBpi, std::memory_order_relaxed);
        const auto soloActive = anySoloActive(mixerStripsBySourceId_);

        for (int sample = 0; sample < outputSamples; ++sample)
        {
            for (auto voiceIt = remoteVoiceChunksBySource_.begin(); voiceIt != remoteVoiceChunksBySource_.end();)
            {
                auto& queuedChunks = voiceIt->second;
                while (! queuedChunks.empty()
                       && queuedChunks.front().playbackPosition >= queuedChunks.front().audio.getNumSamples())
                {
                    queuedChunks.pop_front();
                }

                if (queuedChunks.empty())
                {
                    voiceIt = remoteVoiceChunksBySource_.erase(voiceIt);
                    continue;
                }

                const auto stripIt = mixerStripsBySourceId_.find(voiceIt->first);
                const auto& voiceChunk = queuedChunks.front();
                const auto sourceChannels = voiceChunk.audio.getNumChannels();
                const bool stripAudible = stripIt != mixerStripsBySourceId_.end()
                    && isStripAudible(stripIt->second.snapshot.mix, soloActive);

                if (stripAudible && sourceChannels > 0
                    && voiceChunk.playbackPosition < voiceChunk.audio.getNumSamples())
                {
                    auto* targetOutput = &mainOutput;
                    if (routedOutputAvailable
                        && hostRoutingProofRoute_.outputBusIndex == HostRoutingProof::kRoutedOutputBusIndex
                        && hostRoutingProofRoute_.sourceId == voiceIt->first)
                    {
                        targetOutput = &auxOutput;
                    }

                    mixSourceSampleToOutput(voiceChunk.audio,
                                            voiceChunk.playbackPosition,
                                            *targetOutput,
                                            sample,
                                            stripIt->second.snapshot.mix,
                                            stripIt->second.snapshot.meter,
                                            cpuDiagnosticSnapshot_);
                }

                ++queuedChunks.front().playbackPosition;
                if (queuedChunks.front().playbackPosition >= queuedChunks.front().audio.getNumSamples())
                    queuedChunks.pop_front();

                if (queuedChunks.empty())
                    voiceIt = remoteVoiceChunksBySource_.erase(voiceIt);
                else
                    ++voiceIt;
            }

            for (auto& sourceEntry : remoteActiveIntervalBySource_)
            {
                const auto stripIt = mixerStripsBySourceId_.find(sourceEntry.first);
                if (stripIt == mixerStripsBySourceId_.end()
                    || ! isStripAudible(stripIt->second.snapshot.mix, soloActive)
                    || stripIt->second.snapshot.unsupportedVoiceMode)
                    continue;

                const auto& sourceBuffer = sourceEntry.second;
                if (authoritativeTiming_.samplesIntoInterval >= sourceBuffer.getNumSamples())
                    continue;

                if (sourceBuffer.getNumChannels() <= 0)
                    continue;

                auto* targetOutput = &mainOutput;
                if (routedOutputAvailable
                    && hostRoutingProofRoute_.outputBusIndex == HostRoutingProof::kRoutedOutputBusIndex
                    && hostRoutingProofRoute_.sourceId == sourceEntry.first)
                {
                    targetOutput = &auxOutput;
                }

                mixSourceSampleToOutput(sourceBuffer,
                                        authoritativeTiming_.samplesIntoInterval,
                                        *targetOutput,
                                        sample,
                                        stripIt->second.snapshot.mix,
                                        stripIt->second.snapshot.meter,
                                        cpuDiagnosticSnapshot_);
            }

            if (authoritativeTiming_.activeBeatSamples > 0
                && (authoritativeTiming_.samplesIntoInterval % authoritativeTiming_.activeBeatSamples) == 0)
            {
                const auto beatZeroBased = juce::jlimit(0,
                                                        beatsPerInterval - 1,
                                                        authoritativeTiming_.samplesIntoInterval / authoritativeTiming_.activeBeatSamples);
                currentBeatForUi_.store(beatZeroBased + 1, std::memory_order_relaxed);

                if (renderMetronome)
                {
                    const bool isDownbeat = beatZeroBased == 0;
                    metronomeClickRemainingSamples_ = clickDurationSamples;
                    metronomeClickPhase_ = 0.0f;
                    const auto frequency = isDownbeat ? kMetronomeDownbeatFrequencyHz : kMetronomeBeatFrequencyHz;
                    metronomeClickPhaseIncrement_ = juce::MathConstants<float>::twoPi * frequency
                                                  / static_cast<float>(currentSampleRate_);
                    metronomeClickGain_ = isDownbeat ? kMetronomeDownbeatGain : kMetronomeBeatGain;
                }
            }

            if (renderMetronome && metronomeClickRemainingSamples_ > 0)
            {
                const auto envelope = static_cast<float>(metronomeClickRemainingSamples_)
                                    / static_cast<float>(clickDurationSamples);
                const auto metronomeGain
                    = juce::Decibels::decibelsToGain(metronomeGainDb_.load(std::memory_order_relaxed));
                const auto clickSample = std::sin(metronomeClickPhase_) * metronomeClickGain_ * envelope * metronomeGain;

                for (int channel = 0; channel < mainOutputChannels; ++channel)
                    mainOutput.addSample(channel, sample, clickSample);

                metronomeClickPhase_ += metronomeClickPhaseIncrement_;
                --metronomeClickRemainingSamples_;
            }

            ++authoritativeTiming_.samplesIntoInterval;
            if (authoritativeTiming_.samplesIntoInterval >= authoritativeTiming_.activeIntervalSamples)
            {
                authoritativeTiming_.samplesIntoInterval = 0;
                ++authoritativeTiming_.intervalIndex;
                intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);
                handleStemCaptureBoundary();

                if (transmitWarmupIntervalsRemaining_ > 0)
                {
                    --transmitWarmupIntervalsRemaining_;
                    localUploadIntervalBuffer_.setSize(0, 0);
                    localVoiceUploadBuffer_.setSize(0, 0);
                    localUploadIntervalWritePosition_ = 0;
                    localVoiceUploadWritePosition_ = 0;
                }

                if (authoritativeTiming_.hasPendingTiming)
                {
                    activateTimingConfig(authoritativeTiming_,
                                         authoritativeTiming_.pendingBpm,
                                         authoritativeTiming_.pendingBpi,
                                         authoritativeTiming_.pendingIntervalSamples,
                                         authoritativeTiming_.pendingBeatSamples);
                    localUploadIntervalBuffer_.setSize(0, 0);
                    localVoiceUploadBuffer_.setSize(0, 0);
                    localUploadIntervalWritePosition_ = 0;
                    localVoiceUploadWritePosition_ = 0;
                    remoteQueuedIntervalsBySource_.clear();
                    remoteVoiceChunksBySource_.clear();
                    nextRemoteTargetBoundaryBySource_.clear();
                    remoteActiveIntervalBySource_.clear();
                    lastRemoteActivationBoundaryBySource_.clear();
                    remoteReceiveDiagnosticsBySource_.clear();
                    knownRemoteSourcesById_.clear();
                    
                }

                activateRemoteIntervalsForBoundary(remoteQueuedIntervalsBySource_,
                                                  remoteActiveIntervalBySource_,
                                                  lastRemoteActivationBoundaryBySource_,
                                                  authoritativeTiming_.intervalIndex);
            }
        }

        intervalProgressForUi_.store(static_cast<float>(authoritativeTiming_.samplesIntoInterval)
                                     / static_cast<float>(authoritativeTiming_.activeIntervalSamples),
                                     std::memory_order_relaxed);

        if (authoritativeTiming_.activeBeatSamples > 0)
        {
            const auto beatZeroBased = juce::jlimit(0,
                                                    beatsPerInterval - 1,
                                                    authoritativeTiming_.samplesIntoInterval / authoritativeTiming_.activeBeatSamples);
            currentBeatForUi_.store(beatZeroBased + 1, std::memory_order_relaxed);
        }

        intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);
    }
    else
    {
        clearTransportUi();
    }

    applyMasterOutputToBuffer();

    finishProcessBlock();
}

bool FamaLamaJamAudioProcessor::hasEditor() const { return true; }


juce::AudioProcessorEditor* FamaLamaJamAudioProcessor::createEditor()
{
    auto settingsGetter = [this]() { return settingsStore_.getActiveSettings(); };
    auto apply = [this](app::session::SessionSettings draft) { return applySettingsDraft(std::move(draft)); };
    auto lifecycleGetter = [this]() { return getLifecycleSnapshot(); };
    auto connect = [this]() { return requestConnect(); };
    auto disconnect = [this]() { return requestDisconnect(); };
    auto transportUiGetter = [this]() {
        const auto state = getTransportUiState();
        return FamaLamaJamAudioProcessorEditor::TransportUiState {
            .connected = state.connected,
            .hasServerTiming = state.hasServerTiming,
            .syncHealth = static_cast<FamaLamaJamAudioProcessorEditor::SyncHealth>(state.syncHealth),
            .metronomeAvailable = state.metronomeAvailable,
            .beatsPerMinute = state.beatsPerMinute,
            .beatsPerInterval = state.beatsPerInterval,
            .currentBeat = state.currentBeat,
            .intervalProgress = state.intervalProgress,
            .intervalIndex = state.intervalIndex,
        };
    };
    auto hostSyncAssistUiGetter = [this]() {
        const auto state = getHostSyncAssistUiState();
        return FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState {
            .armable = state.armable,
            .armed = state.armed,
            .waitingForHost = state.waitingForHost,
            .blocked = state.blocked,
            .failed = state.failed,
            .blockReason = static_cast<FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason>(state.blockReason),
            .failureReason
            = static_cast<FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason>(state.failureReason),
            .targetBeatsPerMinute = state.targetBeatsPerMinute,
            .targetBeatsPerInterval = state.targetBeatsPerInterval,
            .hostPlaying = state.hostTransport.playing,
        };
    };
    auto hostSyncAssistToggle = [this]() { return toggleHostSyncAssistArm(); };
    auto roomUiGetter = [this]() {
        const auto state = getRoomUiState();

        FamaLamaJamAudioProcessorEditor::RoomUiState roomState {
            .connected = state.connected,
            .topic = state.topic,
            .bpmVote = FamaLamaJamAudioProcessorEditor::RoomVoteUiState {
                .pending = state.bpmVote.pending,
                .failed = state.bpmVote.failed,
                .requestedValue = state.bpmVote.requestedValue,
                .statusText = state.bpmVote.statusText,
            },
            .bpiVote = FamaLamaJamAudioProcessorEditor::RoomVoteUiState {
                .pending = state.bpiVote.pending,
                .failed = state.bpiVote.failed,
                .requestedValue = state.bpiVote.requestedValue,
                .statusText = state.bpiVote.statusText,
            },
        };

        roomState.visibleFeed.reserve(state.visibleFeed.size());
        for (const auto& entry : state.visibleFeed)
        {
            auto kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat;

            switch (entry.kind)
            {
                case RoomFeedEntryKind::Chat:
                    kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat;
                    break;
                case RoomFeedEntryKind::Topic:
                    kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic;
                    break;
                case RoomFeedEntryKind::Presence:
                    kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Presence;
                    break;
                case RoomFeedEntryKind::VoteSystem:
                    kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::VoteSystem;
                    break;
                case RoomFeedEntryKind::GenericSystem:
                    kind = FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::GenericSystem;
                    break;
            }

            roomState.visibleFeed.push_back(FamaLamaJamAudioProcessorEditor::RoomFeedEntry {
                .kind = kind,
                .author = entry.author,
                .text = entry.text,
                .subdued = entry.subdued,
            });
        }

        return roomState;
    };
    auto roomMessageHandler = [this](std::string text) { return sendRoomChatMessage(std::move(text)); };
    auto roomVoteHandler = [this](FamaLamaJamAudioProcessorEditor::RoomVoteKind kind, int value) {
        return submitRoomVote(kind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm ? RoomVoteKind::Bpm
                                                                                          : RoomVoteKind::Bpi,
                              value);
    };
    auto serverDiscoveryUiGetter = [this]() {
        const auto state = getServerDiscoveryUiState();
        FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState editorState {
            .fetchInProgress = state.fetchInProgress,
            .hasStalePublicData = state.hasStalePublicData,
            .statusText = state.statusText,
        };

        editorState.combinedEntries.reserve(state.combinedEntries.size());
        for (const auto& entry : state.combinedEntries)
        {
            editorState.combinedEntries.push_back(FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry {
                .source = entry.source == ServerDiscoveryEntry::Source::Remembered
                    ? FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Remembered
                    : FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry::Source::Public,
                .label = entry.label,
                .host = entry.host,
                .port = entry.port,
                .username = entry.username,
                .password = entry.password,
                .connectedUsers = entry.connectedUsers,
                .stale = entry.stale,
            });
        }

        return editorState;
    };
    auto serverDiscoveryRefreshHandler = [this](bool manual) { return requestPublicServerDiscoveryRefresh(manual); };
    auto mixerStripsGetter = [this]() {
        std::vector<FamaLamaJamAudioProcessorEditor::MixerStripState> strips;
        for (const auto& snapshot : getMixerStripSnapshots())
        {
            strips.push_back(FamaLamaJamAudioProcessorEditor::MixerStripState {
                .kind = snapshot.descriptor.kind == MixerStripKind::LocalMonitor
                    ? FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor
                    : FamaLamaJamAudioProcessorEditor::MixerStripKind::RemoteDelayed,
                .sourceId = snapshot.descriptor.sourceId,
                .groupId = snapshot.descriptor.groupId,
                .groupLabel = snapshot.descriptor.kind == MixerStripKind::LocalMonitor
                    ? std::string("Local Monitor")
                    : snapshot.descriptor.userName,
                .displayName = snapshot.descriptor.displayName,
                .subtitle = snapshot.descriptor.kind == MixerStripKind::LocalMonitor
                    ? std::string("Live monitor")
                    : (snapshot.descriptor.channelName.empty() ? std::string("Delayed return")
                                                               : snapshot.descriptor.channelName),
                .gainDb = snapshot.mix.gainDb,
                .pan = snapshot.mix.pan,
                .muted = snapshot.mix.muted,
                .soloed = snapshot.mix.soloed,
                .meterLeft = snapshot.meter.left,
                .meterRight = snapshot.meter.right,
                .transmitState = static_cast<FamaLamaJamAudioProcessorEditor::TransmitState>(snapshot.transmitState),
                .localChannelMode = isLocalVoiceMode(localChannelMode_)
                    ? FamaLamaJamAudioProcessorEditor::LocalChannelMode::Voice
                    : FamaLamaJamAudioProcessorEditor::LocalChannelMode::Interval,
                .voiceMode = snapshot.voiceMode,
                .unsupportedVoiceMode = snapshot.unsupportedVoiceMode,
                .statusText = snapshot.statusText,
                .active = snapshot.descriptor.active,
                .visible = snapshot.descriptor.visible,
            });
        }
        return strips;
    };
    auto mixerStripSetter = [this](const std::string& sourceId, float gainDb, float pan, bool muted) {
        return setMixerStripMixState(sourceId, gainDb, pan, muted);
    };
    auto mixerStripSoloSetter = [this](const std::string& sourceId, bool soloed) {
        return setMixerStripSoloState(sourceId, soloed);
    };
    auto metronomeGetter = [this]() { return isMetronomeEnabled(); };
    auto metronomeSetter = [this](bool enabled) { setMetronomeEnabled(enabled); };
    auto metronomeGainGetter = [this]() { return getMetronomeGainDb(); };
    auto metronomeGainSetter = [this](float gainDb) { setMetronomeGainDb(gainDb); };
    auto stemCaptureSettingsGetter = [this]() {
        const auto settings = getStemCaptureSettings();
        std::string statusText;
        bool canRequestNewRun = false;
        {
            const std::scoped_lock lock(stemCaptureMutex_);
            canRequestNewRun = settings.enabled && ! settings.outputDirectory.empty();
            if (stemCaptureState_.pendingNewRunAtBoundary)
                statusText = "New stem folder starts on the next interval.";
        }
        return FamaLamaJamAudioProcessorEditor::StemCaptureUiState {
            .enabled = settings.enabled,
            .outputDirectory = settings.outputDirectory,
            .statusText = std::move(statusText),
            .canRequestNewRun = canRequestNewRun,
        };
    };
    auto stemCaptureSettingsSetter = [this](app::session::StemCaptureSettings settings) {
        return applyStemCaptureSettings(std::move(settings));
    };
    auto stemCaptureNewRunHandler = [this]() { return requestNewStemCaptureRun(); };
    auto diagnosticsTextGetter = [this]() { return getRemoteReceiveDiagnosticsText(); };
    auto masterOutputGainGetter = [this]() { return getMasterOutputGainDb(); };
    auto masterOutputGainSetter = [this](float gainDb) { setMasterOutputGainDb(gainDb); };
    auto transmitToggleHandler = [this]() { return toggleTransmitEnabled(); };
    auto voiceModeToggleHandler = [this]() { return toggleLocalVoiceMode(); };

    return new FamaLamaJamAudioProcessorEditor(*this,
                                               std::move(settingsGetter),
                                               std::move(apply),
                                               std::move(lifecycleGetter),
                                               std::move(connect),
                                               std::move(disconnect),
                                               std::move(transportUiGetter),
                                               std::move(hostSyncAssistUiGetter),
                                               std::move(hostSyncAssistToggle),
                                               std::move(mixerStripsGetter),
                                               std::move(mixerStripSetter),
                                               std::move(metronomeGetter),
                                               std::move(metronomeSetter),
                                               std::move(serverDiscoveryUiGetter),
                                               std::move(serverDiscoveryRefreshHandler),
                                               std::move(roomUiGetter),
                                               std::move(roomMessageHandler),
                                               std::move(roomVoteHandler),
                                               std::move(diagnosticsTextGetter),
                                               std::move(masterOutputGainGetter),
                                               std::move(masterOutputGainSetter),
                                               std::move(metronomeGainGetter),
                                               std::move(metronomeGainSetter),
                                               std::move(stemCaptureSettingsGetter),
                                               std::move(stemCaptureSettingsSetter),
                                               std::move(stemCaptureNewRunHandler),
                                               std::move(transmitToggleHandler),
                                               std::move(voiceModeToggleHandler),
                                               std::move(mixerStripSoloSetter));
}

void FamaLamaJamAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryBlock settingsPayload;
    infra::state::SessionSettingsSerializer::serialize(settingsStore_.getActiveSettings(), settingsPayload);

    juce::ValueTree stateTree(kPluginStateType);
    stateTree.setProperty(kPluginStateSchemaVersion, kPluginStateSchema, nullptr);

    const auto settingsTree = parseValueTree(settingsPayload.getData(), static_cast<int>(settingsPayload.getSize()));
    if (settingsTree.isValid())
        stateTree.addChild(settingsTree.createCopy(), -1, nullptr);

    juce::ValueTree rememberedServerHistoryTree(kPluginStateRememberedServerHistory);
    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        for (const auto& entry : rememberedServers_)
        {
            juce::ValueTree rememberedServerTree(kPluginStateRememberedServer);
            rememberedServerTree.setProperty(kPluginStateRememberedServerHost, juce::String(entry.host), nullptr);
            rememberedServerTree.setProperty(kPluginStateRememberedServerPort, static_cast<int>(entry.port), nullptr);
            rememberedServerTree.setProperty(kPluginStateRememberedServerUsername, juce::String(entry.username), nullptr);
            rememberedServerTree.setProperty(kPluginStateRememberedServerPassword, juce::String(entry.password), nullptr);
            rememberedServerHistoryTree.addChild(rememberedServerTree, -1, nullptr);
        }
    }
    stateTree.addChild(rememberedServerHistoryTree, -1, nullptr);

    const auto lastErrorContext = shortenLastErrorContext(lifecycleController_.getSnapshot().lastError);
    if (! lastErrorContext.empty())
        stateTree.setProperty(kPluginStateLastErrorContext, juce::String(lastErrorContext), nullptr);

    stateTree.setProperty(kPluginStateMetronomeEnabled, metronomeEnabled_.load(std::memory_order_relaxed), nullptr);
    stateTree.setProperty(kPluginStateMetronomeGainDb, metronomeGainDb_.load(std::memory_order_relaxed), nullptr);
    stateTree.setProperty(kPluginStateMasterOutputGainDb,
                          masterOutputGainDb_.load(std::memory_order_relaxed),
                          nullptr);
    {
        const std::scoped_lock lock(stemCaptureMutex_);
        stateTree.setProperty(kPluginStateStemCaptureEnabled, stemCaptureState_.settings.enabled, nullptr);
        stateTree.setProperty(kPluginStateStemCaptureOutputDirectory,
                              juce::String(stemCaptureState_.settings.outputDirectory),
                              nullptr);
    }

    juce::ValueTree mixerStateTree(kPluginStateMixerState);
    for (const auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        juce::ValueTree stripTree(kPluginStateMixerStrip);
        stripTree.setProperty(kPluginStateMixerStripKind,
                              runtimeState.snapshot.descriptor.kind == MixerStripKind::LocalMonitor ? "local" : "remote",
                              nullptr);
        stripTree.setProperty(kPluginStateMixerStripSourceId, juce::String(runtimeState.snapshot.descriptor.sourceId), nullptr);
        stripTree.setProperty(kPluginStateMixerStripGroupId, juce::String(runtimeState.snapshot.descriptor.groupId), nullptr);
        stripTree.setProperty(kPluginStateMixerStripUserName, juce::String(runtimeState.snapshot.descriptor.userName), nullptr);
        stripTree.setProperty(kPluginStateMixerStripChannelName, juce::String(runtimeState.snapshot.descriptor.channelName), nullptr);
        stripTree.setProperty(kPluginStateMixerStripDisplayName, juce::String(runtimeState.snapshot.descriptor.displayName), nullptr);
        stripTree.setProperty(kPluginStateMixerStripChannelIndex, runtimeState.snapshot.descriptor.channelIndex, nullptr);
        stripTree.setProperty(kPluginStateMixerStripGainDb, runtimeState.snapshot.mix.gainDb, nullptr);
        stripTree.setProperty(kPluginStateMixerStripPan, runtimeState.snapshot.mix.pan, nullptr);
        stripTree.setProperty(kPluginStateMixerStripMuted, runtimeState.snapshot.mix.muted, nullptr);
        stripTree.setProperty(kPluginStateMixerStripSoloed, runtimeState.snapshot.mix.soloed, nullptr);
        mixerStateTree.addChild(stripTree, -1, nullptr);
    }

    stateTree.addChild(mixerStateTree, -1, nullptr);

    juce::MemoryOutputStream stream(destData, false);
    stateTree.writeToStream(stream);
}

void FamaLamaJamAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryBlock settingsPayload;
    std::string restoredLastErrorContext;
    bool restoredMetronomeEnabled = false;
    float restoredMetronomeGainDb = 0.0f;
    float restoredMasterOutputGainDb = 0.0f;
    app::session::StemCaptureSettings restoredStemCaptureSettings = app::session::makeDefaultStemCaptureSettings();
    std::vector<MixerStripSnapshot> restoredMixerStrips;
    std::vector<app::session::RememberedServerEntry> restoredRememberedServers;

    const auto stateTree = parseValueTree(data, sizeInBytes);
    const bool hasWrappedState = stateTree.isValid() && stateTree.hasType(kPluginStateType)
        && static_cast<int>(stateTree.getProperty(kPluginStateSchemaVersion, -1)) == kPluginStateSchema;

    if (hasWrappedState)
    {
        for (int childIndex = 0; childIndex < stateTree.getNumChildren(); ++childIndex)
        {
            const auto child = stateTree.getChild(childIndex);
            if (child.hasType(kSessionSettingsStateType))
            {
                juce::MemoryOutputStream settingsStream(settingsPayload, false);
                child.writeToStream(settingsStream);
                break;
            }
        }

        restoredLastErrorContext = shortenLastErrorContext(
            stateTree.getProperty(kPluginStateLastErrorContext, juce::String()).toString().toStdString());
        restoredMetronomeEnabled = static_cast<bool>(stateTree.getProperty(kPluginStateMetronomeEnabled, false));
        restoredMetronomeGainDb
            = normalizeMetronomeGainDb(static_cast<float>(stateTree.getProperty(kPluginStateMetronomeGainDb, 0.0f)));
        restoredMasterOutputGainDb
            = normalizeMasterOutputGainDb(static_cast<float>(stateTree.getProperty(kPluginStateMasterOutputGainDb, 0.0f)));
        restoredStemCaptureSettings.enabled
            = static_cast<bool>(stateTree.getProperty(kPluginStateStemCaptureEnabled, false));
        restoredStemCaptureSettings.outputDirectory
            = stateTree.getProperty(kPluginStateStemCaptureOutputDirectory, juce::String()).toString().toStdString();
        restoredStemCaptureSettings = app::session::normalizeStemCaptureSettings(restoredStemCaptureSettings);

        if (const auto rememberedServerHistoryTree = stateTree.getChildWithName(kPluginStateRememberedServerHistory);
            rememberedServerHistoryTree.isValid())
        {
            for (int childIndex = rememberedServerHistoryTree.getNumChildren(); --childIndex >= 0;)
            {
                const auto rememberedServerTree = rememberedServerHistoryTree.getChild(childIndex);
                if (! rememberedServerTree.hasType(kPluginStateRememberedServer))
                    continue;

                app::session::rememberSuccessfulServer(
                    restoredRememberedServers,
                    app::session::RememberedServerEntry {
                        .host = rememberedServerTree.getProperty(kPluginStateRememberedServerHost, juce::String())
                                    .toString()
                                    .toStdString(),
                        .port = static_cast<std::uint16_t>(juce::jlimit(
                            0,
                            65535,
                            static_cast<int>(rememberedServerTree.getProperty(kPluginStateRememberedServerPort, 0)))),
                        .username = rememberedServerTree.getProperty(kPluginStateRememberedServerUsername, juce::String())
                                        .toString()
                                        .toStdString(),
                        .password = rememberedServerTree.getProperty(kPluginStateRememberedServerPassword, juce::String())
                                        .toString()
                                        .toStdString(),
                    },
                    kMaxRememberedServers);
            }
        }

        if (const auto mixerStateTree = stateTree.getChildWithName(kPluginStateMixerState); mixerStateTree.isValid())
        {
            for (int childIndex = 0; childIndex < mixerStateTree.getNumChildren(); ++childIndex)
            {
                const auto stripTree = mixerStateTree.getChild(childIndex);
                if (! stripTree.hasType(kPluginStateMixerStrip))
                    continue;

                MixerStripSnapshot snapshot;
                snapshot.descriptor.kind
                    = stripTree.getProperty(kPluginStateMixerStripKind).toString() == "local"
                    ? MixerStripKind::LocalMonitor
                    : MixerStripKind::RemoteDelayed;
                snapshot.descriptor.sourceId
                    = stripTree.getProperty(kPluginStateMixerStripSourceId, juce::String()).toString().toStdString();
                snapshot.descriptor.groupId
                    = stripTree.getProperty(kPluginStateMixerStripGroupId, juce::String()).toString().toStdString();
                snapshot.descriptor.userName
                    = stripTree.getProperty(kPluginStateMixerStripUserName, juce::String()).toString().toStdString();
                snapshot.descriptor.channelName
                    = stripTree.getProperty(kPluginStateMixerStripChannelName, juce::String()).toString().toStdString();
                snapshot.descriptor.displayName
                    = stripTree.getProperty(kPluginStateMixerStripDisplayName, juce::String()).toString().toStdString();
                snapshot.descriptor.channelIndex
                    = static_cast<int>(stripTree.getProperty(kPluginStateMixerStripChannelIndex, -1));
                snapshot.mix.gainDb = static_cast<float>(stripTree.getProperty(kPluginStateMixerStripGainDb, 0.0f));
                snapshot.mix.pan = static_cast<float>(stripTree.getProperty(kPluginStateMixerStripPan, 0.0f));
                snapshot.mix.muted = static_cast<bool>(stripTree.getProperty(kPluginStateMixerStripMuted, false));
                snapshot.mix.soloed = static_cast<bool>(stripTree.getProperty(kPluginStateMixerStripSoloed, false));
                snapshot.mix = normalizeMixState(snapshot.mix);

                if (! snapshot.descriptor.sourceId.empty())
                    restoredMixerStrips.push_back(std::move(snapshot));
            }
        }
    }

    const void* settingsData = data;
    int settingsSize = sizeInBytes;

    if (settingsPayload.getSize() > 0)
    {
        settingsData = settingsPayload.getData();
        settingsSize = static_cast<int>(settingsPayload.getSize());
    }

    bool usedFallback = false;
    const auto restored = infra::state::SessionSettingsSerializer::deserializeOrDefault(settingsData,
                                                                                         settingsSize,
                                                                                         &usedFallback);

    app::session::SessionSettingsValidationResult validation;
    settingsStore_.applyCandidate(restored, &validation);

    metronomeEnabled_.store(restoredMetronomeEnabled, std::memory_order_relaxed);
    metronomeGainDb_.store(restoredMetronomeGainDb, std::memory_order_relaxed);
    masterOutputGainDb_.store(restoredMasterOutputGainDb, std::memory_order_relaxed);

    closeLiveSocket();
    applyLifecycleTransition(lifecycleController_.resetToIdle());
    mixerStripsBySourceId_.clear();

    std::vector<app::session::RememberedServerEntry> mergedRememberedServers;
    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        mergedRememberedServers = rememberedServers_;
    }

    for (auto it = restoredRememberedServers.rbegin(); it != restoredRememberedServers.rend(); ++it)
    {
        app::session::rememberSuccessfulServer(mergedRememberedServers, *it, kMaxRememberedServers);
    }

    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        rememberedServers_ = std::move(mergedRememberedServers);
    }
    ensureLocalMonitorMixerStrip();

    for (const auto& restoredStrip : restoredMixerStrips)
    {
        auto snapshot = restoredStrip;
        snapshot.meter = {};
        snapshot.descriptor.active = snapshot.descriptor.kind == MixerStripKind::LocalMonitor;
        snapshot.descriptor.visible = snapshot.descriptor.kind == MixerStripKind::LocalMonitor;
        snapshot.descriptor.inactiveIntervals = 0;

        auto& runtimeState = mixerStripsBySourceId_[snapshot.descriptor.sourceId];
        runtimeState.snapshot = std::move(snapshot);
        runtimeState.lastPresenceIntervalIndex = 0;
        runtimeState.lastAudioIntervalIndex = 0;
    }

    ensureLocalMonitorMixerStrip();
    updateRemoteMixerStripActivity();
    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        cachedPublicServers_.clear();
        publicServerListFetchInProgress_ = false;
        publicServerListStale_ = false;
        publicServerListStatusText_.clear();
    }
    persistRememberedServers();
    applyStemCaptureSettings(restoredStemCaptureSettings);
    lastStatusMessage_ = makeRestoreStatusMessage(usedFallback, restoredLastErrorContext);
}

app::session::SessionSettingsController::ApplyResult FamaLamaJamAudioProcessor::applySettingsDraft(
    app::session::SessionSettings candidate)
{
    const bool wasErrorState = lifecycleController_.getSnapshot().state == app::session::ConnectionState::Error;
    auto result = settingsController_.applyDraft(std::move(candidate));

    if (! result.applied)
    {
        lastStatusMessage_ = result.statusMessage;
        return result;
    }

    if (wasErrorState)
    {
        auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::ApplySettings);
        applyLifecycleTransition(transition);

        if (transition.changed)
            result.statusMessage = transition.snapshot.statusMessage;
        else
            lastStatusMessage_ = result.statusMessage;

        return result;
    }

    lastStatusMessage_ = result.statusMessage;
    return result;
}

bool FamaLamaJamAudioProcessor::applySettingsFromUi(const app::session::SessionSettings& candidate,
                                                     app::session::SessionSettingsValidationResult* validation)
{
    auto result = applySettingsDraft(candidate);

    if (validation != nullptr)
        *validation = result.validation;

    return result.applied;
}

bool FamaLamaJamAudioProcessor::requestConnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Connect);
    applyLifecycleTransition(transition);
    return transition.changed;
}

bool FamaLamaJamAudioProcessor::requestDisconnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Disconnect);
    applyLifecycleTransition(transition);
    return transition.changed;
}

app::session::StemCaptureSettings FamaLamaJamAudioProcessor::getStemCaptureSettings() const
{
    const std::scoped_lock lock(stemCaptureMutex_);
    return stemCaptureState_.settings;
}

bool FamaLamaJamAudioProcessor::applyStemCaptureSettings(app::session::StemCaptureSettings candidate)
{
    candidate = app::session::normalizeStemCaptureSettings(candidate);

    {
        std::scoped_lock lock(stemCaptureMutex_);
        stemCaptureState_.settings = candidate;
    }

    if (candidate.enabled && ! candidate.outputDirectory.empty())
    {
        armStemCaptureForNextBoundary();
        return true;
    }

    requestStemCaptureStop();
    return true;
}

bool FamaLamaJamAudioProcessor::requestNewStemCaptureRun()
{
    std::scoped_lock lock(stemCaptureMutex_);
    if (! stemCaptureState_.settings.enabled || stemCaptureState_.settings.outputDirectory.empty())
        return false;

    stemCaptureState_.pendingNewRunAtBoundary = true;
    return true;
}

bool FamaLamaJamAudioProcessor::toggleTransmitEnabled()
{
    transmitEnabled_ = ! transmitEnabled_;

    if (! transmitEnabled_)
    {
        transmitWarmupIntervalsRemaining_ = 0;
        localUploadIntervalBuffer_.setSize(0, 0);
        localVoiceUploadBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        localVoiceUploadWritePosition_ = 0;
    }
    else if (isSessionConnected())
    {
        transmitWarmupIntervalsRemaining_ = isLocalVoiceMode(localChannelMode_) ? 0
            : (authoritativeTiming_.hasTiming && experimentalStreamingEnabled_
            ? kPreparedTransmitIntervals
            : 0);
        localVoiceTransitionPending_ = isLocalVoiceMode(localChannelMode_);
    }

    ensureLocalMonitorMixerStrip();
    return transmitEnabled_;
}

bool FamaLamaJamAudioProcessor::toggleLocalVoiceMode()
{
    const auto wasVoiceMode = isLocalVoiceMode(localChannelMode_);
    localChannelMode_ = isLocalVoiceMode(localChannelMode_) ? LocalChannelMode::Interval : LocalChannelMode::Voice;

    if (isLocalVoiceMode(localChannelMode_))
    {
        {
            const std::scoped_lock lock(stemCaptureMutex_);
            stemCaptureState_.suppressLocalIntervalStemUntilBoundary = true;
            retireStemCaptureSource(makeLocalStemSourceConfig(false, currentSampleRate_));
            retireStemCaptureSource(makeLocalStemSourceConfig(true, currentSampleRate_));
        }
        transmitWarmupIntervalsRemaining_ = 0;
        localUploadIntervalBuffer_.setSize(0, 0);
        localVoiceUploadBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        localVoiceUploadWritePosition_ = 0;
        localVoiceTransitionPending_ = transmitEnabled_;
        framedTransport_.setLocalChannelInfo("Voice Chat", 0x2u);
    }
    else
    {
        {
            const std::scoped_lock lock(stemCaptureMutex_);
            stemCaptureState_.suppressLocalIntervalStemUntilBoundary = true;
            if (wasVoiceMode)
                retireStemCaptureSource(makeLocalStemSourceConfig(false, currentSampleRate_));
        }
        localVoiceTransitionPending_ = false;
        localUploadIntervalBuffer_.setSize(0, 0);
        localVoiceUploadBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        localVoiceUploadWritePosition_ = 0;
        transmitWarmupIntervalsRemaining_ = (transmitEnabled_ && authoritativeTiming_.hasTiming && experimentalStreamingEnabled_)
            ? kPreparedTransmitIntervals
            : 0;
        framedTransport_.setLocalChannelInfo({}, 0u);
    }

    ensureLocalMonitorMixerStrip();
    return isLocalVoiceMode(localChannelMode_);
}

void FamaLamaJamAudioProcessor::handleConnectionEvent(const app::session::ConnectionEvent& event)
{
    const auto transition = lifecycleController_.handleEvent(event);
    applyLifecycleTransition(transition);

    if (event.type == app::session::ConnectionEventType::Connected
        && transition.changed
        && transition.snapshot.state == app::session::ConnectionState::Active)
    {
        rememberSuccessfulServer();
    }
}

bool FamaLamaJamAudioProcessor::triggerScheduledReconnectForTesting()
{
    const auto transition = lifecycleController_.triggerScheduledReconnect();
    applyLifecycleTransition(transition);
    return transition.changed;
}

int FamaLamaJamAudioProcessor::getScheduledReconnectDelayMs() const noexcept
{
    return scheduledReconnectDelayMs_;
}

app::session::SessionSettings FamaLamaJamAudioProcessor::getActiveSettings() const
{
    return settingsStore_.getActiveSettings();
}

app::session::ConnectionLifecycleSnapshot FamaLamaJamAudioProcessor::getLifecycleSnapshot() const
{
    return lifecycleController_.getSnapshot();
}

std::string FamaLamaJamAudioProcessor::getLastStatusMessage() const
{
    return lastStatusMessage_;
}

bool FamaLamaJamAudioProcessor::isSessionConnected() const noexcept
{
    return lifecycleController_.getSnapshot().isConnected();
}

std::size_t FamaLamaJamAudioProcessor::getLastCodecPayloadBytesForTesting() const noexcept
{
    return lastCodecPayloadBytes_.load(std::memory_order_relaxed);
}

int FamaLamaJamAudioProcessor::getLastDecodedSamplesForTesting() const noexcept
{
    return lastDecodedSamples_.load(std::memory_order_relaxed);
}

bool FamaLamaJamAudioProcessor::isExperimentalStreamingEnabledForTesting() const noexcept
{
    return experimentalStreamingEnabled_;
}

FamaLamaJamAudioProcessor::LiveAuthAttemptSnapshot
FamaLamaJamAudioProcessor::getLastLiveAuthAttemptForTesting() const
{
    const std::scoped_lock lock(liveAuthAttemptMutex_);
    return lastLiveAuthAttempt_;
}

std::size_t FamaLamaJamAudioProcessor::getTransportSentFramesForTesting() const noexcept
{
    return framedTransport_.getSentFrameCount();
}

std::size_t FamaLamaJamAudioProcessor::getTransportReceivedFramesForTesting() const noexcept
{
    return framedTransport_.getReceivedFrameCount();
}

std::size_t FamaLamaJamAudioProcessor::getActiveRemoteSourceCountForTesting() const noexcept
{
    return remoteActiveIntervalBySource_.size();
}

bool FamaLamaJamAudioProcessor::isRemoteSourceActiveForTesting(const std::string& sourceId) const
{
    return remoteActiveIntervalBySource_.find(sourceId) != remoteActiveIntervalBySource_.end();
}

std::vector<std::string> FamaLamaJamAudioProcessor::getActiveRemoteSourceIdsForTesting() const
{
    std::vector<std::string> ids;
    ids.reserve(remoteActiveIntervalBySource_.size());

    for (const auto& entry : remoteActiveIntervalBySource_)
        ids.push_back(entry.first);

    return ids;
}

std::size_t FamaLamaJamAudioProcessor::getQueuedRemoteSourceCountForTesting() const noexcept
{
    return remoteQueuedIntervalsBySource_.size();
}

std::size_t FamaLamaJamAudioProcessor::getPendingRemoteSourceCountForTesting() const noexcept
{
    return 0;
}

std::uint64_t FamaLamaJamAudioProcessor::getLastRemoteActivationBoundaryForTesting(const std::string& sourceId) const noexcept
{
    if (const auto it = lastRemoteActivationBoundaryBySource_.find(sourceId); it != lastRemoteActivationBoundaryBySource_.end())
        return it->second;

    return 0;
}

float FamaLamaJamAudioProcessor::getActiveRemoteIntervalAverageForTesting(const std::string& sourceId) const noexcept
{
    const auto it = remoteActiveIntervalBySource_.find(sourceId);
    if (it == remoteActiveIntervalBySource_.end())
        return 0.0f;

    const auto& buffer = it->second;
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return 0.0f;

    const auto* samples = buffer.getReadPointer(0);
    double sum = 0.0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        sum += samples[sample];

    return static_cast<float>(sum / static_cast<double>(buffer.getNumSamples()));
}

std::string FamaLamaJamAudioProcessor::getRemoteReceiveDiagnosticsText() const
{
    std::ostringstream stream;
    stream << "Recv Debug";

    if (! authoritativeTiming_.hasTiming || authoritativeTiming_.activeIntervalSamples <= 0)
    {
        stream << "\nwaiting for timing";
        return stream.str();
    }

    stream << "\ninterval=" << authoritativeTiming_.intervalIndex
           << " pos=" << authoritativeTiming_.samplesIntoInterval
           << "/" << authoritativeTiming_.activeIntervalSamples
           << " bpm=" << authoritativeTiming_.activeBpm
           << " bpi=" << authoritativeTiming_.activeBpi
           << " sr=" << juce::roundToInt(currentSampleRate_)
           << " anchor=" << (intervalPhaseAnchoredFromDownloadBegin_ ? "dl" : "cfg")
           << " txWarm=" << transmitWarmupIntervalsRemaining_
           << " upl=" << localUploadIntervalWritePosition_
           << "/" << authoritativeTiming_.activeIntervalSamples
           << " encLast=" << codecStreamBridge_.getLastEncodedPayloadBytes()
           << " rx=" << framedTransport_.getReceivedFrameCount()
           << " tx=" << framedTransport_.getSentFrameCount();

    if (remoteReceiveDiagnosticsBySource_.empty()
        && remoteQueuedIntervalsBySource_.empty()
        && remoteActiveIntervalBySource_.empty())
    {
        stream << "\nno remote sources";
        return stream.str();
    }

    std::vector<std::string> sourceIds;
    sourceIds.reserve(remoteReceiveDiagnosticsBySource_.size()
                      + remoteQueuedIntervalsBySource_.size()
                      + remoteActiveIntervalBySource_.size());

    for (const auto& [sourceId, runtime] : remoteReceiveDiagnosticsBySource_)
    {
        (void) runtime;
        sourceIds.push_back(sourceId);
    }
    for (const auto& [sourceId, queued] : remoteQueuedIntervalsBySource_)
    {
        (void) queued;
        if (std::find(sourceIds.begin(), sourceIds.end(), sourceId) == sourceIds.end())
            sourceIds.push_back(sourceId);
    }
    for (const auto& [sourceId, active] : remoteActiveIntervalBySource_)
    {
        (void) active;
        if (std::find(sourceIds.begin(), sourceIds.end(), sourceId) == sourceIds.end())
            sourceIds.push_back(sourceId);
    }
    for (const auto& [sourceId, voice] : remoteVoiceChunksBySource_)
    {
        (void) voice;
        if (std::find(sourceIds.begin(), sourceIds.end(), sourceId) == sourceIds.end())
            sourceIds.push_back(sourceId);
    }

    std::sort(sourceIds.begin(), sourceIds.end());

    bool firstSource = true;
    for (const auto& sourceId : sourceIds)
    {
        const auto diagnosticsIt = remoteReceiveDiagnosticsBySource_.find(sourceId);
        const auto queuedIt = remoteQueuedIntervalsBySource_.find(sourceId);
        const auto activeIt = remoteActiveIntervalBySource_.find(sourceId);
        const auto voiceIt = remoteVoiceChunksBySource_.find(sourceId);
        const auto nextIt = nextRemoteTargetBoundaryBySource_.find(sourceId);
        const auto lastIt = lastRemoteActivationBoundaryBySource_.find(sourceId);
        const auto knownIt = knownRemoteSourcesById_.find(sourceId);
        const auto stripIt = mixerStripsBySourceId_.find(sourceId);

        const auto decodedSamples = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastDecodedSamples : 0;
        const auto decodedRate = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastDecodedSampleRate : 0.0;
        const auto channelFlags = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end()
            ? diagnosticsIt->second.channelFlags
            : static_cast<std::uint8_t>(0);
        const auto encodedBytes = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastEncodedBytes : 0u;
        const auto copiedSamples = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastCopiedSamples : 0;
        const auto lastInboundInterval = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastInboundIntervalIndex : 0u;
        const auto queuedBoundary = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastQueuedBoundary : 0u;
        const auto lastDropTargetBoundary = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastDropTargetBoundary : 0u;
        const auto lastDropCurrentBoundary = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lastDropCurrentBoundary : 0u;
        const auto lateDrops = diagnosticsIt != remoteReceiveDiagnosticsBySource_.end() ? diagnosticsIt->second.lateDrops : 0u;
        const auto queuedCount = queuedIt != remoteQueuedIntervalsBySource_.end() ? queuedIt->second.size() : 0u;
        const auto activeSamples = activeIt != remoteActiveIntervalBySource_.end()
            ? activeIt->second.getNumSamples()
            : (voiceIt != remoteVoiceChunksBySource_.end() && ! voiceIt->second.empty()
                   ? voiceIt->second.front().audio.getNumSamples()
                   : 0);
        const auto nextBoundary = nextIt != nextRemoteTargetBoundaryBySource_.end() ? nextIt->second : 0u;
        const auto lastBoundary = lastIt != lastRemoteActivationBoundaryBySource_.end() ? lastIt->second : 0u;
        const auto knownInRoom = knownIt != knownRemoteSourcesById_.end();
        const auto stripVisible = stripIt != mixerStripsBySourceId_.end() ? stripIt->second.snapshot.descriptor.visible : false;
        const auto stripActive = stripIt != mixerStripsBySourceId_.end() ? stripIt->second.snapshot.descriptor.active : false;
        const auto presenceAt = stripIt != mixerStripsBySourceId_.end() ? stripIt->second.lastPresenceIntervalIndex : 0u;
        const auto audioAt = stripIt != mixerStripsBySourceId_.end() ? stripIt->second.lastAudioIntervalIndex : 0u;
        const auto stripState = ! stripVisible ? "hidden"
            : (stripActive ? (isVoiceMode(channelFlags) ? "voice" : "interval")
                           : (isVoiceMode(channelFlags) ? "voice" : "idle"));
        const auto remoteUser = knownIt != knownRemoteSourcesById_.end()
            ? knownIt->second.userName
            : (stripIt != mixerStripsBySourceId_.end() ? stripIt->second.snapshot.descriptor.userName : std::string {});
        const auto fallbackChannelIndex = stripIt != mixerStripsBySourceId_.end()
            ? juce::jlimit(0, 31, stripIt->second.snapshot.descriptor.channelIndex)
            : 0;
        const auto channelIndex = knownIt != knownRemoteSourcesById_.end()
            ? knownIt->second.channelIndex
            : static_cast<std::uint8_t>(fallbackChannelIndex);
        std::uint32_t subscribedMask = 0;
        const auto hasSubscribedMask = ! remoteUser.empty() && framedTransport_.getSubscribedUserMask(remoteUser, subscribedMask);
        const auto subscribed = hasSubscribedMask && (subscribedMask & (1u << channelIndex)) != 0u;

        stream << (firstSource ? "\n" : "\n\n") << sourceId
               << " mode=" << (channelFlags == 2u ? "voice" : (channelFlags == 0u ? "interval" : ("flags" + std::to_string(channelFlags))))
               << " known=" << (knownInRoom ? "yes" : "no")
               << " sub=" << (subscribed ? "yes" : "no")
               << " mask=" << subscribedMask
               << " strip=" << stripState
               << " enc=" << encodedBytes
               << " dec=" << decodedSamples << "@" << juce::roundToInt(decodedRate)
               << " copy=" << copiedSamples
               << " active=" << activeSamples
               << " queued=" << queuedCount
                << " queuedAt=" << queuedBoundary
               << " in=" << lastInboundInterval
               << " seenUser=" << presenceAt
               << " seenAudio=" << audioAt
               << " next=" << nextBoundary
               << " last=" << lastBoundary
               << " drop=" << lastDropTargetBoundary << "->" << lastDropCurrentBoundary
               << " drops=" << lateDrops;
        firstSource = false;
    }

    return stream.str();
}

void FamaLamaJamAudioProcessor::injectDecodedRemoteIntervalForTesting(const std::string& sourceId,
                                                                      const juce::AudioBuffer<float>& audio,
                                                                      double sampleRate)
{
    if (sourceId.empty())
        return;

    registerRemoteSourceForTesting(sourceId, sourceId, "Channel 1", 0u);

    const auto decodedAtHostRate = audio::resampleAudioBuffer(audio, sampleRate, currentSampleRate_);
    auto& diagnostics = remoteReceiveDiagnosticsBySource_[sourceId];
    diagnostics.lastEncodedBytes = 0;
    diagnostics.lastDecodedSamples = decodedAtHostRate.getNumSamples();
    diagnostics.lastDecodedSampleRate = sampleRate;
    {
        const std::scoped_lock lock(stemCaptureMutex_);
        if (stemCaptureState_.activeRecording)
            captureRemoteStem(sourceId, decodedAtHostRate, false);
    }
    queueDecodedRemoteInterval(remoteQueuedIntervalsBySource_,
                               nextRemoteTargetBoundaryBySource_,
                               remoteReceiveDiagnosticsBySource_,
                               authoritativeTiming_,
                               sourceId,
                               decodedAtHostRate);
}

void FamaLamaJamAudioProcessor::injectDecodedRemoteVoiceForTesting(const std::string& sourceId,
                                                                   const juce::AudioBuffer<float>& audio,
                                                                   double sampleRate)
{
    if (sourceId.empty())
        return;

    registerRemoteSourceForTesting(sourceId, sourceId, "Voice Chat", 0x2u);

    const auto decodedAtHostRate = audio::resampleAudioBuffer(audio, sampleRate, currentSampleRate_);
    auto& diagnostics = remoteReceiveDiagnosticsBySource_[sourceId];
    diagnostics.lastEncodedBytes = 0;
    diagnostics.lastDecodedSamples = decodedAtHostRate.getNumSamples();
    diagnostics.lastDecodedSampleRate = sampleRate;
    diagnostics.lastCopiedSamples = decodedAtHostRate.getNumSamples();
    diagnostics.lastQueuedBoundary = authoritativeTiming_.intervalIndex;
    {
        const std::scoped_lock lock(stemCaptureMutex_);
        retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, false, currentSampleRate_));
        retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, true, currentSampleRate_));
    }
    queueDecodedRemoteVoiceChunk(remoteVoiceChunksBySource_, sourceId, std::move(decodedAtHostRate));
}

void FamaLamaJamAudioProcessor::registerRemoteSourceForTesting(const std::string& sourceId,
                                                               const std::string& userName,
                                                               const std::string& channelName,
                                                               std::uint8_t channelFlags)
{
    if (sourceId.empty())
        return;

    net::FramedSocketTransport::RemoteSourceInfo sourceInfo;
    sourceInfo.sourceId = sourceId;
    sourceInfo.groupId = userName.empty() ? sourceId : userName;
    sourceInfo.userName = userName.empty() ? sourceId : userName;
    sourceInfo.channelName = channelName;
    sourceInfo.displayName = sourceInfo.userName + " - " + channelName;
    sourceInfo.channelFlags = channelFlags;
    syncRemoteMixerStrip(sourceInfo, false);
}

FamaLamaJamAudioProcessor::CpuDiagnosticSnapshot FamaLamaJamAudioProcessor::getCpuDiagnosticSnapshotForTesting() const noexcept
{
    return cpuDiagnosticSnapshot_;
}

void FamaLamaJamAudioProcessor::resetCpuDiagnosticSnapshotForTesting() noexcept
{
    cpuDiagnosticSnapshot_ = {};
}

bool FamaLamaJamAudioProcessor::setStemCaptureDirectoryForTesting(const juce::File& directory, bool enabled)
{
    app::session::StemCaptureSettings settings = getStemCaptureSettings();
    settings.outputDirectory = directory.getFullPathName().toStdString();
    settings.enabled = enabled;
    return applyStemCaptureSettings(std::move(settings));
}

bool FamaLamaJamAudioProcessor::waitForStemCaptureFlushForTesting(int timeoutMs) const
{
    return stemCaptureWriter_->flushForTesting(timeoutMs);
}

juce::File FamaLamaJamAudioProcessor::getStemCaptureSessionDirectoryForTesting() const
{
    return stemCaptureWriter_->getCurrentSessionDirectoryForTesting();
}

std::vector<juce::File> FamaLamaJamAudioProcessor::getWrittenStemFilesForTesting() const
{
    return stemCaptureWriter_->getWrittenFilesForTesting();
}

FamaLamaJamAudioProcessor::TransportUiState FamaLamaJamAudioProcessor::getTransportUiState() const noexcept
{
    const auto snapshot = lifecycleController_.getSnapshot();
    const auto hasServerTiming = hasServerTimingForUi_.load(std::memory_order_relaxed);
    const auto syncHealth = computeSyncHealth(snapshot, hasServerTiming);
    const bool assistHoldingTransport = hostSyncAssistArmed_.load(std::memory_order_relaxed)
        || hostSyncAssistFailed_.load(std::memory_order_relaxed);

    return TransportUiState {
        .connected = snapshot.isConnected(),
        .hasServerTiming = hasServerTiming,
        .syncHealth = syncHealth,
        .metronomeAvailable = syncHealth == SyncHealth::Healthy && ! assistHoldingTransport,
        .beatsPerMinute = serverBpmForUi_.load(std::memory_order_relaxed),
        .beatsPerInterval = beatsPerIntervalForUi_.load(std::memory_order_relaxed),
        .currentBeat = currentBeatForUi_.load(std::memory_order_relaxed),
        .intervalProgress = intervalProgressForUi_.load(std::memory_order_relaxed),
        .intervalIndex = intervalIndexForUi_.load(std::memory_order_relaxed),
    };
}

FamaLamaJamAudioProcessor::HostSyncAssistUiState FamaLamaJamAudioProcessor::getHostSyncAssistUiState() const noexcept
{
    const auto blockReason = hostSyncAssistBlockReason_.load(std::memory_order_relaxed);
    const auto failureReason = hostSyncAssistFailureReason_.load(std::memory_order_relaxed);

    return HostSyncAssistUiState {
        .armable = blockReason == HostSyncAssistBlockReason::None,
        .armed = hostSyncAssistArmed_.load(std::memory_order_relaxed),
        .waitingForHost = hostSyncAssistWaitingForHost_.load(std::memory_order_relaxed),
        .blocked = blockReason != HostSyncAssistBlockReason::None,
        .failed = hostSyncAssistFailed_.load(std::memory_order_relaxed),
        .blockReason = blockReason,
        .failureReason = failureReason,
        .targetBeatsPerMinute = hostSyncAssistTargetBpmForUi_.load(std::memory_order_relaxed),
        .targetBeatsPerInterval = hostSyncAssistTargetBpiForUi_.load(std::memory_order_relaxed),
        .hostTransport = HostTransportSnapshot {
            .available = hostTransportAvailableForUi_.load(std::memory_order_relaxed),
            .playing = hostTransportPlayingForUi_.load(std::memory_order_relaxed),
            .hasTempo = hostTransportHasTempoForUi_.load(std::memory_order_relaxed),
            .tempoBpm = hostTransportTempoBpmForUi_.load(std::memory_order_relaxed),
            .hasPpqPosition = hostTransportHasPpqPositionForUi_.load(std::memory_order_relaxed),
            .ppqPosition = hostTransportPpqPositionForUi_.load(std::memory_order_relaxed),
            .hasBarStartPpqPosition = hostTransportHasBarStartPpqPositionForUi_.load(std::memory_order_relaxed),
            .ppqPositionOfLastBarStart = hostTransportBarStartPpqPositionForUi_.load(std::memory_order_relaxed),
            .hasTimeSignature = hostTransportHasTimeSignatureForUi_.load(std::memory_order_relaxed),
            .timeSigNumerator = hostTransportTimeSigNumeratorForUi_.load(std::memory_order_relaxed),
            .timeSigDenominator = hostTransportTimeSigDenominatorForUi_.load(std::memory_order_relaxed),
        },
    };
}

FamaLamaJamAudioProcessor::RoomUiState FamaLamaJamAudioProcessor::getRoomUiState() const
{
    const_cast<FamaLamaJamAudioProcessor*>(this)->drainRoomTransportEvents();
    const_cast<FamaLamaJamAudioProcessor*>(this)->refreshPendingRoomVotesFromTiming();

    const std::scoped_lock lock(roomUiMutex_);
    auto state = roomUiState_;
    state.connected = isSessionConnected();
    return state;
}

FamaLamaJamAudioProcessor::ServerDiscoveryUiState FamaLamaJamAudioProcessor::getServerDiscoveryUiState() const
{
    const_cast<FamaLamaJamAudioProcessor*>(this)->drainPublicServerDiscoveryResults();

    ServerDiscoveryUiState state;
    std::unordered_map<std::string, const app::session::RememberedServerEntry*> rememberedByEndpoint;
    std::unordered_set<std::string> seenEndpoints;

    const std::scoped_lock lock(serverDiscoveryMutex_);
    state.fetchInProgress = publicServerListFetchInProgress_;
    state.hasStalePublicData = publicServerListStale_ && ! cachedPublicServers_.empty();
    state.statusText = publicServerListStatusText_;
    state.combinedEntries.reserve(rememberedServers_.size() + cachedPublicServers_.size());

    for (const auto& entry : rememberedServers_)
    {
        const auto key = app::session::makeDiscoveryEndpointKey(entry.host, entry.port);
        rememberedByEndpoint.emplace(std::move(key), &entry);
    }

    for (const auto& entry : cachedPublicServers_)
    {
        const auto key = app::session::makeDiscoveryEndpointKey(entry.host, entry.port);
        const auto rememberedIt = rememberedByEndpoint.find(key);
        const auto* remembered = rememberedIt != rememberedByEndpoint.end() ? rememberedIt->second : nullptr;
        seenEndpoints.insert(key);

        state.combinedEntries.push_back(ServerDiscoveryEntry {
            .source = ServerDiscoveryEntry::Source::Public,
            .label = makePublicDiscoveryEntryLabel(entry),
            .host = entry.host,
            .port = entry.port,
            .username = remembered != nullptr ? remembered->username : std::string {},
            .password = remembered != nullptr ? remembered->password : std::string {},
            .connectedUsers = entry.connectedUsers,
            .maxUsers = entry.maxUsers,
            .stale = publicServerListStale_,
        });
    }

    for (const auto& entry : rememberedServers_)
    {
        const auto key = app::session::makeDiscoveryEndpointKey(entry.host, entry.port);
        if (seenEndpoints.contains(key))
            continue;

        state.combinedEntries.push_back(ServerDiscoveryEntry {
            .source = ServerDiscoveryEntry::Source::Remembered,
            .label = makeDiscoveryEntryLabel(entry.host, entry.port),
            .host = entry.host,
            .port = entry.port,
            .username = entry.username,
            .password = entry.password,
            .connectedUsers = -1,
            .maxUsers = -1,
            .stale = false,
        });
    }

    return state;
}

bool FamaLamaJamAudioProcessor::requestPublicServerDiscoveryRefresh(bool manual)
{
    std::scoped_lock lock(serverDiscoveryMutex_);
    if (publicServerDiscoveryClient_ == nullptr)
        return false;

    if (! publicServerDiscoveryClient_->requestRefresh(manual))
        return false;

    publicServerListFetchInProgress_ = true;
    publicServerListStatusText_ = cachedPublicServers_.empty()
        ? "Refreshing public server list..."
        : "Refreshing public server list. Showing cached servers in the meantime.";
    return true;
}

bool FamaLamaJamAudioProcessor::toggleHostSyncAssistArm()
{
    if (hostSyncAssistArmed_.load(std::memory_order_relaxed))
    {
        clearHostSyncAssistState();
        return false;
    }

    const auto hostTransport = HostTransportSnapshot {
        .available = hostTransportAvailableForUi_.load(std::memory_order_relaxed),
        .playing = hostTransportPlayingForUi_.load(std::memory_order_relaxed),
        .hasTempo = hostTransportHasTempoForUi_.load(std::memory_order_relaxed),
        .tempoBpm = hostTransportTempoBpmForUi_.load(std::memory_order_relaxed),
        .hasPpqPosition = hostTransportHasPpqPositionForUi_.load(std::memory_order_relaxed),
        .ppqPosition = hostTransportPpqPositionForUi_.load(std::memory_order_relaxed),
        .hasBarStartPpqPosition = hostTransportHasBarStartPpqPositionForUi_.load(std::memory_order_relaxed),
        .ppqPositionOfLastBarStart = hostTransportBarStartPpqPositionForUi_.load(std::memory_order_relaxed),
        .hasTimeSignature = hostTransportHasTimeSignatureForUi_.load(std::memory_order_relaxed),
        .timeSigNumerator = hostTransportTimeSigNumeratorForUi_.load(std::memory_order_relaxed),
        .timeSigDenominator = hostTransportTimeSigDenominatorForUi_.load(std::memory_order_relaxed),
    };
    const auto targetBpm = hostSyncAssistTargetBpmForUi_.load(std::memory_order_relaxed);
    const auto blockReason = computeHostSyncAssistBlockReason(hasServerTimingForUi_.load(std::memory_order_relaxed),
                                                              targetBpm,
                                                              hostTransport);

    hostSyncAssistBlockReason_.store(blockReason, std::memory_order_relaxed);
    if (blockReason != HostSyncAssistBlockReason::None)
        return false;

    hostSyncAssistArmed_.store(true, std::memory_order_relaxed);
    hostSyncAssistWaitingForHost_.store(true, std::memory_order_relaxed);
    hostSyncAssistFailed_.store(false, std::memory_order_relaxed);
    hostSyncAssistFailureReason_.store(HostSyncAssistFailureReason::None, std::memory_order_relaxed);
    return true;
}

bool FamaLamaJamAudioProcessor::sendRoomChatMessage(std::string text)
{
    text = trimRoomText(std::move(text));

    if (text.empty() || ! isSessionConnected() || ! experimentalStreamingEnabled_)
        return false;

    return framedTransport_.enqueueRoomMessage(std::array<std::string, 5> { "MSG", std::move(text), {}, {}, {} });
}

bool FamaLamaJamAudioProcessor::submitRoomVote(RoomVoteKind kind, int value)
{
    if (! isSessionConnected() || ! experimentalStreamingEnabled_ || value <= 0)
        return false;

    if (! framedTransport_.enqueueRoomMessage(
            std::array<std::string, 5> { "MSG", makeVoteCommandText(kind, value), {}, {}, {} }))
        return false;

    std::scoped_lock lock(roomUiMutex_);
    auto& voteState = kind == RoomVoteKind::Bpm ? roomUiState_.bpmVote : roomUiState_.bpiVote;
    voteState.pending = true;
    voteState.failed = false;
    voteState.requestedValue = value;
    voteState.statusText = "Vote pending";
    return true;
}

bool FamaLamaJamAudioProcessor::isMetronomeEnabled() const noexcept
{
    return metronomeEnabled_.load(std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::setMetronomeEnabled(bool enabled) noexcept
{
    metronomeEnabled_.store(enabled, std::memory_order_relaxed);
}

float FamaLamaJamAudioProcessor::getMetronomeGainDb() const noexcept
{
    return metronomeGainDb_.load(std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::setMetronomeGainDb(float gainDb) noexcept
{
    metronomeGainDb_.store(normalizeMetronomeGainDb(gainDb), std::memory_order_relaxed);
}

float FamaLamaJamAudioProcessor::getMasterOutputGainDb() const noexcept
{
    return masterOutputGainDb_.load(std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::setMasterOutputGainDb(float gainDb) noexcept
{
    masterOutputGainDb_.store(normalizeMasterOutputGainDb(gainDb), std::memory_order_relaxed);
}

FamaLamaJamAudioProcessor::TransmitState FamaLamaJamAudioProcessor::getTransmitState() const noexcept
{
    if (! transmitEnabled_ || ! isSessionConnected())
        return TransmitState::Disabled;

    if (isLocalVoiceMode(localChannelMode_))
        return TransmitState::Active;

    if (! authoritativeTiming_.hasTiming || transmitWarmupIntervalsRemaining_ > 0)
        return TransmitState::WarmingUp;

    return TransmitState::Active;
}

bool FamaLamaJamAudioProcessor::isLocalVoiceModeEnabled() const noexcept
{
    return isLocalVoiceMode(localChannelMode_);
}

std::vector<FamaLamaJamAudioProcessor::MixerStripSnapshot> FamaLamaJamAudioProcessor::getMixerStripSnapshots() const
{
    std::vector<MixerStripSnapshot> snapshots;
    snapshots.reserve(mixerStripsBySourceId_.size());

    for (const auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        auto snapshot = runtimeState.snapshot;
        if (snapshot.descriptor.kind == MixerStripKind::LocalMonitor)
        {
            snapshot.transmitState = getTransmitState();
            snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
            snapshot.unsupportedVoiceMode = false;
            snapshot.statusText = makeLocalTransmitStatus(localChannelMode_,
                                                          snapshot.transmitState,
                                                          localVoiceTransitionPending_);
        }
        snapshots.push_back(std::move(snapshot));
    }

    std::sort(snapshots.begin(),
              snapshots.end(),
              [](const MixerStripSnapshot& lhs, const MixerStripSnapshot& rhs) {
                  if (lhs.descriptor.kind != rhs.descriptor.kind)
                      return lhs.descriptor.kind == MixerStripKind::LocalMonitor;
                  if (lhs.descriptor.groupId != rhs.descriptor.groupId)
                      return lhs.descriptor.groupId < rhs.descriptor.groupId;
                  if (lhs.descriptor.channelIndex != rhs.descriptor.channelIndex)
                      return lhs.descriptor.channelIndex < rhs.descriptor.channelIndex;
                  return lhs.descriptor.sourceId < rhs.descriptor.sourceId;
              });

    return snapshots;
}

bool FamaLamaJamAudioProcessor::getMixerStripSnapshot(const std::string& sourceId, MixerStripSnapshot& snapshot) const
{
    if (const auto it = mixerStripsBySourceId_.find(sourceId); it != mixerStripsBySourceId_.end())
    {
        snapshot = it->second.snapshot;
        if (snapshot.descriptor.kind == MixerStripKind::LocalMonitor)
        {
            snapshot.transmitState = getTransmitState();
            snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
            snapshot.unsupportedVoiceMode = false;
            snapshot.statusText = makeLocalTransmitStatus(localChannelMode_,
                                                          snapshot.transmitState,
                                                          localVoiceTransitionPending_);
        }
        return true;
    }

    return false;
}

bool FamaLamaJamAudioProcessor::setMixerStripMixState(const std::string& sourceId, float gainDb, float pan, bool muted)
{
    if (sourceId.empty())
        return false;

    ensureLocalMonitorMixerStrip();

    if (const auto it = mixerStripsBySourceId_.find(sourceId); it != mixerStripsBySourceId_.end())
    {
        const auto soloed = it->second.snapshot.mix.soloed;
        it->second.snapshot.mix = normalizeMixState(MixerStripMixState {
            .gainDb = gainDb,
            .pan = pan,
            .muted = muted,
            .soloed = soloed,
        });
        return true;
    }

    return false;
}

bool FamaLamaJamAudioProcessor::setMixerStripSoloState(const std::string& sourceId, bool soloed)
{
    if (sourceId.empty())
        return false;

    ensureLocalMonitorMixerStrip();

    if (const auto it = mixerStripsBySourceId_.find(sourceId); it != mixerStripsBySourceId_.end())
    {
        it->second.snapshot.mix.soloed = soloed;
        return true;
    }

    return false;
}

void FamaLamaJamAudioProcessor::setPublicServerDiscoveryClientForTesting(
    std::unique_ptr<infra::net::PublicServerDiscoveryClient> client)
{
    const std::scoped_lock lock(serverDiscoveryMutex_);
    publicServerDiscoveryClient_ = std::move(client);
    publicServerListFetchInProgress_ = publicServerDiscoveryClient_ != nullptr
        && publicServerDiscoveryClient_->isRefreshInProgress();
}

bool FamaLamaJamAudioProcessor::requestPublicServerDiscoveryRefreshForTesting(bool manual)
{
    return requestPublicServerDiscoveryRefresh(manual);
}

void FamaLamaJamAudioProcessor::timerCallback()
{
    drainRoomTransportEvents();
    drainPublicServerDiscoveryResults();
    refreshPendingRoomVotesFromTiming();
    triggerScheduledReconnectForTesting();
}

void FamaLamaJamAudioProcessor::applyLifecycleTransition(const app::session::ConnectionLifecycleTransition& transition)
{
    if (! transition.changed)
        return;

    lastStatusMessage_ = transition.snapshot.statusMessage;

    if (transition.effect == app::session::LifecycleEffect::ScheduleReconnect)
        scheduleReconnectTimer(transition.snapshot.nextRetryDelayMs);
    else
        clearReconnectTimer();

    if (transition.snapshot.state == app::session::ConnectionState::Active)
    {
        codecStreamBridge_.start();
        clearRoomUiState(true);
        if (experimentalStreamingEnabled_ && transmitEnabled_)
            transmitWarmupIntervalsRemaining_ = kPreparedTransmitIntervals;
    }
    else
    {
        codecStreamBridge_.stop();
        localUploadIntervalBuffer_.setSize(0, 0);
        localVoiceUploadBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        localVoiceUploadWritePosition_ = 0;
        lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
        lastDecodedSamples_.store(0, std::memory_order_relaxed);
        remoteQueuedIntervalsBySource_.clear();
        remoteVoiceChunksBySource_.clear();
        nextRemoteTargetBoundaryBySource_.clear();
        remoteActiveIntervalBySource_.clear();
        lastRemoteActivationBoundaryBySource_.clear();
        remoteReceiveDiagnosticsBySource_.clear();
        knownRemoteSourcesById_.clear();
        clearAuthoritativeTiming();
        clearRoomUiState(false);
        hideAllRemoteMixerStrips();

        if (transition.snapshot.state == app::session::ConnectionState::Idle
            || transition.snapshot.state == app::session::ConnectionState::Error
            || transition.snapshot.state == app::session::ConnectionState::Reconnecting)
        {
            closeLiveSocket();
        }
    }

    if (! liveTransportEnabled_)
        return;

    if (transition.effect == app::session::LifecycleEffect::BeginConnect)
    {
        attemptLiveConnect();
        return;
    }

    if (transition.effect == app::session::LifecycleEffect::BeginDisconnect
        || transition.effect == app::session::LifecycleEffect::CancelPendingConnect)
    {
        closeLiveSocket();
    }
}

void FamaLamaJamAudioProcessor::clearReconnectTimer()
{
    reconnectScheduled_ = false;
    scheduledReconnectDelayMs_ = 0;
    stopTimer();
}

void FamaLamaJamAudioProcessor::scheduleReconnectTimer(int delayMs)
{
    const auto boundedDelay = juce::jmax(1, delayMs);
    reconnectScheduled_ = true;
    scheduledReconnectDelayMs_ = boundedDelay;
    startTimer(boundedDelay);
}

void FamaLamaJamAudioProcessor::clearAuthoritativeTiming() noexcept
{
    const bool wasArmed = hostSyncAssistArmed_.load(std::memory_order_relaxed);
    const bool wasWaitingForHost = hostSyncAssistWaitingForHost_.load(std::memory_order_relaxed);

    authoritativeTiming_ = {};
    lastHandledIntervalBoundaryGeneration_ = 0;
    intervalPhaseAnchoredFromDownloadBegin_ = false;
    localUploadIntervalBuffer_.setSize(0, 0);
    localVoiceUploadBuffer_.setSize(0, 0);
    localUploadIntervalWritePosition_ = 0;
    localVoiceUploadWritePosition_ = 0;
    transmitWarmupIntervalsRemaining_ = 0;
    nextRemoteTargetBoundaryBySource_.clear();
    lastRemoteActivationBoundaryBySource_.clear();
    remoteVoiceChunksBySource_.clear();
    remoteReceiveDiagnosticsBySource_.clear();
    knownRemoteSourcesById_.clear();
    hasServerTimingForUi_.store(false, std::memory_order_relaxed);
    serverBpmForUi_.store(0, std::memory_order_relaxed);
    beatsPerIntervalForUi_.store(0, std::memory_order_relaxed);
    currentBeatForUi_.store(0, std::memory_order_relaxed);
    intervalProgressForUi_.store(0.0f, std::memory_order_relaxed);
    intervalIndexForUi_.store(0, std::memory_order_relaxed);
    metronomeClickRemainingSamples_ = 0;
    hostSyncAssistTargetBpmForUi_.store(0, std::memory_order_relaxed);
    hostSyncAssistTargetBpiForUi_.store(0, std::memory_order_relaxed);
    hostSyncAssistBlockReason_.store(HostSyncAssistBlockReason::MissingServerTiming, std::memory_order_relaxed);
    hostSyncAssistArmed_.store(false, std::memory_order_relaxed);
    hostSyncAssistWaitingForHost_.store(false, std::memory_order_relaxed);

    if (wasArmed || wasWaitingForHost)
    {
        hostSyncAssistFailed_.store(true, std::memory_order_relaxed);
        hostSyncAssistFailureReason_.store(HostSyncAssistFailureReason::TimingLost, std::memory_order_relaxed);
    }

    previousHostTransportPlaying_ = false;
    clearStemCaptureState(true);
}

void FamaLamaJamAudioProcessor::clearHostTransportSnapshot() noexcept
{
    hostTransportAvailableForUi_.store(false, std::memory_order_relaxed);
    hostTransportPlayingForUi_.store(false, std::memory_order_relaxed);
    hostTransportHasTempoForUi_.store(false, std::memory_order_relaxed);
    hostTransportTempoBpmForUi_.store(0.0, std::memory_order_relaxed);
    hostTransportHasPpqPositionForUi_.store(false, std::memory_order_relaxed);
    hostTransportPpqPositionForUi_.store(0.0, std::memory_order_relaxed);
    hostTransportHasBarStartPpqPositionForUi_.store(false, std::memory_order_relaxed);
    hostTransportBarStartPpqPositionForUi_.store(0.0, std::memory_order_relaxed);
    hostTransportHasTimeSignatureForUi_.store(false, std::memory_order_relaxed);
    hostTransportTimeSigNumeratorForUi_.store(0, std::memory_order_relaxed);
    hostTransportTimeSigDenominatorForUi_.store(0, std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::clearHostSyncAssistState() noexcept
{
    hostSyncAssistArmed_.store(false, std::memory_order_relaxed);
    hostSyncAssistWaitingForHost_.store(false, std::memory_order_relaxed);
    hostSyncAssistFailed_.store(false, std::memory_order_relaxed);
    hostSyncAssistFailureReason_.store(HostSyncAssistFailureReason::None, std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::armStemCaptureForNextBoundary()
{
    std::scoped_lock lock(stemCaptureMutex_);
    if (! stemCaptureState_.settings.enabled || stemCaptureState_.settings.outputDirectory.empty())
        return;

    stemCaptureState_.pendingNewRunAtBoundary = false;

    if (! authoritativeTiming_.hasTiming || authoritativeTiming_.activeBpm <= 0 || authoritativeTiming_.activeBpi <= 0)
    {
        stemCaptureState_.sessionPrepared = false;
        stemCaptureState_.activeRecording = false;
        stemCaptureState_.pendingStartAtBoundary = true;
        stemCaptureState_.pendingStopAtBoundary = false;
        return;
    }

    if (! stemCaptureState_.sessionPrepared)
    {
        const audio::StemCaptureWriter::SessionConfig sessionConfig {
            .baseDirectory = juce::File(stemCaptureState_.settings.outputDirectory),
            .beatsPerMinute = authoritativeTiming_.activeBpm,
            .beatsPerInterval = authoritativeTiming_.activeBpi,
            .startedAt = juce::Time::getCurrentTime(),
        };
        stemCaptureWriter_->beginSession(sessionConfig);
        stemCaptureState_.sessionPrepared = true;
        stemCaptureState_.sessionStartedAt = sessionConfig.startedAt;
        stemCaptureState_.sessionBeatsPerMinute = sessionConfig.beatsPerMinute;
        stemCaptureState_.sessionBeatsPerInterval = sessionConfig.beatsPerInterval;
    }

    stemCaptureState_.activeRecording = false;
    stemCaptureState_.pendingStartAtBoundary = true;
    stemCaptureState_.pendingStopAtBoundary = false;
}

void FamaLamaJamAudioProcessor::requestStemCaptureStop()
{
    std::scoped_lock lock(stemCaptureMutex_);
    stemCaptureState_.pendingNewRunAtBoundary = false;
    if (stemCaptureState_.activeRecording)
    {
        stemCaptureState_.pendingStopAtBoundary = true;
    }
    else
    {
        stemCaptureState_.pendingStartAtBoundary = false;
        stemCaptureState_.pendingStopAtBoundary = false;
        stemCaptureState_.sessionPrepared = false;
        stemCaptureWriter_->endSession();
    }
}

void FamaLamaJamAudioProcessor::clearStemCaptureState(bool preserveArmedState)
{
    std::scoped_lock lock(stemCaptureMutex_);
    stemCaptureWriter_->pauseSession();
    stemCaptureState_.activeRecording = false;
    stemCaptureState_.pendingStopAtBoundary = false;
    stemCaptureState_.pendingNewRunAtBoundary = false;
    stemCaptureState_.suppressLocalIntervalStemUntilBoundary = false;

    if (! preserveArmedState)
    {
        stemCaptureState_.pendingStartAtBoundary = false;
        stemCaptureState_.sessionPrepared = false;
        stemCaptureState_.settings.enabled = false;
        stemCaptureWriter_->endSession();
        return;
    }

    const bool canResume = stemCaptureState_.settings.enabled
        && ! stemCaptureState_.settings.outputDirectory.empty();
    stemCaptureState_.pendingStartAtBoundary = canResume;
}

void FamaLamaJamAudioProcessor::handleStemCaptureBoundary()
{
    std::scoped_lock lock(stemCaptureMutex_);

    stemCaptureState_.suppressLocalIntervalStemUntilBoundary = false;

    if (stemCaptureState_.pendingStopAtBoundary)
    {
        stemCaptureState_.activeRecording = false;
        stemCaptureState_.pendingStopAtBoundary = false;
        stemCaptureState_.pendingStartAtBoundary = false;
        stemCaptureState_.pendingNewRunAtBoundary = false;
        stemCaptureState_.sessionPrepared = false;
        stemCaptureWriter_->endSession();
        return;
    }

    if (stemCaptureState_.pendingNewRunAtBoundary && stemCaptureState_.settings.enabled)
    {
        stemCaptureWriter_->endSession();
        stemCaptureState_.sessionPrepared = false;
        stemCaptureState_.activeRecording = false;
        stemCaptureState_.pendingStartAtBoundary = false;
        stemCaptureState_.pendingNewRunAtBoundary = false;

        if (! stemCaptureState_.settings.outputDirectory.empty()
            && authoritativeTiming_.activeBpm > 0
            && authoritativeTiming_.activeBpi > 0)
        {
            const audio::StemCaptureWriter::SessionConfig sessionConfig {
                .baseDirectory = juce::File(stemCaptureState_.settings.outputDirectory),
                .beatsPerMinute = authoritativeTiming_.activeBpm,
                .beatsPerInterval = authoritativeTiming_.activeBpi,
                .startedAt = juce::Time::getCurrentTime(),
            };
            stemCaptureWriter_->beginSession(sessionConfig);
            stemCaptureState_.sessionPrepared = true;
            stemCaptureState_.sessionStartedAt = sessionConfig.startedAt;
            stemCaptureState_.sessionBeatsPerMinute = sessionConfig.beatsPerMinute;
            stemCaptureState_.sessionBeatsPerInterval = sessionConfig.beatsPerInterval;
            stemCaptureState_.activeRecording = true;
        }
        return;
    }

    if (stemCaptureState_.pendingStartAtBoundary && stemCaptureState_.settings.enabled)
    {
        if (! stemCaptureState_.sessionPrepared
            && ! stemCaptureState_.settings.outputDirectory.empty()
            && authoritativeTiming_.activeBpm > 0
            && authoritativeTiming_.activeBpi > 0)
        {
            const audio::StemCaptureWriter::SessionConfig sessionConfig {
                .baseDirectory = juce::File(stemCaptureState_.settings.outputDirectory),
                .beatsPerMinute = authoritativeTiming_.activeBpm,
                .beatsPerInterval = authoritativeTiming_.activeBpi,
                .startedAt = juce::Time::getCurrentTime(),
            };
            stemCaptureWriter_->beginSession(sessionConfig);
            stemCaptureState_.sessionPrepared = true;
            stemCaptureState_.sessionStartedAt = sessionConfig.startedAt;
            stemCaptureState_.sessionBeatsPerMinute = sessionConfig.beatsPerMinute;
            stemCaptureState_.sessionBeatsPerInterval = sessionConfig.beatsPerInterval;
        }

        stemCaptureState_.activeRecording = true;
        stemCaptureState_.pendingStartAtBoundary = false;
    }
}

void FamaLamaJamAudioProcessor::captureLocalIntervalStem(const juce::AudioBuffer<float>& intervalBuffer)
{
    stemCaptureWriter_->submitFrame(audio::StemCaptureWriter::Frame {
        .source = makeLocalStemSourceConfig(false, currentSampleRate_),
        .audio = intervalBuffer,
    });
}

void FamaLamaJamAudioProcessor::captureLocalVoiceStem(const juce::AudioBuffer<float>& voiceChunk)
{
    juce::ignoreUnused(voiceChunk);
}

void FamaLamaJamAudioProcessor::captureRemoteStem(const std::string& sourceId,
                                                  const juce::AudioBuffer<float>& audio,
                                                  bool voiceMode)
{
    if (voiceMode)
    {
        retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, false, currentSampleRate_));
        retireStemCaptureSource(makeRemoteStemSourceConfig(sourceId, true, currentSampleRate_));
        return;
    }

    stemCaptureWriter_->submitFrame(audio::StemCaptureWriter::Frame {
        .source = makeRemoteStemSourceConfig(sourceId, voiceMode, currentSampleRate_),
        .audio = audio,
    });
}

void FamaLamaJamAudioProcessor::retireStemCaptureSource(const audio::StemCaptureWriter::SourceConfig& sourceConfig)
{
    stemCaptureWriter_->retireSource(sourceConfig.sourceKey);
}

audio::StemCaptureWriter::SourceConfig FamaLamaJamAudioProcessor::makeLocalStemSourceConfig(bool voiceMode,
                                                                                             double sampleRate) const
{
    const auto settings = settingsStore_.getActiveSettings();
    return {
        .sourceKey = std::string(kLocalMonitorSourceId) + (voiceMode ? "#voice" : "#interval"),
        .userName = settings.username.empty() ? std::string("local") : settings.username,
        .channelName = voiceMode ? std::string("Voice Chat") : std::string("Monitor"),
        .mode = voiceMode ? audio::StemCaptureWriter::Mode::Voice : audio::StemCaptureWriter::Mode::Interval,
        .sampleRate = sampleRate,
    };
}

audio::StemCaptureWriter::SourceConfig FamaLamaJamAudioProcessor::makeRemoteStemSourceConfig(const std::string& sourceId,
                                                                                              bool voiceMode,
                                                                                              double sampleRate) const
{
    const auto sourceIt = knownRemoteSourcesById_.find(sourceId);
    const auto userName = sourceIt != knownRemoteSourcesById_.end() && ! sourceIt->second.userName.empty()
        ? sourceIt->second.userName
        : sourceId;
    const auto channelName = sourceIt != knownRemoteSourcesById_.end() && ! sourceIt->second.channelName.empty()
        ? sourceIt->second.channelName
        : std::string("Channel 1");

    return {
        .sourceKey = sourceId + (voiceMode ? "#voice" : "#interval"),
        .userName = userName,
        .channelName = channelName,
        .mode = voiceMode ? audio::StemCaptureWriter::Mode::Voice : audio::StemCaptureWriter::Mode::Interval,
        .sampleRate = sampleRate,
    };
}

void FamaLamaJamAudioProcessor::drainRoomTransportEvents()
{
    if (! experimentalStreamingEnabled_)
        return;

    net::FramedSocketTransport::RoomEvent event;
    int drainedEvents = 0;

    while (drainedEvents < 32 && framedTransport_.popRoomEvent(event))
    {
        applyRoomEvent(event);
        ++drainedEvents;
    }
}

void FamaLamaJamAudioProcessor::drainPublicServerDiscoveryResults()
{
    infra::net::PublicServerDiscoveryClient* client = nullptr;

    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        client = publicServerDiscoveryClient_.get();
        if (client == nullptr)
            return;
    }

    infra::net::PublicServerDiscoveryClient::Result result;
    bool drainedAny = false;

    while (client->popResult(result))
    {
        drainedAny = true;

        std::scoped_lock lock(serverDiscoveryMutex_);
        publicServerListFetchInProgress_ = client->isRefreshInProgress();

        if (result.succeeded)
        {
            cachedPublicServers_ = app::session::parsePublicServerList(result.payloadText);
            publicServerListStale_ = false;
            publicServerListStatusText_ = cachedPublicServers_.empty() ? "No public servers were listed." : std::string {};
        }
        else
        {
            publicServerListStale_ = ! cachedPublicServers_.empty();
            publicServerListStatusText_ = result.errorText.empty()
                ? "Couldn't refresh the public server list."
                : result.errorText;

            if (publicServerListStale_)
                publicServerListStatusText_ += " Showing cached servers.";
        }
    }

    if (! drainedAny)
    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        if (publicServerDiscoveryClient_ != nullptr)
            publicServerListFetchInProgress_ = publicServerDiscoveryClient_->isRefreshInProgress();
    }
}

void FamaLamaJamAudioProcessor::loadRememberedServersFromStore()
{
    std::vector<app::session::RememberedServerEntry> loadedRememberedServers;
    if (rememberedServerStore_ != nullptr)
        loadedRememberedServers = rememberedServerStore_->load();

    const std::scoped_lock lock(serverDiscoveryMutex_);
    rememberedServers_ = std::move(loadedRememberedServers);
}

void FamaLamaJamAudioProcessor::persistRememberedServers()
{
    if (rememberedServerStore_ == nullptr)
        return;

    std::vector<app::session::RememberedServerEntry> rememberedServers;
    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        rememberedServers = rememberedServers_;
    }

    (void) rememberedServerStore_->save(rememberedServers);
}

void FamaLamaJamAudioProcessor::applyRoomEvent(const net::FramedSocketTransport::RoomEvent& event)
{
    std::scoped_lock lock(roomUiMutex_);

    RoomFeedEntry entry;

    switch (event.kind)
    {
        case net::FramedSocketTransport::RoomEvent::Kind::Chat:
            entry.kind = RoomFeedEntryKind::Chat;
            entry.author = event.author;
            entry.text = event.text;
            break;

        case net::FramedSocketTransport::RoomEvent::Kind::Topic:
            entry.kind = RoomFeedEntryKind::Topic;
            entry.author = event.author;
            entry.text = event.text;
            roomUiState_.topic = event.text;
            break;

        case net::FramedSocketTransport::RoomEvent::Kind::Join:
            entry.kind = RoomFeedEntryKind::Presence;
            entry.author = event.author;
            entry.text = "joined the room";
            entry.subdued = true;
            break;

        case net::FramedSocketTransport::RoomEvent::Kind::Part:
            entry.kind = RoomFeedEntryKind::Presence;
            entry.author = event.author;
            entry.text = "left the room";
            entry.subdued = true;
            break;

        case net::FramedSocketTransport::RoomEvent::Kind::System:
            entry.kind = isVoteSystemText(event.text) ? RoomFeedEntryKind::VoteSystem : RoomFeedEntryKind::GenericSystem;
            entry.author = event.author;
            entry.text = event.text;
            break;
    }

    roomUiState_.visibleFeed.push_back(std::move(entry));
    while (roomUiState_.visibleFeed.size() > kMaxRoomFeedEntries)
        roomUiState_.visibleFeed.erase(roomUiState_.visibleFeed.begin());

    auto applyVoteFeedback = [&](RoomVoteKind kind, RoomVoteUiState& voteState) {
        if (! voteState.pending || ! voteTextMatchesRequest(event.text, kind, voteState.requestedValue))
            return;

        voteState.pending = false;
        voteState.failed = voteTextLooksFailed(event.text);
        voteState.statusText = event.text;
    };

    if (event.kind == net::FramedSocketTransport::RoomEvent::Kind::System)
    {
        applyVoteFeedback(RoomVoteKind::Bpm, roomUiState_.bpmVote);
        applyVoteFeedback(RoomVoteKind::Bpi, roomUiState_.bpiVote);
    }
}

void FamaLamaJamAudioProcessor::clearRoomUiState(bool connected)
{
    std::scoped_lock lock(roomUiMutex_);
    roomUiState_ = {};
    roomUiState_.connected = connected;
}

void FamaLamaJamAudioProcessor::refreshPendingRoomVotesFromTiming()
{
    const auto currentBpm = serverBpmForUi_.load(std::memory_order_relaxed);
    const auto currentBpi = beatsPerIntervalForUi_.load(std::memory_order_relaxed);

    std::scoped_lock lock(roomUiMutex_);

    if (roomUiState_.bpmVote.pending && roomUiState_.bpmVote.requestedValue > 0
        && roomUiState_.bpmVote.requestedValue == currentBpm)
    {
        roomUiState_.bpmVote.pending = false;
        roomUiState_.bpmVote.failed = false;
        roomUiState_.bpmVote.statusText = "Room BPM updated";
    }

    if (roomUiState_.bpiVote.pending && roomUiState_.bpiVote.requestedValue > 0
        && roomUiState_.bpiVote.requestedValue == currentBpi)
    {
        roomUiState_.bpiVote.pending = false;
        roomUiState_.bpiVote.failed = false;
        roomUiState_.bpiVote.statusText = "Room BPI updated";
    }

    roomUiState_.connected = isSessionConnected();
}

void FamaLamaJamAudioProcessor::rememberSuccessfulServer()
{
    const auto settings = settingsStore_.getActiveSettings();

    {
        const std::scoped_lock lock(serverDiscoveryMutex_);
        app::session::rememberSuccessfulServer(
            rememberedServers_,
            app::session::RememberedServerEntry {
                .host = settings.serverHost,
                .port = settings.serverPort,
                .username = settings.username,
                .password = settings.password,
            },
            kMaxRememberedServers);
    }

    persistRememberedServers();
}

void FamaLamaJamAudioProcessor::ensureLocalMonitorMixerStrip()
{
    auto it = mixerStripsBySourceId_.find(kLocalMonitorSourceId);
    if (it == mixerStripsBySourceId_.end())
    {
        MixerStripSnapshot snapshot;
        snapshot.descriptor.kind = MixerStripKind::LocalMonitor;
        snapshot.descriptor.sourceId = kLocalMonitorSourceId;
        snapshot.descriptor.groupId = "local";
        snapshot.descriptor.userName = "Local";
        snapshot.descriptor.channelName = "Monitor";
        snapshot.descriptor.displayName = "Local Monitor";
        snapshot.descriptor.active = true;
        snapshot.descriptor.visible = true;
        snapshot.mix = normalizeMixState(makeUnityMixerStripMixState());
        snapshot.transmitState = transmitEnabled_ ? TransmitState::WarmingUp : TransmitState::Disabled;
        snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
        snapshot.statusText = makeLocalTransmitStatus(localChannelMode_,
                                                      snapshot.transmitState,
                                                      localVoiceTransitionPending_);

        it = mixerStripsBySourceId_.emplace(kLocalMonitorSourceId,
                                            MixerStripRuntimeState {
                                                .snapshot = std::move(snapshot),
                                                .lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex,
                                                .lastAudioIntervalIndex = authoritativeTiming_.intervalIndex,
                                            })
                 .first;
    }

    it->second.snapshot.descriptor.kind = MixerStripKind::LocalMonitor;
    it->second.snapshot.descriptor.sourceId = kLocalMonitorSourceId;
    it->second.snapshot.descriptor.groupId = "local";
    it->second.snapshot.descriptor.userName = "Local";
    it->second.snapshot.descriptor.channelName = "Monitor";
    it->second.snapshot.descriptor.displayName = "Local Monitor";
    it->second.snapshot.descriptor.active = true;
    it->second.snapshot.descriptor.visible = true;
    it->second.snapshot.descriptor.inactiveIntervals = 0;
    it->second.snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
    it->second.snapshot.unsupportedVoiceMode = false;
    it->second.lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex;
    it->second.lastAudioIntervalIndex = authoritativeTiming_.intervalIndex;
}

void FamaLamaJamAudioProcessor::syncRemoteMixerStrip(const net::FramedSocketTransport::RemoteSourceInfo& sourceInfo,
                                                     bool hasAudioActivity)
{
    if (sourceInfo.sourceId.empty())
        return;

    remoteReceiveDiagnosticsBySource_[sourceInfo.sourceId].channelFlags = sourceInfo.channelFlags;
    knownRemoteSourcesById_[sourceInfo.sourceId] = sourceInfo;

    auto it = mixerStripsBySourceId_.find(sourceInfo.sourceId);
    if (it == mixerStripsBySourceId_.end())
    {
        MixerStripSnapshot snapshot;
        snapshot.descriptor.kind = MixerStripKind::RemoteDelayed;
        snapshot.descriptor.sourceId = sourceInfo.sourceId;
        snapshot.descriptor.groupId = sourceInfo.groupId;
        snapshot.descriptor.userName = sourceInfo.userName;
        snapshot.descriptor.channelName = sourceInfo.channelName;
        snapshot.descriptor.displayName = sourceInfo.displayName;
        snapshot.descriptor.channelIndex = static_cast<int>(sourceInfo.channelIndex);
        snapshot.descriptor.active = true;
        snapshot.descriptor.visible = true;
        snapshot.mix = normalizeMixState(makeUnityMixerStripMixState());
        snapshot.voiceMode = isVoiceMode(sourceInfo.channelFlags);
        snapshot.unsupportedVoiceMode = false;
        snapshot.statusText = isVoiceMode(sourceInfo.channelFlags) ? "Voice chat: near realtime" : std::string {};

        it = mixerStripsBySourceId_.emplace(sourceInfo.sourceId,
                                            MixerStripRuntimeState {
                                                .snapshot = std::move(snapshot),
                                                .lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex,
                                                .lastAudioIntervalIndex = hasAudioActivity ? authoritativeTiming_.intervalIndex : 0u,
                                            })
                 .first;
    }

    auto& descriptor = it->second.snapshot.descriptor;
    descriptor.kind = MixerStripKind::RemoteDelayed;
    descriptor.sourceId = sourceInfo.sourceId;
    descriptor.groupId = sourceInfo.groupId;
    descriptor.userName = sourceInfo.userName;
    descriptor.channelName = sourceInfo.channelName;
    descriptor.displayName = sourceInfo.displayName;
    descriptor.channelIndex = static_cast<int>(sourceInfo.channelIndex);
    descriptor.active = true;
    descriptor.visible = true;
    descriptor.inactiveIntervals = 0;
    it->second.snapshot.voiceMode = isVoiceMode(sourceInfo.channelFlags);
    it->second.snapshot.unsupportedVoiceMode = false;
    it->second.snapshot.statusText = isVoiceMode(sourceInfo.channelFlags) ? "Voice chat: near realtime"
                                                                          : std::string {};
    it->second.lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex;
    if (hasAudioActivity)
        it->second.lastAudioIntervalIndex = authoritativeTiming_.intervalIndex;
}

void FamaLamaJamAudioProcessor::markRemoteMixerStripInactive(const std::string& sourceId)
{
    if (sourceId.empty())
        return;

    knownRemoteSourcesById_.erase(sourceId);
    remoteQueuedIntervalsBySource_.erase(sourceId);
    remoteVoiceChunksBySource_.erase(sourceId);
    nextRemoteTargetBoundaryBySource_.erase(sourceId);
    remoteActiveIntervalBySource_.erase(sourceId);
    lastRemoteActivationBoundaryBySource_.erase(sourceId);
    remoteReceiveDiagnosticsBySource_.erase(sourceId);

    if (auto it = mixerStripsBySourceId_.find(sourceId); it != mixerStripsBySourceId_.end())
    {
        auto& descriptor = it->second.snapshot.descriptor;
        descriptor.active = false;
        descriptor.visible = false;
        descriptor.inactiveIntervals = 0;
        it->second.snapshot.meter = {};
        it->second.lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex;
    }
}

void FamaLamaJamAudioProcessor::hideAllRemoteMixerStrips() noexcept
{
    for (auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        auto& descriptor = runtimeState.snapshot.descriptor;
        if (descriptor.kind == MixerStripKind::LocalMonitor)
            continue;

        descriptor.active = false;
        descriptor.visible = false;
        descriptor.inactiveIntervals = 0;
        runtimeState.snapshot.meter = {};
        runtimeState.lastPresenceIntervalIndex = authoritativeTiming_.intervalIndex;
    }
}

void FamaLamaJamAudioProcessor::updateRemoteMixerStripActivity()
{
    ensureLocalMonitorMixerStrip();

    if (! isSessionConnected() || ! authoritativeTiming_.hasTiming)
    {
        hideAllRemoteMixerStrips();
        return;
    }

    const auto currentIntervalIndex = authoritativeTiming_.intervalIndex;

    for (auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        auto& descriptor = runtimeState.snapshot.descriptor;

        if (descriptor.kind == MixerStripKind::LocalMonitor)
        {
            descriptor.active = true;
            descriptor.visible = true;
            descriptor.inactiveIntervals = 0;
            runtimeState.snapshot.voiceMode = isLocalVoiceMode(localChannelMode_);
            runtimeState.snapshot.statusText = makeLocalTransmitStatus(localChannelMode_,
                                                                       getTransmitState(),
                                                                       localVoiceTransitionPending_);
            runtimeState.lastPresenceIntervalIndex = currentIntervalIndex;
            runtimeState.lastAudioIntervalIndex = currentIntervalIndex;
            continue;
        }

        const bool isKnownRemoteSource = knownRemoteSourcesById_.find(sourceId) != knownRemoteSourcesById_.end();
        const bool hasActiveInterval = remoteActiveIntervalBySource_.find(sourceId) != remoteActiveIntervalBySource_.end();
        const bool hasQueuedInterval = remoteQueuedIntervalsBySource_.find(sourceId) != remoteQueuedIntervalsBySource_.end();
        const bool hasVoiceAudio = remoteVoiceChunksBySource_.find(sourceId) != remoteVoiceChunksBySource_.end();
        const bool hasAudioActivity = hasActiveInterval || hasQueuedInterval || hasVoiceAudio;
        const bool voiceMode = knownRemoteSourcesById_.find(sourceId) != knownRemoteSourcesById_.end()
            && isVoiceMode(knownRemoteSourcesById_.at(sourceId).channelFlags);

        if (hasAudioActivity)
        {
            descriptor.active = true;
            descriptor.visible = true;
            descriptor.inactiveIntervals = 0;
            runtimeState.snapshot.voiceMode = voiceMode;
            runtimeState.lastAudioIntervalIndex = currentIntervalIndex;
            runtimeState.snapshot.statusText = voiceMode ? "Voice chat: near realtime" : "Receiving interval audio";
            continue;
        }

        descriptor.active = false;
        runtimeState.snapshot.voiceMode = voiceMode;

        const auto referenceIntervalIndex = runtimeState.lastAudioIntervalIndex > 0
            ? runtimeState.lastAudioIntervalIndex
            : runtimeState.lastPresenceIntervalIndex;
        const auto inactiveIntervals = currentIntervalIndex >= referenceIntervalIndex
            ? currentIntervalIndex - referenceIntervalIndex
            : 0u;
        descriptor.inactiveIntervals = inactiveIntervals;

        if (isKnownRemoteSource)
        {
            descriptor.visible = true;
            runtimeState.snapshot.statusText = voiceMode ? "Voice chat: near realtime" : "In room, waiting for interval audio";
        }
        else
        {
            descriptor.visible = descriptor.visible && inactiveIntervals <= kInactiveStripRetentionIntervals;
            runtimeState.snapshot.statusText.clear();
        }
    }
}

void FamaLamaJamAudioProcessor::resetMixerStripMeters() noexcept
{
    ++cpuDiagnosticSnapshot_.meterResetCalls;
    for (auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        runtimeState.snapshot.meter = {};
    }
}

void FamaLamaJamAudioProcessor::attemptLiveConnect()
{
    const auto settings = settingsStore_.getActiveSettings();
    const auto protocolUsername = makeProtocolUsername(settings.username, settings.password);

    {
        const std::scoped_lock lock(liveAuthAttemptMutex_);
        lastLiveAuthAttempt_ = LiveAuthAttemptSnapshot {
            .settingsUsername = settings.username,
            .protocolUsername = protocolUsername,
            .effectiveUsername = {},
            .failureReason = {},
            .authenticated = false,
        };
    }

    if (settings.serverHost.empty() || settings.serverPort == 0)
    {
        const std::scoped_lock lock(liveAuthAttemptMutex_);
        lastLiveAuthAttempt_.failureReason = "invalid host/port";
        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::ConnectionFailed,
            .reason = "invalid host/port",
        });
        return;
    }

    auto socket = std::make_unique<juce::StreamingSocket>();
    const auto connected = socket->connect(settings.serverHost, static_cast<int>(settings.serverPort), kConnectTimeoutMs);

    if (! connected)
    {
        const auto failureReason = makeConnectionFailureReason(settings);
        const std::scoped_lock lock(liveAuthAttemptMutex_);
        lastLiveAuthAttempt_.failureReason = failureReason;
        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::ConnectionFailed,
            .reason = failureReason,
        });
        return;
    }

    if (experimentalStreamingEnabled_)
    {
        if (! framedTransport_.start(std::move(socket), protocolUsername, settings.password))
        {
            const std::scoped_lock lock(liveAuthAttemptMutex_);
            lastLiveAuthAttempt_.failureReason = "transport startup failed";
            handleConnectionEvent(app::session::ConnectionEvent {
                .type = app::session::ConnectionEventType::ConnectionFailed,
                .reason = "transport startup failed",
            });
            return;
        }

        std::string authFailure;
        std::string effectiveUser;

        if (! framedTransport_.waitForAuthentication(kConnectTimeoutMs, &authFailure, &effectiveUser))
        {
            if (authFailure.empty())
                authFailure = "authorization timeout";

            const std::scoped_lock lock(liveAuthAttemptMutex_);
            lastLiveAuthAttempt_.failureReason = authFailure;
            handleConnectionEvent(app::session::ConnectionEvent {
                .type = app::session::ConnectionEventType::ConnectionFailed,
                .reason = authFailure,
            });
            return;
        }

        if (effectiveUser.empty())
            effectiveUser = protocolUsername;

        {
            const std::scoped_lock lock(liveAuthAttemptMutex_);
            lastLiveAuthAttempt_.effectiveUsername = effectiveUser;
            lastLiveAuthAttempt_.failureReason.clear();
            lastLiveAuthAttempt_.authenticated = true;
        }

        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::Connected,
            .reason = "Connected as " + effectiveUser + " (NINJAM auth ok)",
        });

        return;
    }

    {
        const std::scoped_lock lock(liveSocketMutex_);
        liveSocket_ = std::move(socket);
    }

    handleConnectionEvent(app::session::ConnectionEvent {
        .type = app::session::ConnectionEventType::Connected,
        .reason = "Connected (socket only)",
    });
}

void FamaLamaJamAudioProcessor::closeLiveSocket()
{
    framedTransport_.stop();

    const std::scoped_lock lock(liveSocketMutex_);

    if (liveSocket_ == nullptr)
        return;

    liveSocket_->close();
    liveSocket_.reset();
}
} // namespace famalamajam::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    const auto env = juce::SystemStats::getEnvironmentVariable("FAMALAMAJAM_EXPERIMENTAL_STREAMING", juce::String())
                         .trim()
                         .toLowerCase();
    const bool explicitlyEnabled = env == "1" || env == "true" || env == "yes" || env == "on";
    const bool explicitlyDisabled = env == "0" || env == "false" || env == "no" || env == "off";
    const bool enableExperimental = explicitlyEnabled || (! explicitlyDisabled && env.isEmpty());

    return new famalamajam::plugin::FamaLamaJamAudioProcessor(true, enableExperimental);
}








































