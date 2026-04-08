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
#include "app/session/StemCaptureSettings.h"

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
    static constexpr const char* kLocalHeaderTitle = "Local Sends";
    static constexpr const char* kLocalHeaderTransmitLabel = "Transmit";
    static constexpr const char* kLocalHeaderVoiceLabel = "Voice";
    static constexpr const char* kAddLocalChannelLabel = "+";
    static constexpr const char* kRemoveLocalChannelLabel = "-";
    static constexpr const char* kCollapseLocalChannelLabel = "Collapse";
    static constexpr const char* kExpandLocalChannelLabel = "Expand";
    static constexpr const char* kHideLocalChannelLabel = "Hide";
    static constexpr const char* kConfirmHideLocalChannelLabel = "Confirm hide";
    static constexpr const char* kMainOutputLabel = "FLJ Main Output";
    static constexpr int kDefaultEditorWidth = 1350;
    static constexpr int kDefaultEditorHeight = 760;

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
        bool stale { false };
    };

    struct ServerDiscoveryUiState
    {
        bool fetchInProgress { false };
        bool hasStalePublicData { false };
        std::string statusText;
        std::vector<ServerDiscoveryEntry> combinedEntries;
    };

    struct StemCaptureUiState
    {
        bool enabled { false };
        std::string outputDirectory;
        std::string statusText;
        bool canRequestNewRun { false };
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

    enum class LocalChannelMode
    {
        Interval,
        Voice,
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
        bool soloed { false };
        float meterLeft { 0.0f };
        float meterRight { 0.0f };
        TransmitState transmitState { TransmitState::Disabled };
        LocalChannelMode localChannelMode { LocalChannelMode::Interval };
        bool voiceMode { false };
        bool unsupportedVoiceMode { false };
        std::string statusText;
        bool active { false };
        bool visible { false };
        bool editableName { false };
        int outputAssignmentIndex { 0 };
        std::vector<std::string> outputAssignmentLabels;
    };

    struct CpuDiagnosticSnapshot
    {
        std::uint64_t resizedCalls { 0 };
        std::uint64_t timerCallbackCalls { 0 };
        std::uint64_t transportRefreshCalls { 0 };
        std::uint64_t serverDiscoveryRefreshCalls { 0 };
        std::uint64_t roomRefreshCalls { 0 };
        std::uint64_t diagnosticsRefreshCalls { 0 };
        std::uint64_t diagnosticsDocumentUpdateCalls { 0 };
        std::uint64_t roomFeedRebuildCalls { 0 };
        std::uint64_t roomFeedEntryWidgetsBuilt { 0 };
        std::uint64_t mixerRefreshCalls { 0 };
        std::uint64_t mixerMeterUpdateCalls { 0 };
        std::uint64_t mixerStripUpdateCalls { 0 };
        std::uint64_t mixerStripWidgetBuildCount { 0 };
    };

    using SettingsGetter = std::function<app::session::SessionSettings()>;
    using ApplyHandler = std::function<app::session::SessionSettingsController::ApplyResult(app::session::SessionSettings)>;
    using LifecycleGetter = std::function<app::session::ConnectionLifecycleSnapshot()>;
    using CommandHandler = std::function<bool()>;
    using TransportUiGetter = std::function<TransportUiState()>;
    using HostSyncAssistUiGetter = std::function<HostSyncAssistUiState()>;
    using ServerDiscoveryUiGetter = std::function<ServerDiscoveryUiState()>;
    using ServerDiscoveryRefreshHandler = std::function<bool(bool)>;
    using RoomUiGetter = std::function<RoomUiState()>;
    using RoomMessageHandler = std::function<bool(std::string)>;
    using RoomVoteHandler = std::function<bool(RoomVoteKind, int)>;
    using MixerStripsGetter = std::function<std::vector<MixerStripState>()>;
    using MixerStripSetter = std::function<bool(const std::string&, float, float, bool)>;
    using MixerStripSoloSetter = std::function<bool(const std::string&, bool)>;
    using MixerStripNameSetter = std::function<bool(const std::string&, std::string)>;
    using MixerStripOutputAssignmentSetter = std::function<bool(const std::string&, int)>;
    using LocalChannelVisibilitySetter = std::function<bool(const std::string&, bool)>;
    using DiagnosticsTextGetter = std::function<std::string()>;
    using FloatGetter = std::function<float()>;
    using FloatSetter = std::function<void(float)>;
    using BoolGetter = std::function<bool()>;
    using BoolSetter = std::function<void(bool)>;
    using StemCaptureUiGetter = std::function<StemCaptureUiState()>;
    using StemCaptureSettingsSetter = std::function<bool(app::session::StemCaptureSettings)>;
    using StemCaptureNewRunHandler = std::function<bool()>;
    using VoiceModeToggleHandler = std::function<bool()>;

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
                                    BoolSetter metronomeSetter,
                                    ServerDiscoveryUiGetter serverDiscoveryUiGetter = {},
                                    ServerDiscoveryRefreshHandler serverDiscoveryRefreshHandler = {},
                                    RoomUiGetter roomUiGetter = {},
                                    RoomMessageHandler roomMessageHandler = {},
                                    RoomVoteHandler roomVoteHandler = {},
                                    DiagnosticsTextGetter diagnosticsTextGetter = {},
                                    FloatGetter masterOutputGainGetter = {},
                                    FloatSetter masterOutputGainSetter = {},
                                    FloatGetter metronomeGainGetter = {},
                                    FloatSetter metronomeGainSetter = {},
                                    StemCaptureUiGetter stemCaptureUiGetter = {},
                                    StemCaptureSettingsSetter stemCaptureSettingsSetter = {},
                                    StemCaptureNewRunHandler stemCaptureNewRunHandler = {},
                                    MixerStripNameSetter mixerStripNameSetter = {},
                                    MixerStripOutputAssignmentSetter mixerStripOutputAssignmentSetter = {},
                                    LocalChannelVisibilitySetter localChannelVisibilitySetter = {},
                                    CommandHandler addLocalChannelHandler = {},
                                    CommandHandler transmitToggleHandler = {},
                                    VoiceModeToggleHandler voiceModeToggleHandler = {},
                                    MixerStripSoloSetter mixerStripSoloSetter = {});
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
                                    BoolSetter metronomeSetter,
                                    RoomUiGetter roomUiGetter,
                                    RoomMessageHandler roomMessageHandler,
                                    RoomVoteHandler roomVoteHandler,
                                    DiagnosticsTextGetter diagnosticsTextGetter = {},
                                    FloatGetter masterOutputGainGetter = {},
                                    FloatSetter masterOutputGainSetter = {},
                                    FloatGetter metronomeGainGetter = {},
                                    FloatSetter metronomeGainSetter = {},
                                    StemCaptureUiGetter stemCaptureUiGetter = {},
                                    StemCaptureSettingsSetter stemCaptureSettingsSetter = {},
                                    StemCaptureNewRunHandler stemCaptureNewRunHandler = {},
                                    MixerStripNameSetter mixerStripNameSetter = {},
                                    MixerStripOutputAssignmentSetter mixerStripOutputAssignmentSetter = {},
                                    LocalChannelVisibilitySetter localChannelVisibilitySetter = {},
                                    CommandHandler addLocalChannelHandler = {},
                                    CommandHandler transmitToggleHandler = {},
                                    VoiceModeToggleHandler voiceModeToggleHandler = {},
                                    MixerStripSoloSetter mixerStripSoloSetter = {});
    ~FamaLamaJamAudioProcessorEditor() override;

    void resized() override;

    [[nodiscard]] juce::String getTransportStatusTextForTesting() const;
    [[nodiscard]] juce::String getHostSyncAssistStatusTextForTesting() const;
    [[nodiscard]] juce::String getHostSyncAssistButtonTextForTesting() const;
    [[nodiscard]] bool isHostSyncAssistEnabledForTesting() const noexcept;
    [[nodiscard]] double getIntervalProgressForTesting() const noexcept;
    [[nodiscard]] int getIntervalBeatDivisionsForTesting() const noexcept;
    [[nodiscard]] bool isMetronomeToggleEnabledForTesting() const noexcept;
    [[nodiscard]] juce::String getRoomTopicTextForTesting() const;
    [[nodiscard]] juce::String getRoomStatusTextForTesting() const;
    [[nodiscard]] juce::String getServerDiscoveryStatusTextForTesting() const;
    [[nodiscard]] std::vector<juce::String> getVisibleServerDiscoveryLabelsForTesting() const;
    [[nodiscard]] juce::String getSelectedServerDiscoveryLabelForTesting() const;
    [[nodiscard]] juce::String getHostTextForTesting() const;
    [[nodiscard]] juce::String getPortTextForTesting() const;
    [[nodiscard]] juce::String getUsernameTextForTesting() const;
    [[nodiscard]] juce::String getPasswordTextForTesting() const;
    [[nodiscard]] std::vector<RoomFeedEntry> getVisibleRoomFeedForTesting() const;
    [[nodiscard]] juce::String getRoomVoteStatusTextForTesting(RoomVoteKind kind) const;
    [[nodiscard]] bool isRoomComposerEnabledForTesting() const noexcept;
    [[nodiscard]] bool isRoomVoteEnabledForTesting(RoomVoteKind kind) const noexcept;
    [[nodiscard]] bool isRoomSectionAboveMixerForTesting() const noexcept;
    [[nodiscard]] bool hasRoomFeedViewportForTesting() const noexcept;
    [[nodiscard]] juce::String getRoomComposerTextForTesting() const;
    void setRoomComposerTextForTesting(const juce::String& text);
    bool selectServerDiscoveryEntryForTesting(int index);
    bool clickServerDiscoveryRefreshForTesting();
    bool submitRoomComposerForTesting(bool useReturnKey);
    void setRoomVoteValueForTesting(RoomVoteKind kind, int value);
    bool submitRoomVoteForTesting(RoomVoteKind kind);
    struct MixerStripLayoutSnapshotForTesting
    {
        juce::Rectangle<int> stripBounds;
        juce::Rectangle<int> meterBounds;
        juce::Rectangle<int> gainBounds;
        juce::Rectangle<int> panBounds;
        juce::Rectangle<int> soloBounds;
        juce::Rectangle<int> muteBounds;
        juce::Rectangle<int> transmitBounds;
        juce::Rectangle<int> voiceBounds;
        juce::Rectangle<int> outputBounds;
    };

    struct MixerLocalHeaderLayoutSnapshotForTesting
    {
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> labelBounds;
        juce::Rectangle<int> removeBounds;
        juce::Rectangle<int> addBounds;
        juce::Rectangle<int> collapseBounds;
    };

    [[nodiscard]] juce::Rectangle<int> getMixerViewportBoundsForTesting() const;
    [[nodiscard]] bool getMixerStripLayoutSnapshotForTesting(const juce::String& sourceId,
                                                             MixerStripLayoutSnapshotForTesting& snapshot) const;
    [[nodiscard]] MixerLocalHeaderLayoutSnapshotForTesting getLocalHeaderLayoutSnapshotForTesting() const;
    [[nodiscard]] std::vector<juce::String> getVisibleMixerGroupLabelsForTesting() const;
    [[nodiscard]] std::vector<juce::String> getVisibleMixerStripLabelsForTesting() const;
    [[nodiscard]] juce::String getMixerStripStatusTextForTesting(const juce::String& sourceId) const;
    [[nodiscard]] juce::String getMixerStripTransmitButtonTextForTesting(const juce::String& sourceId) const;
    [[nodiscard]] juce::String getMixerStripVoiceButtonTextForTesting(const juce::String& sourceId) const;
    [[nodiscard]] bool getMixerStripVoiceToggleStateForTesting(const juce::String& sourceId, bool& enabled) const;
    [[nodiscard]] bool getMixerStripSoloStateForTesting(const juce::String& sourceId, bool& soloed) const;
    [[nodiscard]] bool getMixerStripControlStateForTesting(const juce::String& sourceId,
                                                           double& gain,
                                                           double& pan,
                                                           bool& muted) const;
    [[nodiscard]] bool getMixerStripMeterLevelsForTesting(const juce::String& sourceId, float& left, float& right) const;
    [[nodiscard]] juce::Colour getMixerStripStatusColourForTesting(const juce::String& sourceId) const;
    [[nodiscard]] juce::String getMixerStripRemoveButtonTextForTesting(const juce::String& sourceId) const;
    bool setMixerStripControlStateForTesting(const juce::String& sourceId, double gain, double pan, bool muted);
    bool clickMixerStripTransmitForTesting(const juce::String& sourceId);
    bool clickMixerStripVoiceToggleForTesting(const juce::String& sourceId);
    bool clickMixerStripRemoveForTesting(const juce::String& sourceId);
    bool setMixerStripSoloStateForTesting(const juce::String& sourceId, bool soloed);
    [[nodiscard]] CpuDiagnosticSnapshot getCpuDiagnosticSnapshotForTesting() const noexcept;
    void resetCpuDiagnosticSnapshotForTesting() noexcept;
    [[nodiscard]] juce::String getDiagnosticsTextForTesting() const;
    [[nodiscard]] bool isDiagnosticsExpandedForTesting() const noexcept;
    [[nodiscard]] bool isLocalGroupCollapsedForTesting() const noexcept;
    [[nodiscard]] juce::String getServerSettingsSummaryForTesting() const;
    [[nodiscard]] juce::String getStemCaptureDirectoryForTesting() const;
    [[nodiscard]] juce::String getStemCaptureStatusTextForTesting() const;
    [[nodiscard]] bool isStemCaptureEnabledForTesting() const noexcept;
    [[nodiscard]] bool canClickNewStemRunForTesting() const noexcept;
    void setSettingsDraftForTesting(const app::session::SessionSettings& settings);
    void setPasswordTextForTesting(const juce::String& text);
    void setStemCaptureDirectoryForTesting(const juce::String& text);
    void clickConnectForTesting();
    void clickHostSyncAssistForTesting();
    void clickDiagnosticsToggleForTesting();
    void clickStemCaptureToggleForTesting();
    void clickNewStemRunForTesting();
    void runTimerTickForTesting();
    void refreshForTesting();

