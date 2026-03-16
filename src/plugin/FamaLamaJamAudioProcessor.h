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
#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"
#include "audio/CodecStreamBridge.h"
#include "net/FramedSocketTransport.h"

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

    enum class MixerStripKind
    {
        LocalMonitor,
        RemoteDelayed,
    };

    struct MixerStripMixState
    {
        float gainDb { 0.0f };
        float pan { 0.0f };
        bool muted { false };
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
    };

    explicit FamaLamaJamAudioProcessor(bool enableLiveTransport = false,
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
    [[nodiscard]] std::size_t getTransportSentFramesForTesting() const noexcept;
    [[nodiscard]] std::size_t getTransportReceivedFramesForTesting() const noexcept;
    [[nodiscard]] std::size_t getActiveRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] bool isRemoteSourceActiveForTesting(const std::string& sourceId) const;
    [[nodiscard]] std::vector<std::string> getActiveRemoteSourceIdsForTesting() const;
    [[nodiscard]] std::size_t getQueuedRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] std::size_t getPendingRemoteSourceCountForTesting() const noexcept;
    [[nodiscard]] TransportUiState getTransportUiState() const noexcept;
    [[nodiscard]] bool isMetronomeEnabled() const noexcept;
    void setMetronomeEnabled(bool enabled) noexcept;
    [[nodiscard]] std::vector<MixerStripSnapshot> getMixerStripSnapshots() const;
    [[nodiscard]] bool getMixerStripSnapshot(const std::string& sourceId, MixerStripSnapshot& snapshot) const;
    bool setMixerStripMixState(const std::string& sourceId, float gainDb, float pan, bool muted);

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

    struct RemotePendingInterval
    {
        juce::AudioBuffer<float> audio;
        int writePosition { 0 };
        std::uint64_t targetIntervalIndex { 0 };
    };

private:
    struct MixerStripRuntimeState
    {
        MixerStripSnapshot snapshot;
        std::uint64_t lastSeenIntervalIndex { 0 };
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
    void syncRemoteMixerStrip(const net::FramedSocketTransport::RemoteSourceInfo& sourceInfo);
    void markRemoteMixerStripInactive(const std::string& sourceId);
    void hideAllRemoteMixerStrips() noexcept;
    void updateRemoteMixerStripActivity();
    void resetMixerStripMeters() noexcept;

    app::session::SessionSettingsStore settingsStore_;
    app::session::SessionSettingsController settingsController_;
    app::session::ConnectionLifecycleController lifecycleController_;
    std::string lastStatusMessage_ { "Ready" };
    const bool liveTransportEnabled_ { false };
    const bool experimentalStreamingEnabled_ { false };
    bool reconnectScheduled_ { false };
    int scheduledReconnectDelayMs_ { 0 };
    std::mutex liveSocketMutex_;
    std::unique_ptr<juce::StreamingSocket> liveSocket_;
    net::FramedSocketTransport framedTransport_;
    double currentSampleRate_ { 48000.0 };
    int currentSamplesPerBlock_ { 0 };
    audio::CodecStreamBridge codecStreamBridge_;
    juce::AudioBuffer<float> localUploadIntervalBuffer_;
    int localUploadIntervalWritePosition_ { 0 };
    std::atomic<std::size_t> lastCodecPayloadBytes_ { 0 };
    std::atomic<int> lastDecodedSamples_ { 0 };
    std::unordered_map<std::string, std::deque<RemoteQueuedInterval>> remoteQueuedIntervalsBySource_;
    std::unordered_map<std::string, juce::AudioBuffer<float>> remoteActiveIntervalBySource_;
    std::unordered_map<std::string, RemotePendingInterval> remotePendingIntervalsBySource_;
    std::unordered_map<std::string, MixerStripRuntimeState> mixerStripsBySourceId_;
    std::atomic<bool> hasServerTimingForUi_ { false };
    std::atomic<int> serverBpmForUi_ { 0 };
    std::atomic<int> beatsPerIntervalForUi_ { 0 };
    std::atomic<int> currentBeatForUi_ { 0 };
    std::atomic<float> intervalProgressForUi_ { 0.0f };
    std::atomic<std::uint64_t> intervalIndexForUi_ { 0 };
    std::atomic<bool> metronomeEnabled_ { false };
    AuthoritativeTimingState authoritativeTiming_;
    int metronomeClickRemainingSamples_ { 0 };
    float metronomeClickPhase_ { 0.0f };
    float metronomeClickPhaseIncrement_ { 0.0f };
    float metronomeClickGain_ { 0.0f };
};
} // namespace famalamajam::plugin
