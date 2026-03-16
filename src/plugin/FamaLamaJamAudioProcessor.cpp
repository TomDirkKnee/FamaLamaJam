#include "FamaLamaJamAudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

#include "audio/AudioBufferResampler.h"
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
constexpr int kPluginStateSchema = 1;
constexpr std::size_t kMaxLastErrorContextLength = 64;
constexpr int kConnectTimeoutMs = 3000;

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

[[nodiscard]] std::string makeProtocolUsername(const std::string& username)
{
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
constexpr int kMinBpm = 20;
constexpr int kMaxBpm = 400;
constexpr int kMinBpi = 1;
constexpr int kMaxBpi = 256;
constexpr float kMetronomeClickDurationMs = 18.0f;
constexpr float kMetronomeDownbeatFrequencyHz = 1760.0f;
constexpr float kMetronomeBeatFrequencyHz = 1320.0f;
constexpr float kMetronomeDownbeatGain = 0.18f;
constexpr float kMetronomeBeatGain = 0.12f;
constexpr std::uint64_t kInactiveStripRetentionIntervals = 2;
constexpr float kMinMixerGainDb = -60.0f;
constexpr float kMaxMixerGainDb = 12.0f;

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

void preparePendingRemoteInterval(FamaLamaJamAudioProcessor::RemotePendingInterval& pending,
                                  int numChannels,
                                  int intervalSamples,
                                  std::uint64_t targetIntervalIndex)
{
    pending.audio.setSize(juce::jmax(1, numChannels), juce::jmax(1, intervalSamples), false, true, true);
    pending.audio.clear();
    pending.writePosition = 0;
    pending.targetIntervalIndex = targetIntervalIndex;
}

void queueCompletedRemoteInterval(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    const std::string& sourceId,
    FamaLamaJamAudioProcessor::RemotePendingInterval& pending)
{
    if (pending.audio.getNumChannels() <= 0 || pending.audio.getNumSamples() <= 0)
        return;

    auto& queuedIntervals = queuedBySource[sourceId];
    queuedIntervals.push_back(FamaLamaJamAudioProcessor::RemoteQueuedInterval {
        .audio = std::move(pending.audio),
        .targetIntervalIndex = pending.targetIntervalIndex,
    });

    while (queuedIntervals.size() > kMaxQueuedIntervalsPerSource)
        queuedIntervals.pop_front();
}

void appendDecodedRemoteChunk(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    std::unordered_map<std::string, FamaLamaJamAudioProcessor::RemotePendingInterval>& pendingBySource,
    const FamaLamaJamAudioProcessor::AuthoritativeTimingState& timing,
    const std::string& sourceId,
    const juce::AudioBuffer<float>& decoded)
{
    if (! timing.hasTiming || timing.activeIntervalSamples <= 0)
        return;

    if (decoded.getNumChannels() <= 0 || decoded.getNumSamples() <= 0)
        return;

    const auto currentTargetBoundary = timing.intervalIndex + 2;
    auto& pending = pendingBySource[sourceId];

    if (pending.targetIntervalIndex < currentTargetBoundary
        || pending.audio.getNumSamples() != timing.activeIntervalSamples
        || pending.audio.getNumChannels() != decoded.getNumChannels())
    {
        preparePendingRemoteInterval(pending,
                                     decoded.getNumChannels(),
                                     timing.activeIntervalSamples,
                                     currentTargetBoundary);
    }

    int sourceOffset = 0;
    while (sourceOffset < decoded.getNumSamples())
    {
        if (pending.writePosition >= timing.activeIntervalSamples)
        {
            const auto nextTargetBoundary = pending.targetIntervalIndex + 1;
            queueCompletedRemoteInterval(queuedBySource, sourceId, pending);
            preparePendingRemoteInterval(pending,
                                         decoded.getNumChannels(),
                                         timing.activeIntervalSamples,
                                         nextTargetBoundary);
        }

        const auto writableSamples = juce::jmin(decoded.getNumSamples() - sourceOffset,
                                                timing.activeIntervalSamples - pending.writePosition);

        for (int channel = 0; channel < decoded.getNumChannels(); ++channel)
        {
            pending.audio.copyFrom(channel,
                                   pending.writePosition,
                                   decoded,
                                   channel,
                                   sourceOffset,
                                   writableSamples);
        }

        pending.writePosition += writableSamples;
        sourceOffset += writableSamples;
    }

    if (pending.writePosition >= timing.activeIntervalSamples)
    {
        queueCompletedRemoteInterval(queuedBySource, sourceId, pending);
        pendingBySource.erase(sourceId);
    }
}

void activateRemoteIntervalsForBoundary(
    std::unordered_map<std::string, std::deque<FamaLamaJamAudioProcessor::RemoteQueuedInterval>>& queuedBySource,
    std::unordered_map<std::string, juce::AudioBuffer<float>>& activeBySource,
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
            queuedIntervals.pop_front();
        }

        if (queuedIntervals.empty())
            sourceIt = queuedBySource.erase(sourceIt);
        else
            ++sourceIt;
    }
}

