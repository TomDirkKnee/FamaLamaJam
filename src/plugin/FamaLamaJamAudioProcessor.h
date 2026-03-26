#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "app/session/ConnectionLifecycleController.h"
#include "app/session/ServerDiscoveryModel.h"
#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"
#include "audio/CodecStreamBridge.h"
#include "infra/net/PublicServerDiscoveryClient.h"
#include "net/FramedSocketTransport.h"

namespace famalamajam::infra::state
{
class RememberedServerStore;
}

namespace famalamajam::plugin
{
class FamaLamaJamAudioProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr const char* kLocalMonitorSourceId = "local-monitor";

    enum class SyncHealth
    {
        Disconnected,
        WaitingForTiming,
        Healthy,
        Reconnecting,
        TimingLost,
    };

    struct TransportUiState
    {
        bool connected { false };
        bool hasServerTiming { false };
        SyncHealth syncHealth { SyncHealth::Disconnected };
        bool metronomeAvailable { false };
        int beatsPerMinute { 0 };
        int beatsPerInterval { 0 };
        int currentBeat { 0 };
        float intervalProgress { 0.0f };
        std::uint64_t intervalIndex { 0 };
    };

    enum class HostSyncAssistBlockReason
    {
        None,
        MissingServerTiming,
        MissingHostTempo,
        HostTempoMismatch,
    };

    enum class HostSyncAssistFailureReason
    {
        None,
        TimingLost,
        MissingHostMusicalPosition,
    };

    struct HostTransportSnapshot
    {
        bool available { false };
        bool playing { false };
        bool hasTempo { false };
        double tempoBpm { 0.0 };
        bool hasPpqPosition { false };
        double ppqPosition { 0.0 };
        bool hasBarStartPpqPosition { false };
        double ppqPositionOfLastBarStart { 0.0 };
        bool hasTimeSignature { false };
        int timeSigNumerator { 0 };
        int timeSigDenominator { 0 };
    };

    struct HostSyncAssistUiState
    {
        bool armable { false };
        bool armed { false };
        bool waitingForHost { false };
        bool blocked { false };
        bool failed { false };
        HostSyncAssistBlockReason blockReason { HostSyncAssistBlockReason::None };
        HostSyncAssistFailureReason failureReason { HostSyncAssistFailureReason::None };
        int targetBeatsPerMinute { 0 };
        int targetBeatsPerInterval { 0 };
        HostTransportSnapshot hostTransport;
    };

    enum class RoomVoteKind
    {
        Bpm,
        Bpi,
    };

    enum class RoomFeedEntryKind
    {
        Chat,
        Topic,
        Presence,
        VoteSystem,
        GenericSystem,
    };

    struct RoomVoteUiState
    {
        bool pending { false };
        bool failed { false };
        int requestedValue { 0 };
        std::string statusText;
    };

    struct RoomFeedEntry
    {
        RoomFeedEntryKind kind { RoomFeedEntryKind::Chat };
        std::string author;
        std::string text;
        bool subdued { false };
    };

    struct RoomUiState
    {
        bool connected { false };
        std::string topic;
        std::vector<RoomFeedEntry> visibleFeed;
        RoomVoteUiState bpmVote;
        RoomVoteUiState bpiVote;
    };

    struct ServerDiscoveryEntry
    {
        enum class Source
        {
            Remembered,
            Public,
        };

        Source source { Source::Remembered };
        std::string label;
        std::string host;
        std::uint16_t port { 0 };
        std::string username;
        std::string password;
        int connectedUsers { -1 };
        int maxUsers { -1 };
        bool stale { false };
    };

    struct ServerDiscoveryUiState
    {
        bool fetchInProgress { false };
        bool hasStalePublicData { false };
        std::string statusText;
        std::vector<ServerDiscoveryEntry> combinedEntries;
    };

    enum class MixerStripKind
    {
        LocalMonitor,
        RemoteDelayed,
    };

    enum class TransmitState
    {
        Disabled,
        WarmingUp,
        Active,
    };

    struct MixerStripMixState
    {
        float gainDb { 0.0f };
        float pan { 0.0f };
        bool muted { false };
        bool soloed { false };
    };

    struct MixerStripMeter
    {
        float left { 0.0f };
        float right { 0.0f };
    };

    struct MixerStripDescriptor
    {
        MixerStripKind kind { MixerStripKind::RemoteDelayed };
        std::string sourceId;
        std::string groupId;
        std::string userName;
        std::string channelName;
        std::string displayName;
        int channelIndex { -1 };
        bool active { false };
        bool visible { false };
        std::uint64_t inactiveIntervals { 0 };
    };