private:
    struct RoomFeedEntryWidgets
    {
        juce::TextEditor editor;
    };

    struct RoomVoteFeedbackState
    {
        bool lastPending { false };
        bool lastFailed { false };
        int lastRequestedValue { 0 };
        std::string lastStatusText;
        juce::String transientFailureText;
        int transientFailureTicks { 0 };
    };

    struct MixerStripWidgets
    {
        std::string sourceId;
        std::string groupId;
        juce::Label groupLabel;
        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::Label statusLabel;
        StereoMeterComponent meter;
        juce::Slider gainSlider;
        juce::Slider panSlider;
        juce::ToggleButton soloToggle;
        juce::ToggleButton muteToggle;
        juce::TextEditor nameEditor;
        juce::TextButton transmitButton;
        juce::ToggleButton voiceModeToggle;
        juce::ComboBox outputSelector;
        juce::TextButton removeButton;
        bool editableName { false };
        bool hasTransmitControl { false };
        bool hasVoiceModeControl { false };
        bool removable { false };
        bool showsGroupLabel { false };
        float lastMeterLeft { 0.0f };
        float lastMeterRight { 0.0f };
        juce::Rectangle<int> stripBounds;
    };

    void timerCallback() override;
    app::session::SessionSettings makeDraftFromUi() const;
    void loadFromSettings(const app::session::SessionSettings& settings);
    void applyServerDiscoveryEntrySelection(const ServerDiscoveryEntry& entry);
    void applyRememberedPassword(const std::string& password);
    void clearRememberedPassword();
    void updateRoomVoteInputState(RoomVoteKind changedKind);
    void refreshLifecycleStatus();
    void refreshTransportStatus();
    void refreshHostSyncAssistStatus();
    void refreshServerDiscoveryUi();
    void refreshRoomUi();
    void refreshStemCaptureUi();
    void refreshDiagnosticsUi();
    void rebuildRoomFeedWidgets(const std::vector<RoomFeedEntry>& entries);
    bool submitRoomComposerMessage();
    bool submitRoomVote(RoomVoteKind kind);
    void refreshMixerStrips();
    void rebuildMixerStripWidgets(const std::vector<MixerStripState>& visibleStrips);
    void updateTransmitButtonAppearance(MixerStripWidgets& widgets, const MixerStripState& strip);
    void updateVoiceModeButtonAppearance(MixerStripWidgets& widgets, const MixerStripState& strip);
    [[nodiscard]] RoomVoteKind getActiveRoomVoteKind() const noexcept;
    [[nodiscard]] bool isRoomFeedNearBottom() const noexcept;
    void scrollRoomFeedToBottom();
    bool applyStemCaptureSettingsFromUi(bool enabled);
    void launchStemCaptureFolderChooser();
    [[nodiscard]] juce::String getCollapsedServerSummary() const;
    [[nodiscard]] juce::String getCollapsedServerSummaryAscii() const;
    [[nodiscard]] juce::String getDiagnosticsToggleText() const;
    [[nodiscard]] juce::String getServerSettingsToggleText() const;

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
    MixerStripSoloSetter mixerStripSoloSetter_;
    MixerStripNameSetter mixerStripNameSetter_;
    MixerStripOutputAssignmentSetter mixerStripOutputAssignmentSetter_;
    LocalChannelVisibilitySetter localChannelVisibilitySetter_;
    BoolGetter metronomeGetter_;
    BoolSetter metronomeSetter_;
    ServerDiscoveryUiGetter serverDiscoveryUiGetter_;
    ServerDiscoveryRefreshHandler serverDiscoveryRefreshHandler_;
    RoomUiGetter roomUiGetter_;
    RoomMessageHandler roomMessageHandler_;
    RoomVoteHandler roomVoteHandler_;
    DiagnosticsTextGetter diagnosticsTextGetter_;
    FloatGetter masterOutputGainGetter_;
    FloatSetter masterOutputGainSetter_;
    FloatGetter metronomeGainGetter_;
    FloatSetter metronomeGainSetter_;
    StemCaptureUiGetter stemCaptureUiGetter_;
    StemCaptureSettingsSetter stemCaptureSettingsSetter_;
    StemCaptureNewRunHandler stemCaptureNewRunHandler_;
    CommandHandler addLocalChannelHandler_;
    CommandHandler transmitToggleHandler_;
    VoiceModeToggleHandler voiceModeToggleHandler_;

    juce::Label titleLabel_;
    juce::TextButton serverSettingsToggle_;
    juce::Label serverSettingsSummaryLabel_;
    juce::Label hostLabel_;
    juce::Label portLabel_;
    juce::Label usernameLabel_;
    juce::Label passwordLabel_;
    juce::Label gainLabel_;
    juce::Label panLabel_;
    juce::Label serverPickerLabel_;

    juce::ComboBox serverPickerCombo_;
    juce::TextButton refreshServersButton_;
    juce::Label serverDiscoveryStatusLabel_;
    juce::TextEditor hostEditor_;
    juce::TextEditor portEditor_;
    juce::TextEditor usernameEditor_;
    juce::TextEditor passwordEditor_;
    juce::ToggleButton stemCaptureToggle_;
    juce::TextButton stemCaptureBrowseButton_;
    juce::TextButton stemCaptureNewRunButton_;
    juce::Label stemCapturePathLabel_;
    juce::Label stemCapturePathValueLabel_;
    juce::Label stemCaptureStatusLabel_;
    juce::Slider gainSlider_;
    juce::Slider panSlider_;
    juce::ToggleButton muteToggle_;
    juce::ToggleButton metronomeToggle_;
    juce::Slider metronomeVolumeSlider_;
    juce::TextButton connectButton_;
    juce::TextButton disconnectButton_;
    juce::Label authStatusLabel_;
    juce::Label transportLabel_;
    juce::TextButton diagnosticsToggle_;
    juce::TextEditor diagnosticsEditor_;
    juce::Label hostSyncAssistTargetLabel_;
    juce::TextButton hostSyncAssistButton_;
    juce::Label hostSyncAssistStatusLabel_;
    juce::Label roomSectionLabel_;
    juce::Label roomTopicLabel_;
    juce::Label roomTopicValueLabel_;
    juce::Label roomStatusLabel_;
    juce::Label roomComposerLabel_;
    juce::TextEditor roomComposerEditor_;
    juce::TextButton roomSendButton_;
    juce::Label roomBpmLabel_;
    juce::TextEditor roomBpmEditor_;
    juce::Label roomBpmStatusLabel_;
    juce::Label roomBpiLabel_;
    juce::TextEditor roomBpiEditor_;
    juce::TextButton roomVoteButton_;
    juce::Label roomBpiStatusLabel_;
    juce::Viewport roomFeedViewport_;
    juce::Component roomFeedContent_;
    std::vector<std::unique_ptr<RoomFeedEntryWidgets>> roomFeedWidgets_;
    RoomVoteFeedbackState roomBpmFeedbackState_;
    RoomVoteFeedbackState roomBpiFeedbackState_;
    double intervalProgressValue_ { 0.0 };
    BeatDividedProgressBar intervalProgressBar_;
    juce::Label mixerSectionLabel_;
    juce::Viewport mixerViewport_;
    juce::Component mixerContent_;
    juce::Label localHeaderLabel_;
    juce::ToggleButton localHeaderTransmitToggle_;
    juce::ToggleButton localHeaderVoiceToggle_;
    juce::TextButton removeLocalChannelButton_;
    juce::TextButton addLocalChannelButton_;
    juce::TextButton collapseLocalChannelButton_;
    juce::Label masterOutputLabel_;
    juce::Slider masterOutputSlider_;
    std::vector<std::unique_ptr<MixerStripWidgets>> mixerStripWidgets_;
    std::vector<std::string> visibleMixerStripOrder_;
    std::vector<MixerStripState> currentVisibleMixerStrips_;
    std::string pendingLocalRemovalSourceId_;
    CpuDiagnosticSnapshot cpuDiagnosticSnapshot_;
    std::string selectedServerDiscoveryEndpointKey_;
    std::string recalledPassword_;
    StemCaptureUiState currentStemCaptureUiState_;
    bool showingRememberedPasswordMask_ { false };
    bool updatingPasswordEditor_ { false };
    ServerDiscoveryUiState currentServerDiscoveryUiState_;
    bool refreshingServerDiscoveryUi_ { false };
    RoomUiState currentRoomUiState_;
    juce::Label statusLabel_;
    bool hostSyncAssistLastActionWasCancel_ { false };
    bool serverSettingsExpanded_ { true };
    bool diagnosticsExpanded_ { false };
    bool localGroupCollapsed_ { false };
    bool updatingRoomVoteInputs_ { false };
    std::unique_ptr<juce::FileChooser> stemCaptureFolderChooser_;
    juce::String stemCaptureInlineStatusText_;
    std::uint64_t uiRefreshTick_ { 0 };
    bool lastTransportUiInitialized_ { false };
    SyncHealth lastTransportSyncHealth_ { SyncHealth::Disconnected };
    int lastTransportBeat_ { 0 };
    std::uint64_t lastTransportIntervalIndex_ { 0 };
    std::string lastDiagnosticsText_;
    RoomVoteKind activeRoomVoteKind_ { RoomVoteKind::Bpm };
};
} // namespace famalamajam::plugin