void discardLatePendingIntervals(
    std::unordered_map<std::string, FamaLamaJamAudioProcessor::RemotePendingInterval>& pendingBySource,
    std::uint64_t boundaryIntervalIndex)
{
    for (auto sourceIt = pendingBySource.begin(); sourceIt != pendingBySource.end();)
    {
        if (sourceIt->second.targetIntervalIndex <= boundaryIntervalIndex)
            sourceIt = pendingBySource.erase(sourceIt);
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
} // namespace
FamaLamaJamAudioProcessor::FamaLamaJamAudioProcessor(bool enableLiveTransport, bool enableExperimentalStreaming)
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , settingsController_(settingsStore_)
    , liveTransportEnabled_(enableLiveTransport)
    , experimentalStreamingEnabled_(enableExperimentalStreaming)
{
    ensureLocalMonitorMixerStrip();
}

FamaLamaJamAudioProcessor::~FamaLamaJamAudioProcessor()
{
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
        codecStreamBridge_.stop();
        if (isSessionConnected())
            codecStreamBridge_.start();

        localUploadIntervalBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
        lastDecodedSamples_.store(0, std::memory_order_relaxed);
        remoteQueuedIntervalsBySource_.clear();
        remoteActiveIntervalBySource_.clear();
        remotePendingIntervalsBySource_.clear();
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

    codecStreamBridge_.stop();
    localUploadIntervalBuffer_.setSize(0, 0);
    localUploadIntervalWritePosition_ = 0;
    lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
    lastDecodedSamples_.store(0, std::memory_order_relaxed);
    remoteQueuedIntervalsBySource_.clear();
    remoteActiveIntervalBySource_.clear();
    remotePendingIntervalsBySource_.clear();
    clearAuthoritativeTiming();
    resetMixerStripMeters();
    updateRemoteMixerStripActivity();
}

bool FamaLamaJamAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void FamaLamaJamAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    ensureLocalMonitorMixerStrip();
    resetMixerStripMeters();

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto clearTransportUi = [this]() {
        currentBeatForUi_.store(0, std::memory_order_relaxed);
        intervalProgressForUi_.store(0.0f, std::memory_order_relaxed);
        intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);
    };

    auto applyLocalMonitorToOutput = [this, &buffer]() {
        auto& localStrip = mixerStripsBySourceId_[kLocalMonitorSourceId];
        applyMixToBuffer(buffer, localStrip.snapshot.mix, localStrip.snapshot.meter);
        localStrip.snapshot.descriptor.active = true;
        localStrip.snapshot.descriptor.visible = true;
        localStrip.snapshot.descriptor.inactiveIntervals = 0;
        localStrip.lastSeenIntervalIndex = authoritativeTiming_.intervalIndex;
    };

    if (! isSessionConnected() || currentSampleRate_ <= 0.0)
    {
        applyLocalMonitorToOutput();
        clearTransportUi();
        updateRemoteMixerStripActivity();
        return;
    }

    if (experimentalStreamingEnabled_ && ! framedTransport_.isRunning())
    {
        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::ConnectionLostRetryable,
            .reason = "transport disconnected",
        });
        applyLocalMonitorToOutput();
        clearTransportUi();
        updateRemoteMixerStripActivity();
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
        if (authoritativeTiming_.hasTiming && authoritativeTiming_.activeIntervalSamples > 0)
        {
            if (localUploadIntervalBuffer_.getNumChannels() != buffer.getNumChannels()
                || localUploadIntervalBuffer_.getNumSamples() != authoritativeTiming_.activeIntervalSamples)
            {
                localUploadIntervalBuffer_.setSize(buffer.getNumChannels(),
                                                   authoritativeTiming_.activeIntervalSamples,
                                                   false,
                                                   true,
                                                   true);
                localUploadIntervalBuffer_.clear();
                localUploadIntervalWritePosition_ = 0;
            }

            int sourceOffset = 0;
            while (sourceOffset < buffer.getNumSamples())
            {
                const auto writableSamples = juce::jmin(buffer.getNumSamples() - sourceOffset,
                                                        authoritativeTiming_.activeIntervalSamples
                                                            - localUploadIntervalWritePosition_);

                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    localUploadIntervalBuffer_.copyFrom(channel,
                                                        localUploadIntervalWritePosition_,
                                                        buffer,
                                                        channel,
                                                        sourceOffset,
                                                        writableSamples);
                }

                localUploadIntervalWritePosition_ += writableSamples;
                sourceOffset += writableSamples;

                if (localUploadIntervalWritePosition_ >= authoritativeTiming_.activeIntervalSamples)
                {
                    codecStreamBridge_.submitInput(localUploadIntervalBuffer_, currentSampleRate_);
                    localUploadIntervalBuffer_.clear();
                    localUploadIntervalWritePosition_ = 0;
                }
            }
        }
    }
    else
    {
        codecStreamBridge_.submitInput(buffer, currentSampleRate_);
    }

    applyLocalMonitorToOutput();

    juce::MemoryBlock encoded;
    if (codecStreamBridge_.popEncoded(encoded))
    {
        lastCodecPayloadBytes_.store(encoded.getSize(), std::memory_order_relaxed);

        if (experimentalStreamingEnabled_)
            framedTransport_.enqueueOutbound(encoded);
    }

    if (experimentalStreamingEnabled_)
    {
        net::FramedSocketTransport::RemoteSourceActivityUpdate sourceActivityUpdate;
        int sourceUpdatesConsumed = 0;

        while (sourceUpdatesConsumed < 16 && framedTransport_.popRemoteSourceActivityUpdate(sourceActivityUpdate))
        {
            if (sourceActivityUpdate.active)
                syncRemoteMixerStrip(sourceActivityUpdate.sourceInfo);
            else
                markRemoteMixerStripInactive(sourceActivityUpdate.sourceInfo.sourceId);

            ++sourceUpdatesConsumed;
        }

        net::FramedSocketTransport::InboundFrame inboundFrame;
        int framesConsumed = 0;

        while (framesConsumed < 8 && framedTransport_.popInboundFrame(inboundFrame))
        {
            syncRemoteMixerStrip(inboundFrame.sourceInfo);
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
        appendDecodedRemoteChunk(remoteQueuedIntervalsBySource_,
                                 remotePendingIntervalsBySource_,
                                 authoritativeTiming_,
                                 sourceId,
                                 decodedAtHostRate);

        ++decodedFrames;
    }

    if (authoritativeTiming_.hasTiming && authoritativeTiming_.activeIntervalSamples > 0)
    {
        const auto outputChannels = buffer.getNumChannels();
        const auto outputSamples = buffer.getNumSamples();
        const auto beatsPerInterval = juce::jmax(1, authoritativeTiming_.activeBpi);
        const auto clickDurationSamples = juce::jmax(1,
                                                     static_cast<int>((kMetronomeClickDurationMs / 1000.0f)
                                                                      * static_cast<float>(currentSampleRate_)));
        const bool renderMetronome = metronomeEnabled_.load(std::memory_order_relaxed)
            && authoritativeTiming_.activeBeatSamples > 0;

        hasServerTimingForUi_.store(true, std::memory_order_relaxed);
        serverBpmForUi_.store(authoritativeTiming_.activeBpm, std::memory_order_relaxed);
        beatsPerIntervalForUi_.store(authoritativeTiming_.activeBpi, std::memory_order_relaxed);

        for (int sample = 0; sample < outputSamples; ++sample)
        {
            for (auto& sourceEntry : remoteActiveIntervalBySource_)
            {
                const auto stripIt = mixerStripsBySourceId_.find(sourceEntry.first);
                if (stripIt == mixerStripsBySourceId_.end() || stripIt->second.snapshot.mix.muted)
                    continue;

                const auto& sourceBuffer = sourceEntry.second;
                if (authoritativeTiming_.samplesIntoInterval >= sourceBuffer.getNumSamples())
                    continue;

                const auto sourceChannels = sourceBuffer.getNumChannels();
                if (sourceChannels <= 0)
                    continue;

                float panLeft = 1.0f;
                float panRight = 1.0f;
                float panOther = 1.0f;
                computePanGains(stripIt->second.snapshot.mix.pan, panLeft, panRight, panOther);
                const auto gainLinear = juce::Decibels::decibelsToGain(stripIt->second.snapshot.mix.gainDb);

                for (int channel = 0; channel < outputChannels; ++channel)
                {
                    const auto sourceChannel = sourceChannels == 1 ? 0 : juce::jmin(channel, sourceChannels - 1);
                    float channelGain = gainLinear * panOther;
                    if (outputChannels > 1)
                    {
                        if (channel == 0)
                            channelGain = gainLinear * panLeft;
                        else if (channel == 1)
                            channelGain = gainLinear * panRight;
                    }

                    const auto mixedSample = sourceBuffer.getSample(sourceChannel,
                                                                    authoritativeTiming_.samplesIntoInterval)
                        * channelGain;
                    buffer.addSample(channel, sample, mixedSample);

                    const auto magnitude = std::abs(mixedSample);
                    if (channel == 0)
                        stripIt->second.snapshot.meter.left
                            = juce::jmax(stripIt->second.snapshot.meter.left, magnitude);
                    else if (channel == 1)
                        stripIt->second.snapshot.meter.right
                            = juce::jmax(stripIt->second.snapshot.meter.right, magnitude);
                }
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
                const auto clickSample = std::sin(metronomeClickPhase_) * metronomeClickGain_ * envelope;

                for (int channel = 0; channel < outputChannels; ++channel)
                    buffer.addSample(channel, sample, clickSample);

                metronomeClickPhase_ += metronomeClickPhaseIncrement_;
                --metronomeClickRemainingSamples_;
            }

            ++authoritativeTiming_.samplesIntoInterval;
            if (authoritativeTiming_.samplesIntoInterval >= authoritativeTiming_.activeIntervalSamples)
            {
                authoritativeTiming_.samplesIntoInterval = 0;
                ++authoritativeTiming_.intervalIndex;
                intervalIndexForUi_.store(authoritativeTiming_.intervalIndex, std::memory_order_relaxed);

                if (authoritativeTiming_.hasPendingTiming)
                {
                    activateTimingConfig(authoritativeTiming_,
                                         authoritativeTiming_.pendingBpm,
                                         authoritativeTiming_.pendingBpi,
                                         authoritativeTiming_.pendingIntervalSamples,
                                         authoritativeTiming_.pendingBeatSamples);
                    localUploadIntervalBuffer_.setSize(0, 0);
                    localUploadIntervalWritePosition_ = 0;
                    remoteQueuedIntervalsBySource_.clear();
                    remoteActiveIntervalBySource_.clear();
                    remotePendingIntervalsBySource_.clear();
                }

                discardLatePendingIntervals(remotePendingIntervalsBySource_, authoritativeTiming_.intervalIndex);
                activateRemoteIntervalsForBoundary(remoteQueuedIntervalsBySource_,
                                                  remoteActiveIntervalBySource_,
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

    updateRemoteMixerStripActivity();
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
                .meterLeft = snapshot.meter.left,
                .meterRight = snapshot.meter.right,
                .active = snapshot.descriptor.active,
                .visible = snapshot.descriptor.visible,
            });
        }
        return strips;
    };
    auto mixerStripSetter = [this](const std::string& sourceId, float gainDb, float pan, bool muted) {
        return setMixerStripMixState(sourceId, gainDb, pan, muted);
    };
    auto metronomeGetter = [this]() { return isMetronomeEnabled(); };
    auto metronomeSetter = [this](bool enabled) { setMetronomeEnabled(enabled); };

    return new FamaLamaJamAudioProcessorEditor(*this,
                                               std::move(settingsGetter),
                                               std::move(apply),
                                               std::move(lifecycleGetter),
                                               std::move(connect),
                                               std::move(disconnect),
                                               std::move(transportUiGetter),
                                               std::move(mixerStripsGetter),
                                               std::move(mixerStripSetter),
                                               std::move(metronomeGetter),
                                               std::move(metronomeSetter));
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

    const auto lastErrorContext = shortenLastErrorContext(lifecycleController_.getSnapshot().lastError);
    if (! lastErrorContext.empty())
        stateTree.setProperty(kPluginStateLastErrorContext, juce::String(lastErrorContext), nullptr);

    stateTree.setProperty(kPluginStateMetronomeEnabled, metronomeEnabled_.load(std::memory_order_relaxed), nullptr);

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
    std::vector<MixerStripSnapshot> restoredMixerStrips;

    const auto stateTree = parseValueTree(data, sizeInBytes);
    const bool hasWrappedState = stateTree.isValid() && stateTree.hasType(kPluginStateType)
        && static_cast<int>(stateTree.getProperty(kPluginStateSchemaVersion, -1)) == kPluginStateSchema;

    if (hasWrappedState)
    {
        if (stateTree.getNumChildren() > 0)
        {
            juce::MemoryOutputStream settingsStream(settingsPayload, false);
            stateTree.getChild(0).writeToStream(settingsStream);
        }

        restoredLastErrorContext = shortenLastErrorContext(
            stateTree.getProperty(kPluginStateLastErrorContext, juce::String()).toString().toStdString());
        restoredMetronomeEnabled = static_cast<bool>(stateTree.getProperty(kPluginStateMetronomeEnabled, false));

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

    closeLiveSocket();
    applyLifecycleTransition(lifecycleController_.resetToIdle());
    mixerStripsBySourceId_.clear();
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
        runtimeState.lastSeenIntervalIndex = 0;
    }

    ensureLocalMonitorMixerStrip();
    updateRemoteMixerStripActivity();
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

void FamaLamaJamAudioProcessor::handleConnectionEvent(const app::session::ConnectionEvent& event)
{
    const auto transition = lifecycleController_.handleEvent(event);
    applyLifecycleTransition(transition);
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
    return remotePendingIntervalsBySource_.size();
}

FamaLamaJamAudioProcessor::TransportUiState FamaLamaJamAudioProcessor::getTransportUiState() const noexcept
{
    const auto snapshot = lifecycleController_.getSnapshot();
    const auto hasServerTiming = hasServerTimingForUi_.load(std::memory_order_relaxed);
    const auto syncHealth = computeSyncHealth(snapshot, hasServerTiming);

    return TransportUiState {
        .connected = snapshot.isConnected(),
        .hasServerTiming = hasServerTiming,
        .syncHealth = syncHealth,
        .metronomeAvailable = syncHealth == SyncHealth::Healthy,
        .beatsPerMinute = serverBpmForUi_.load(std::memory_order_relaxed),
        .beatsPerInterval = beatsPerIntervalForUi_.load(std::memory_order_relaxed),
        .currentBeat = currentBeatForUi_.load(std::memory_order_relaxed),
        .intervalProgress = intervalProgressForUi_.load(std::memory_order_relaxed),
        .intervalIndex = intervalIndexForUi_.load(std::memory_order_relaxed),
    };
}

bool FamaLamaJamAudioProcessor::isMetronomeEnabled() const noexcept
{
    return metronomeEnabled_.load(std::memory_order_relaxed);
}

void FamaLamaJamAudioProcessor::setMetronomeEnabled(bool enabled) noexcept
{
    metronomeEnabled_.store(enabled, std::memory_order_relaxed);
}

std::vector<FamaLamaJamAudioProcessor::MixerStripSnapshot> FamaLamaJamAudioProcessor::getMixerStripSnapshots() const
{
    std::vector<MixerStripSnapshot> snapshots;
    snapshots.reserve(mixerStripsBySourceId_.size());

    for (const auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        snapshots.push_back(runtimeState.snapshot);
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
        it->second.snapshot.mix = normalizeMixState(MixerStripMixState {
            .gainDb = gainDb,
            .pan = pan,
            .muted = muted,
        });
        return true;
    }

    return false;
}

void FamaLamaJamAudioProcessor::timerCallback()
{
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
    }
    else
    {
        codecStreamBridge_.stop();
        localUploadIntervalBuffer_.setSize(0, 0);
        localUploadIntervalWritePosition_ = 0;
        lastCodecPayloadBytes_.store(0, std::memory_order_relaxed);
        lastDecodedSamples_.store(0, std::memory_order_relaxed);
        remoteQueuedIntervalsBySource_.clear();
        remoteActiveIntervalBySource_.clear();
        remotePendingIntervalsBySource_.clear();
        clearAuthoritativeTiming();
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
    authoritativeTiming_ = {};
    localUploadIntervalBuffer_.setSize(0, 0);
    localUploadIntervalWritePosition_ = 0;
    hasServerTimingForUi_.store(false, std::memory_order_relaxed);
    serverBpmForUi_.store(0, std::memory_order_relaxed);
    beatsPerIntervalForUi_.store(0, std::memory_order_relaxed);
    currentBeatForUi_.store(0, std::memory_order_relaxed);
    intervalProgressForUi_.store(0.0f, std::memory_order_relaxed);
    intervalIndexForUi_.store(0, std::memory_order_relaxed);
    metronomeClickRemainingSamples_ = 0;
}

void FamaLamaJamAudioProcessor::ensureLocalMonitorMixerStrip()
{
    auto it = mixerStripsBySourceId_.find(kLocalMonitorSourceId);
    if (it == mixerStripsBySourceId_.end())
    {
        const auto defaults = settingsStore_.getActiveSettings();
        MixerStripSnapshot snapshot;
        snapshot.descriptor.kind = MixerStripKind::LocalMonitor;
        snapshot.descriptor.sourceId = kLocalMonitorSourceId;
        snapshot.descriptor.groupId = "local";
        snapshot.descriptor.userName = "Local";
        snapshot.descriptor.channelName = "Monitor";
        snapshot.descriptor.displayName = "Local Monitor";
        snapshot.descriptor.active = true;
        snapshot.descriptor.visible = true;
        snapshot.mix = normalizeMixState(MixerStripMixState {
            .gainDb = defaults.defaultChannelGainDb,
            .pan = defaults.defaultChannelPan,
            .muted = defaults.defaultChannelMuted,
        });

        it = mixerStripsBySourceId_.emplace(kLocalMonitorSourceId,
                                            MixerStripRuntimeState {
                                                .snapshot = std::move(snapshot),
                                                .lastSeenIntervalIndex = authoritativeTiming_.intervalIndex,
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
}

void FamaLamaJamAudioProcessor::syncRemoteMixerStrip(const net::FramedSocketTransport::RemoteSourceInfo& sourceInfo)
{
    if (sourceInfo.sourceId.empty())
        return;

    auto it = mixerStripsBySourceId_.find(sourceInfo.sourceId);
    if (it == mixerStripsBySourceId_.end())
    {
        const auto defaults = settingsStore_.getActiveSettings();
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
        snapshot.mix = normalizeMixState(MixerStripMixState {
            .gainDb = defaults.defaultChannelGainDb,
            .pan = defaults.defaultChannelPan,
            .muted = defaults.defaultChannelMuted,
        });

        it = mixerStripsBySourceId_.emplace(sourceInfo.sourceId,
                                            MixerStripRuntimeState {
                                                .snapshot = std::move(snapshot),
                                                .lastSeenIntervalIndex = authoritativeTiming_.intervalIndex,
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
    it->second.lastSeenIntervalIndex = authoritativeTiming_.intervalIndex;
}

void FamaLamaJamAudioProcessor::markRemoteMixerStripInactive(const std::string& sourceId)
{
    if (sourceId.empty())
        return;

    remoteQueuedIntervalsBySource_.erase(sourceId);
    remoteActiveIntervalBySource_.erase(sourceId);
    remotePendingIntervalsBySource_.erase(sourceId);

    if (auto it = mixerStripsBySourceId_.find(sourceId); it != mixerStripsBySourceId_.end())
    {
        auto& descriptor = it->second.snapshot.descriptor;
        descriptor.active = false;
        descriptor.visible = false;
        descriptor.inactiveIntervals = 0;
        it->second.snapshot.meter = {};
        it->second.lastSeenIntervalIndex = authoritativeTiming_.intervalIndex;
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
        runtimeState.lastSeenIntervalIndex = authoritativeTiming_.intervalIndex;
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
            runtimeState.lastSeenIntervalIndex = currentIntervalIndex;
            continue;
        }

        const bool hasActiveInterval = remoteActiveIntervalBySource_.find(sourceId) != remoteActiveIntervalBySource_.end();
        const bool hasQueuedInterval = remoteQueuedIntervalsBySource_.find(sourceId) != remoteQueuedIntervalsBySource_.end();
        const bool hasPendingInterval = remotePendingIntervalsBySource_.find(sourceId) != remotePendingIntervalsBySource_.end();
        const bool isActiveNow = hasActiveInterval || hasQueuedInterval || hasPendingInterval;

        if (isActiveNow)
        {
            descriptor.active = true;
            descriptor.visible = true;
            descriptor.inactiveIntervals = 0;
            runtimeState.lastSeenIntervalIndex = currentIntervalIndex;
            continue;
        }

        descriptor.active = false;

        const auto inactiveIntervals = currentIntervalIndex >= runtimeState.lastSeenIntervalIndex
            ? currentIntervalIndex - runtimeState.lastSeenIntervalIndex
            : 0u;
        descriptor.inactiveIntervals = inactiveIntervals;
        descriptor.visible = descriptor.visible && inactiveIntervals <= kInactiveStripRetentionIntervals;
    }
}

void FamaLamaJamAudioProcessor::resetMixerStripMeters() noexcept
{
    for (auto& [sourceId, runtimeState] : mixerStripsBySourceId_)
    {
        (void) sourceId;
        runtimeState.snapshot.meter = {};
    }
}

void FamaLamaJamAudioProcessor::attemptLiveConnect()
{
    const auto settings = settingsStore_.getActiveSettings();

    if (settings.serverHost.empty() || settings.serverPort == 0)
    {
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
        handleConnectionEvent(app::session::ConnectionEvent {
            .type = app::session::ConnectionEventType::ConnectionFailed,
            .reason = makeConnectionFailureReason(settings),
        });
        return;
    }

    if (experimentalStreamingEnabled_)
    {
        const auto protocolUsername = makeProtocolUsername(settings.username);

        if (! framedTransport_.start(std::move(socket), protocolUsername, ""))
        {
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

            handleConnectionEvent(app::session::ConnectionEvent {
                .type = app::session::ConnectionEventType::ConnectionFailed,
                .reason = authFailure,
            });
            return;
        }

        if (effectiveUser.empty())
            effectiveUser = protocolUsername;

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








