    struct MixerStripSnapshot
    {
        MixerStripDescriptor descriptor;
        MixerStripMixState mix;
        MixerStripMeter meter;
        TransmitState transmitState { TransmitState::Disabled };
        bool unsupportedVoiceMode { false };
        std::string statusText;
    };

    struct CpuDiagnosticSnapshot
    {
        std::uint64_t processBlockCalls { 0 };
        std::uint64_t meterResetCalls { 0 };
        std::uint64_t remoteMixSourceVisits { 0 };
        std::uint64_t remoteMixChannelWrites { 0 };
        std::uint64_t remoteFramesDecoded { 0 };
    };

    struct RemoteReceiveDiagnosticRuntime
    {
        std::size_t lastEncodedBytes { 0 };
        int lastDecodedSamples { 0 };
        double lastDecodedSampleRate { 0.0 };
        std::uint8_t channelFlags { 0 };
        int lastCopiedSamples { 0 };
        std::uint64_t lastInboundIntervalIndex { 0 };
        std::uint64_t lastQueuedBoundary { 0 };
        std::uint64_t lastDropTargetBoundary { 0 };
        std::uint64_t lastDropCurrentBoundary { 0 };
        std::uint64_t lateDrops { 0 };
    };

    struct LiveAuthAttemptSnapshot
    {
        std::string settingsUsername;
        std::string protocolUsername;
        std::string effectiveUsername;
        std::string failureReason;
        bool authenticated { false };
    };

    explicit FamaLamaJamAudioProcessor(bool enableLiveTransport = false,
                                       bool enableExperimentalStreaming = false);
    explicit FamaLamaJamAudioProcessor(
        std::unique_ptr<famalamajam::infra::state::RememberedServerStore> rememberedServerStore,
        bool enableLiveTransport = false,
        bool enableExperimentalStreaming = false);
    ~FamaLamaJamAudioProcessor() override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool applySettingsFromUi(const app::session::SessionSettings& candidate,
                             app::session::SessionSettingsValidationResult* validation = nullptr);

    bool requestConnect();
    bool requestDisconnect();
    bool toggleTransmitEnabled();
    void handleConnectionEvent(const app::session::ConnectionEvent& event);

    bool triggerScheduledReconnectForTesting();
    [[nodiscard]] int getScheduledReconnectDelayMs() const noexcept;

    app::session::SessionSettings getActiveSettings() const;
    app::session::ConnectionLifecycleSnapshot getLifecycleSnapshot() const;
    std::string getLastStatusMessage() const;
    bool isSessionConnected() const noexcept;
    [[nodiscard]] std::size_t getLastCodecPayloadBytesForTesting() const noexcept;
    [[nodiscard]] int getLastDecodedSamplesForTesting() const noexcept;
    [[nodiscard]] bool isExperimentalStreamingEnabledForTesting() const noexcept;
    [[nodiscard]] LiveAuthAttemptSnapshot getLastLiveAuthAttemptForTesting() const;
    [[nodiscard]] std::size_t getTransportSentFramesForTesting() const noexcept;
    [[nodiscard]] std::size_t getTransportReceivedFramesForTesting() const noexcept;
    [[nodiscard]] std::size_t getActiveRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] bool isRemoteSourceActiveForTesting(const std::string& sourceId) const;
    [[nodiscard]] std::vector<std::string> getActiveRemoteSourceIdsForTesting() const;
    [[nodiscard]] std::size_t getQueuedRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] std::size_t getPendingRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] std::uint64_t getLastRemoteActivationBoundaryForTesting(const std::string& sourceId) const noexcept;
    [[nodiscard]] float getActiveRemoteIntervalAverageForTesting(const std::string& sourceId) const noexcept;
    [[nodiscard]] std::string getRemoteReceiveDiagnosticsText() const;
    void injectDecodedRemoteIntervalForTesting(const std::string& sourceId,
                                               const juce::AudioBuffer<float>& audio,
                                               double sampleRate);
    [[nodiscard]] CpuDiagnosticSnapshot getCpuDiagnosticSnapshotForTesting() const noexcept;
    void resetCpuDiagnosticSnapshotForTesting() noexcept;
    [[nodiscard]] TransportUiState getTransportUiState() const noexcept;
    [[nodiscard]] HostSyncAssistUiState getHostSyncAssistUiState() const noexcept;
    [[nodiscard]] RoomUiState getRoomUiState() const;
    [[nodiscard]] ServerDiscoveryUiState getServerDiscoveryUiState() const;
    bool requestPublicServerDiscoveryRefresh(bool manual);
    bool toggleHostSyncAssistArm();
    bool sendRoomChatMessage(std::string text);
    bool submitRoomVote(RoomVoteKind kind, int value);
    [[nodiscard]] bool isMetronomeEnabled() const noexcept;
    void setMetronomeEnabled(bool enabled) noexcept;
    [[nodiscard]] float getMetronomeGainDb() const noexcept;
    void setMetronomeGainDb(float gainDb) noexcept;
    [[nodiscard]] float getMasterOutputGainDb() const noexcept;
    void setMasterOutputGainDb(float gainDb) noexcept;
    [[nodiscard]] TransmitState getTransmitState() const noexcept;
    [[nodiscard]] std::vector<MixerStripSnapshot> getMixerStripSnapshots() const;
    [[nodiscard]] bool getMixerStripSnapshot(const std::string& sourceId, MixerStripSnapshot& snapshot) const;
    bool setMixerStripMixState(const std::string& sourceId, float gainDb, float pan, bool muted);
    bool setMixerStripSoloState(const std::string& sourceId, bool soloed);
    void setPublicServerDiscoveryClientForTesting(std::unique_ptr<infra::net::PublicServerDiscoveryClient> client);
    bool requestPublicServerDiscoveryRefreshForTesting(bool manual);

    struct AuthoritativeTimingState
    {
        bool hasTiming { false };
        int activeBpm { 0 };
        int activeBpi { 0 };
        int activeIntervalSamples { 0 };
        int activeBeatSamples { 0 };
        int pendingBpm { 0 };
        int pendingBpi { 0 };
        int pendingIntervalSamples { 0 };
        int pendingBeatSamples { 0 };
        bool hasPendingTiming { false };
        int samplesIntoInterval { 0 };
        std::uint64_t intervalIndex { 0 };
    };

    struct RemoteQueuedInterval
    {
        juce::AudioBuffer<float> audio;
        std::uint64_t targetIntervalIndex { 0 };
    };

