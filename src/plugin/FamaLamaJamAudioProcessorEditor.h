#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"

namespace famalamajam::plugin
{
class BeatDividedProgressBar final : public juce::Component
{
public:
    void setProgress(double progress, int beatDivisions);
    [[nodiscard]] double getProgressForTesting() const noexcept;
    [[nodiscard]] int getBeatDivisionsForTesting() const noexcept;

    void paint(juce::Graphics& graphics) override;

private:
    double progress_ { 0.0 };
    int beatDivisions_ { 0 };
};

class StereoMeterComponent final : public juce::Component
{
public:
    void setLevels(float left, float right);
    [[nodiscard]] float getLeftLevelForTesting() const noexcept;
    [[nodiscard]] float getRightLevelForTesting() const noexcept;

    void paint(juce::Graphics& graphics) override;

private:
    float leftLevel_ { 0.0f };
    float rightLevel_ { 0.0f };
};

class FamaLamaJamAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
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
        bool hostPlaying { false };
    };

    enum class MixerStripKind
    {
        LocalMonitor,
        RemoteDelayed,
    };

    struct MixerStripState
    {
        MixerStripKind kind { MixerStripKind::RemoteDelayed };
        std::string sourceId;
        std::string groupId;
        std::string groupLabel;
        std::string displayName;
        std::string subtitle;
        float gainDb { 0.0f };
        float pan { 0.0f };
        bool muted { false };
        float meterLeft { 0.0f };
        float meterRight { 0.0f };
        bool active { false };
        bool visible { false };
    };

    using SettingsGetter = std::function<app::session::SessionSettings()>;
    using ApplyHandler = std::function<app::session::SessionSettingsController::ApplyResult(app::session::SessionSettings)>;
    using LifecycleGetter = std::function<app::session::ConnectionLifecycleSnapshot()>;
    using CommandHandler = std::function<bool()>;
    using TransportUiGetter = std::function<TransportUiState()>;
    using HostSyncAssistUiGetter = std::function<HostSyncAssistUiState()>;
    using MixerStripsGetter = std::function<std::vector<MixerStripState>()>;
    using MixerStripSetter = std::function<bool(const std::string&, float, float, bool)>;
    using BoolGetter = std::function<bool()>;
    using BoolSetter = std::function<void(bool)>;

    FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
                                    SettingsGetter settingsGetter,
                                    ApplyHandler applyHandler,
                                    LifecycleGetter lifecycleGetter,
                                    CommandHandler connectHandler,
                                    CommandHandler disconnectHandler,
                                    TransportUiGetter transportUiGetter,
                                    HostSyncAssistUiGetter hostSyncAssistUiGetter,
                                    CommandHandler hostSyncAssistToggleHandler,
                                    MixerStripsGetter mixerStripsGetter,
                                    MixerStripSetter mixerStripSetter,
                                    BoolGetter metronomeGetter,
                                    BoolSetter metronomeSetter);

    void resized() override;

    [[nodiscard]] juce::String getTransportStatusTextForTesting() const;
    [[nodiscard]] juce::String getHostSyncAssistStatusTextForTesting() const;
    [[nodiscard]] juce::String getHostSyncAssistButtonTextForTesting() const;
    [[nodiscard]] bool isHostSyncAssistEnabledForTesting() const noexcept;
    [[nodiscard]] double getIntervalProgressForTesting() const noexcept;
    [[nodiscard]] int getIntervalBeatDivisionsForTesting() const noexcept;
    [[nodiscard]] bool isMetronomeToggleEnabledForTesting() const noexcept;
    [[nodiscard]] std::vector<juce::String> getVisibleMixerGroupLabelsForTesting() const;
    [[nodiscard]] std::vector<juce::String> getVisibleMixerStripLabelsForTesting() const;
    [[nodiscard]] bool getMixerStripControlStateForTesting(const juce::String& sourceId,
                                                           double& gain,
                                                           double& pan,
                                                           bool& muted) const;
    bool setMixerStripControlStateForTesting(const juce::String& sourceId, double gain, double pan, bool muted);
    void setSettingsDraftForTesting(const app::session::SessionSettings& settings);
    void clickConnectForTesting();
    void clickHostSyncAssistForTesting();
    void refreshForTesting();

private:
    struct MixerStripWidgets
    {
        std::string sourceId;
        std::string groupId;
        juce::Label groupLabel;
        juce::Label titleLabel;
        juce::Label subtitleLabel;
        StereoMeterComponent meter;
        juce::Slider gainSlider;
        juce::Slider panSlider;
        juce::ToggleButton muteToggle;
        bool showsGroupLabel { false };
    };

    void timerCallback() override;
    app::session::SessionSettings makeDraftFromUi() const;
    void loadFromSettings(const app::session::SessionSettings& settings);
    void refreshLifecycleStatus();
    void refreshTransportStatus();
    void refreshHostSyncAssistStatus();
    void refreshMixerStrips();
    void rebuildMixerStripWidgets(const std::vector<MixerStripState>& visibleStrips);

    SettingsGetter settingsGetter_;
    ApplyHandler applyHandler_;
    LifecycleGetter lifecycleGetter_;
    CommandHandler connectHandler_;
    CommandHandler disconnectHandler_;
    TransportUiGetter transportUiGetter_;
    HostSyncAssistUiGetter hostSyncAssistUiGetter_;
    CommandHandler hostSyncAssistToggleHandler_;
    MixerStripsGetter mixerStripsGetter_;
    MixerStripSetter mixerStripSetter_;
    BoolGetter metronomeGetter_;
    BoolSetter metronomeSetter_;

    juce::Label titleLabel_;
    juce::Label hostLabel_;
    juce::Label portLabel_;
    juce::Label usernameLabel_;
    juce::Label gainLabel_;
    juce::Label panLabel_;

    juce::TextEditor hostEditor_;
    juce::TextEditor portEditor_;
    juce::TextEditor usernameEditor_;
    juce::Slider gainSlider_;
    juce::Slider panSlider_;
    juce::ToggleButton muteToggle_;
    juce::ToggleButton metronomeToggle_;
    juce::TextButton connectButton_;
    juce::TextButton disconnectButton_;
    juce::Label transportLabel_;
    juce::Label hostSyncAssistTargetLabel_;
    juce::TextButton hostSyncAssistButton_;
    juce::Label hostSyncAssistStatusLabel_;
    double intervalProgressValue_ { 0.0 };
    BeatDividedProgressBar intervalProgressBar_;
    juce::Label mixerSectionLabel_;
    juce::Viewport mixerViewport_;
    juce::Component mixerContent_;
    std::vector<std::unique_ptr<MixerStripWidgets>> mixerStripWidgets_;
    std::vector<std::string> visibleMixerStripOrder_;
    juce::Label statusLabel_;
    bool hostSyncAssistLastActionWasCancel_ { false };
};
} // namespace famalamajam::plugin