private:
    struct MixerStripRuntimeState
    {
        MixerStripSnapshot snapshot;
        std::uint64_t lastPresenceIntervalIndex { 0 };
        std::uint64_t lastAudioIntervalIndex { 0 };
    };

    void timerCallback() override;

    app::session::SessionSettingsController::ApplyResult applySettingsDraft(app::session::SessionSettings candidate);
    void applyLifecycleTransition(const app::session::ConnectionLifecycleTransition& transition);
    void clearReconnectTimer();
    void scheduleReconnectTimer(int delayMs);
    void attemptLiveConnect();
    void closeLiveSocket();
    void clearAuthoritativeTiming() noexcept;
    void ensureLocalMonitorMixerStrip();
    void syncRemoteMixerStrip(const net::FramedSocketTransport::RemoteSourceInfo& sourceInfo, bool hasAudioActivity);
    void markRemoteMixerStripInactive(const std::string& sourceId);
    void hideAllRemoteMixerStrips() noexcept;
    void updateRemoteMixerStripActivity();
    void resetMixerStripMeters() noexcept;
    void clearHostTransportSnapshot() noexcept;
    void clearHostSyncAssistState() noexcept;
    void drainRoomTransportEvents();
    void drainPublicServerDiscoveryResults();
    void loadRememberedServersFromStore();
    void persistRememberedServers();
    void applyRoomEvent(const net::FramedSocketTransport::RoomEvent& event);
    void clearRoomUiState(bool connected);
    void refreshPendingRoomVotesFromTiming();
    void rememberSuccessfulServer();

    app::session::SessionSettingsStore settingsStore_;
    app::session::SessionSettingsController settingsController_;
    app::session::ConnectionLifecycleController lifecycleController_;
    std::string lastStatusMessage_ { "Ready" };
    const bool liveTransportEnabled_ { false };
    const bool experimentalStreamingEnabled_ { false };
    bool reconnectScheduled_ { false };
    int scheduledReconnectDelayMs_ { 0 };
    std::mutex liveSocketMutex_;
    mutable std::mutex liveAuthAttemptMutex_;
    LiveAuthAttemptSnapshot lastLiveAuthAttempt_;
    std::unique_ptr<juce::StreamingSocket> liveSocket_;
    net::FramedSocketTransport framedTransport_;
    double currentSampleRate_ { 48000.0 };
    int currentSamplesPerBlock_ { 0 };
    audio::CodecStreamBridge codecStreamBridge_;
    juce::AudioBuffer<float> localUploadIntervalBuffer_;
    int localUploadIntervalWritePosition_ { 0 };
    int transmitWarmupIntervalsRemaining_ { 0 };
    std::atomic<std::size_t> lastCodecPayloadBytes_ { 0 };
    std::atomic<int> lastDecodedSamples_ { 0 };
    std::unordered_map<std::string, std::deque<RemoteQueuedInterval>> remoteQueuedIntervalsBySource_;
    std::unordered_map<std::string, std::uint64_t> nextRemoteTargetBoundaryBySource_;
    std::unordered_map<std::string, juce::AudioBuffer<float>> remoteActiveIntervalBySource_;
    std::unordered_map<std::string, std::uint64_t> lastRemoteActivationBoundaryBySource_;
    std::unordered_map<std::string, RemoteReceiveDiagnosticRuntime> remoteReceiveDiagnosticsBySource_;
    std::unordered_map<std::string, net::FramedSocketTransport::RemoteSourceInfo> knownRemoteSourcesById_;
    std::unordered_map<std::string, MixerStripRuntimeState> mixerStripsBySourceId_;
    CpuDiagnosticSnapshot cpuDiagnosticSnapshot_;
    std::atomic<bool> hasServerTimingForUi_ { false };
    std::atomic<int> serverBpmForUi_ { 0 };
    std::atomic<int> beatsPerIntervalForUi_ { 0 };
    std::atomic<int> currentBeatForUi_ { 0 };
    std::atomic<float> intervalProgressForUi_ { 0.0f };
    std::atomic<std::uint64_t> intervalIndexForUi_ { 0 };
    std::atomic<bool> metronomeEnabled_ { false };
    std::atomic<float> metronomeGainDb_ { 0.0f };
    std::atomic<float> masterOutputGainDb_ { 0.0f };
    std::atomic<bool> hostTransportAvailableForUi_ { false };
    std::atomic<bool> hostTransportPlayingForUi_ { false };
    std::atomic<bool> hostTransportHasTempoForUi_ { false };
    std::atomic<double> hostTransportTempoBpmForUi_ { 0.0 };
    std::atomic<bool> hostTransportHasPpqPositionForUi_ { false };
    std::atomic<double> hostTransportPpqPositionForUi_ { 0.0 };
    std::atomic<bool> hostTransportHasBarStartPpqPositionForUi_ { false };
    std::atomic<double> hostTransportBarStartPpqPositionForUi_ { 0.0 };
    std::atomic<bool> hostTransportHasTimeSignatureForUi_ { false };
    std::atomic<int> hostTransportTimeSigNumeratorForUi_ { 0 };
    std::atomic<int> hostTransportTimeSigDenominatorForUi_ { 0 };
    std::atomic<bool> hostSyncAssistArmed_ { false };
    std::atomic<bool> hostSyncAssistWaitingForHost_ { false };
    std::atomic<bool> hostSyncAssistFailed_ { false };
    std::atomic<HostSyncAssistBlockReason> hostSyncAssistBlockReason_ { HostSyncAssistBlockReason::None };
    std::atomic<HostSyncAssistFailureReason> hostSyncAssistFailureReason_ { HostSyncAssistFailureReason::None };
    std::atomic<int> hostSyncAssistTargetBpmForUi_ { 0 };
    std::atomic<int> hostSyncAssistTargetBpiForUi_ { 0 };
    mutable std::mutex roomUiMutex_;
    RoomUiState roomUiState_;
    mutable std::mutex serverDiscoveryMutex_;
    std::vector<app::session::RememberedServerEntry> rememberedServers_;
    std::vector<app::session::ParsedPublicServerEntry> cachedPublicServers_;
    bool publicServerListFetchInProgress_ { false };
    bool publicServerListStale_ { false };
    std::string publicServerListStatusText_;
    std::unique_ptr<infra::net::PublicServerDiscoveryClient> publicServerDiscoveryClient_;
    std::unique_ptr<famalamajam::infra::state::RememberedServerStore> rememberedServerStore_;
    AuthoritativeTimingState authoritativeTiming_;
    std::uint64_t lastHandledIntervalBoundaryGeneration_ { 0 };
    bool intervalPhaseAnchoredFromDownloadBegin_ { false };
    int metronomeClickRemainingSamples_ { 0 };
    float metronomeClickPhase_ { 0.0f };
    float metronomeClickPhaseIncrement_ { 0.0f };
    float metronomeClickGain_ { 0.0f };
    bool previousHostTransportPlaying_ { false };
    bool transmitEnabled_ { true };
};
} // namespace famalamajam::plugin
