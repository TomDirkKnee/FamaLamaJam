#include "FamaLamaJamAudioProcessorEditor.h"

#include <algorithm>

#include "app/session/ConnectionFailureClassifier.h"

namespace famalamajam::plugin
{
void BeatDividedProgressBar::setProgress(double progress, int beatDivisions)
{
    const auto nextProgress = juce::jlimit(0.0, 1.0, progress);
    const auto nextBeatDivisions = juce::jmax(0, beatDivisions);

    if (juce::approximatelyEqual(progress_, nextProgress) && beatDivisions_ == nextBeatDivisions)
        return;

    progress_ = nextProgress;
    beatDivisions_ = nextBeatDivisions;
    repaint();
}

double BeatDividedProgressBar::getProgressForTesting() const noexcept
{
    return progress_;
}

int BeatDividedProgressBar::getBeatDivisionsForTesting() const noexcept
{
    return beatDivisions_;
}

void BeatDividedProgressBar::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (bounds.isEmpty())
        return;

    graphics.setColour(juce::Colour::fromRGB(28, 34, 44));
    graphics.fillRoundedRectangle(bounds, 4.0f);

    if (progress_ > 0.0)
    {
        auto filled = bounds;
        filled.setWidth(bounds.getWidth() * static_cast<float>(progress_));

        graphics.saveState();
        graphics.reduceClipRegion(filled.getSmallestIntegerContainer());
        graphics.setColour(juce::Colour::fromRGB(87, 168, 118));
        graphics.fillRoundedRectangle(bounds, 4.0f);
        graphics.restoreState();
    }

    if (beatDivisions_ > 1)
    {
        graphics.setColour(juce::Colour::fromRGBA(255, 255, 255, 72));

        for (int division = 1; division < beatDivisions_; ++division)
        {
            const auto x = bounds.getX() + (bounds.getWidth() * static_cast<float>(division)
                                            / static_cast<float>(beatDivisions_));
            graphics.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());
        }
    }

    graphics.setColour(juce::Colour::fromRGBA(255, 255, 255, 96));
    graphics.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void StereoMeterComponent::setLevels(float left, float right)
{
    const auto nextLeft = juce::jlimit(0.0f, 1.0f, left);
    const auto nextRight = juce::jlimit(0.0f, 1.0f, right);
    if (juce::approximatelyEqual(leftLevel_, nextLeft) && juce::approximatelyEqual(rightLevel_, nextRight))
        return;

    leftLevel_ = nextLeft;
    rightLevel_ = nextRight;
    repaint();
}

float StereoMeterComponent::getLeftLevelForTesting() const noexcept
{
    return leftLevel_;
}

float StereoMeterComponent::getRightLevelForTesting() const noexcept
{
    return rightLevel_;
}

void StereoMeterComponent::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (bounds.isEmpty())
        return;

    graphics.setColour(juce::Colour::fromRGB(22, 26, 33));
    graphics.fillRoundedRectangle(bounds, 4.0f);

    auto topHalf = bounds.removeFromTop(bounds.getHeight() * 0.5f).reduced(4.0f, 3.0f);
    auto bottomHalf = bounds.reduced(4.0f, 3.0f);

    auto drawMeter = [&](juce::Rectangle<float> area, float level, juce::Colour colour) {
        graphics.setColour(juce::Colour::fromRGB(42, 48, 56));
        graphics.fillRoundedRectangle(area, 3.0f);

        auto filled = area;
        filled.setWidth(area.getWidth() * level);
        graphics.setColour(colour);
        graphics.fillRoundedRectangle(filled, 3.0f);
    };

    drawMeter(topHalf, leftLevel_, juce::Colour::fromRGB(86, 174, 114));
    drawMeter(bottomHalf, rightLevel_, juce::Colour::fromRGB(79, 146, 204));
}

namespace
{
constexpr auto kRememberedPasswordMask = "********";

[[nodiscard]] bool serverDiscoveryEntryMatches(const FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry& lhs,
                                               const FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry& rhs)
{
    return lhs.source == rhs.source
        && lhs.label == rhs.label
        && lhs.host == rhs.host
        && lhs.port == rhs.port
        && lhs.username == rhs.username
        && lhs.password == rhs.password
        && lhs.connectedUsers == rhs.connectedUsers
        && lhs.stale == rhs.stale;
}

[[nodiscard]] bool serverDiscoveryStateMatches(const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& lhs,
                                               const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& rhs)
{
    if (lhs.fetchInProgress != rhs.fetchInProgress
        || lhs.hasStalePublicData != rhs.hasStalePublicData
        || lhs.statusText != rhs.statusText
        || lhs.combinedEntries.size() != rhs.combinedEntries.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.combinedEntries.size(); ++index)
    {
        if (! serverDiscoveryEntryMatches(lhs.combinedEntries[index], rhs.combinedEntries[index]))
            return false;
    }

    return true;
}

[[nodiscard]] std::string makeServerDiscoveryEndpointKey(std::string_view host, std::uint16_t port)
{
    return juce::String(host.data()).trim().toLowerCase().toStdString() + ":" + std::to_string(port);
}

[[nodiscard]] int secondsFromDelayMs(int delayMs)
{
    if (delayMs <= 0)
        return 0;

    return (delayMs + 999) / 1000;
}

[[nodiscard]] std::string formatLifecycleStatus(const app::session::ConnectionLifecycleSnapshot& snapshot)
{
    switch (snapshot.state)
    {
        case app::session::ConnectionState::Idle:
            if (snapshot.statusMessage.empty() || snapshot.statusMessage == "Idle")
                return "Ready";
            return snapshot.statusMessage;

        case app::session::ConnectionState::Connecting:
            return "Connecting";

        case app::session::ConnectionState::Active:
            return {};

        case app::session::ConnectionState::Reconnecting:
            if (! snapshot.statusMessage.empty()
                && snapshot.statusMessage != "Reconnecting")
            {
                return snapshot.statusMessage;
            }

        {
            std::string status = "Reconnecting";

            if (snapshot.nextRetryDelayMs > 0)
                status += " in " + std::to_string(secondsFromDelayMs(snapshot.nextRetryDelayMs)) + "s";

            if (snapshot.retryAttemptLimit > 0 && snapshot.retryAttempt > 0)
            {
                status += " (attempt " + std::to_string(snapshot.retryAttempt) + "/"
                        + std::to_string(snapshot.retryAttemptLimit) + ")";
            }

            if (! snapshot.lastError.empty())
                status += ": " + snapshot.lastError;

            return status;
        }

        case app::session::ConnectionState::Error:
            if (! snapshot.statusMessage.empty() && snapshot.statusMessage != "Error")
                return snapshot.statusMessage;

            if (! snapshot.lastError.empty())
                return "Error: " + snapshot.lastError + ". Press Connect to retry.";

            if (snapshot.statusMessage.empty() || snapshot.statusMessage == "Error")
                return "Error. Press Connect to retry.";

            return snapshot.statusMessage;
    }

    return "Ready";
}

[[nodiscard]] std::string formatTransportStatus(const FamaLamaJamAudioProcessorEditor::TransportUiState& state)
{
    if ((state.hasServerTiming || (state.beatsPerMinute > 0 && state.beatsPerInterval > 0))
        && state.beatsPerMinute > 0 && state.beatsPerInterval > 0)
    {
        return std::to_string(state.beatsPerMinute) + " BPM | " + std::to_string(state.beatsPerInterval) + " BPI";
    }

    switch (state.syncHealth)
    {
        case FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected:
            return {};

        case FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming:
            return state.connected ? "Waiting for room timing." : std::string {};

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Reconnecting:
            return "Reconnecting room timing.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost:
            return "Room timing lost.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy:
            return {};
    }

    return {};
}

[[nodiscard]] std::string formatHostSyncAssistTarget(
    const FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState& state)
{
    if (state.targetBeatsPerMinute > 0 && state.targetBeatsPerInterval > 0)
    {
        return "Room sync target: " + std::to_string(state.targetBeatsPerMinute)
             + " BPM / " + std::to_string(state.targetBeatsPerInterval) + " BPI";
    }

    return "Room sync target: waiting for interval timing";
}

[[nodiscard]] std::string formatHostSyncAssistStatus(
    const FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState& state,
    bool lastActionWasCancel)
{
    if (state.failed)
    {
        switch (state.failureReason)
        {
            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::TimingLost:
                return "Room timing dropped before Ableton could start aligned.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::MissingHostMusicalPosition:
                return "Couldn't align from Ableton's playhead. Re-arm and try again.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::None:
                break;
        }
    }

    if (state.armed || state.waitingForHost)
        return "Armed. Press Play in Ableton to start aligned.";

    if (state.blocked)
    {
        switch (state.blockReason)
        {
            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::MissingServerTiming:
                return "Sync assist needs room timing before it can arm.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::MissingHostTempo:
                return "Open Ableton transport so sync assist can read the host tempo.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::HostTempoMismatch:
                return {};

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::None:
                break;
        }
    }

    if (lastActionWasCancel)
        return "Sync assist canceled. Arm again when you want Ableton Play to start aligned.";

    if (state.targetBeatsPerMinute > 0 && state.targetBeatsPerInterval > 0)
        return {};

    return {};
}

[[nodiscard]] juce::Colour stripAccentColour(FamaLamaJamAudioProcessorEditor::MixerStripKind kind, bool active)
{
    if (kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
        return active ? juce::Colour::fromRGB(214, 141, 72) : juce::Colour::fromRGB(132, 102, 74);

    return active ? juce::Colour::fromRGB(80, 150, 204) : juce::Colour::fromRGB(78, 90, 110);
}

[[nodiscard]] juce::String formatRoomFeedEntryText(
    const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& entry)
{
    juce::String line;

    if (! entry.author.empty())
        line << entry.author << ": ";

    line << entry.text;
    return line;
}

[[nodiscard]] juce::Colour roomFeedEntryColour(const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& entry)
{
    switch (entry.kind)
    {
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic:
            return juce::Colour::fromRGB(230, 214, 143);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::VoteSystem:
            return juce::Colour::fromRGB(131, 198, 152);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::GenericSystem:
            return juce::Colour::fromRGB(188, 196, 210);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Presence:
            return juce::Colour::fromRGB(154, 164, 179);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat:
            break;
    }

    return juce::Colours::white;
}

[[nodiscard]] juce::String neutralVoteStatusText(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind)
{
    juce::ignoreUnused(kind);
    return {};
}

[[nodiscard]] juce::String invalidVoteStatusText(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind)
{
    return kind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm ? "Enter a BPM between 40 and 400."
                                                                      : "Enter a BPI between 2 and 64.";
}

[[nodiscard]] bool isVoteValueInRange(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind, int value)
{
    if (kind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
        return value >= 40 && value <= 400;

    return value >= 2 && value <= 64;
}

[[nodiscard]] juce::String formatServerDiscoveryStatus(
    const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& state)
{
    if (state.fetchInProgress)
        return "Refreshing public server list...";

    if (! state.statusText.empty())
        return state.statusText;

    return {};
}

[[nodiscard]] juce::String formatInlineAuthStatus(const app::session::ConnectionLifecycleSnapshot& snapshot)
{
    if (snapshot.state != app::session::ConnectionState::Error)
        return {};

    const auto reason = ! snapshot.lastError.empty() ? snapshot.lastError : snapshot.statusMessage;
    const auto kind = app::session::classifyFailure(app::session::ConnectionEventType::ConnectionFailed, reason);
    if (kind != app::session::ConnectionFailureKind::NonRetryableAuthentication)
        return {};

    if (! snapshot.lastError.empty())
        return "Authentication failed: " + snapshot.lastError;

    return "Authentication failed. Check username or password and press Connect.";
}

[[nodiscard]] bool roomFeedEntryMatches(const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& lhs,
                                        const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.author == rhs.author
        && lhs.text == rhs.text
        && lhs.subdued == rhs.subdued;
}

[[nodiscard]] bool roomFeedMatches(const std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry>& lhs,
                                   const std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (! roomFeedEntryMatches(lhs[index], rhs[index]))
            return false;
    }

    return true;
}

[[nodiscard]] bool mixerStripStateMatches(const FamaLamaJamAudioProcessorEditor::MixerStripState& lhs,
                                          const FamaLamaJamAudioProcessorEditor::MixerStripState& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.sourceId == rhs.sourceId
        && lhs.groupId == rhs.groupId
        && lhs.groupLabel == rhs.groupLabel
        && lhs.displayName == rhs.displayName
        && lhs.subtitle == rhs.subtitle
        && juce::approximatelyEqual(lhs.gainDb, rhs.gainDb)
        && juce::approximatelyEqual(lhs.pan, rhs.pan)
        && lhs.muted == rhs.muted
        && lhs.soloed == rhs.soloed
        && lhs.transmitState == rhs.transmitState
        && lhs.unsupportedVoiceMode == rhs.unsupportedVoiceMode
        && lhs.statusText == rhs.statusText
        && lhs.active == rhs.active
        && lhs.visible == rhs.visible;
}
} // namespace

FamaLamaJamAudioProcessorEditor::FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
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
                                                                 ServerDiscoveryUiGetter serverDiscoveryUiGetter,
                                                                 ServerDiscoveryRefreshHandler serverDiscoveryRefreshHandler,
                                                                 RoomUiGetter roomUiGetter,
                                                                 RoomMessageHandler roomMessageHandler,
                                                                 RoomVoteHandler roomVoteHandler,
                                                                 DiagnosticsTextGetter diagnosticsTextGetter,
                                                                 FloatGetter masterOutputGainGetter,
                                                                 FloatSetter masterOutputGainSetter,
                                                                 FloatGetter metronomeGainGetter,
                                                                 FloatSetter metronomeGainSetter,
                                                                 CommandHandler transmitToggleHandler,
                                                                 MixerStripSoloSetter mixerStripSoloSetter)
    : juce::AudioProcessorEditor(processor)
    , settingsGetter_(std::move(settingsGetter))
    , applyHandler_(std::move(applyHandler))
    , lifecycleGetter_(std::move(lifecycleGetter))
    , connectHandler_(std::move(connectHandler))
    , disconnectHandler_(std::move(disconnectHandler))
    , transportUiGetter_(std::move(transportUiGetter))
    , hostSyncAssistUiGetter_(std::move(hostSyncAssistUiGetter))
    , hostSyncAssistToggleHandler_(std::move(hostSyncAssistToggleHandler))
    , mixerStripsGetter_(std::move(mixerStripsGetter))
    , mixerStripSetter_(std::move(mixerStripSetter))
    , mixerStripSoloSetter_(mixerStripSoloSetter ? std::move(mixerStripSoloSetter) : [](const std::string&, bool) { return false; })
    , metronomeGetter_(std::move(metronomeGetter))
    , metronomeSetter_(std::move(metronomeSetter))
    , serverDiscoveryUiGetter_(serverDiscoveryUiGetter ? std::move(serverDiscoveryUiGetter)
                                                       : []() { return ServerDiscoveryUiState {}; })
    , serverDiscoveryRefreshHandler_(serverDiscoveryRefreshHandler ? std::move(serverDiscoveryRefreshHandler)
                                                                   : [](bool) { return false; })
    , roomUiGetter_(roomUiGetter ? std::move(roomUiGetter) : []() { return RoomUiState {}; })
    , roomMessageHandler_(roomMessageHandler ? std::move(roomMessageHandler) : [](std::string) { return false; })
    , roomVoteHandler_(roomVoteHandler ? std::move(roomVoteHandler) : [](RoomVoteKind, int) { return false; })
    , diagnosticsTextGetter_(diagnosticsTextGetter ? std::move(diagnosticsTextGetter) : []() { return std::string {}; })
    , masterOutputGainGetter_(masterOutputGainGetter ? std::move(masterOutputGainGetter) : []() { return 0.0f; })
    , masterOutputGainSetter_(masterOutputGainSetter ? std::move(masterOutputGainSetter) : [](float) {})
    , metronomeGainGetter_(metronomeGainGetter ? std::move(metronomeGainGetter) : []() { return 0.0f; })
    , metronomeGainSetter_(metronomeGainSetter ? std::move(metronomeGainSetter) : [](float) {})
    , transmitToggleHandler_(transmitToggleHandler ? std::move(transmitToggleHandler) : []() { return false; })
{
    titleLabel_.setText("Session", juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    serverSettingsToggle_.setButtonText(getServerSettingsToggleText());
    serverSettingsToggle_.onClick = [this]() {
        serverSettingsExpanded_ = ! serverSettingsExpanded_;
        serverSettingsToggle_.setButtonText(getServerSettingsToggleText());
        resized();
    };
    addAndMakeVisible(serverSettingsToggle_);

    serverSettingsSummaryLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(serverSettingsSummaryLabel_);

    hostLabel_.setText("Host", juce::dontSendNotification);
    portLabel_.setText("Port", juce::dontSendNotification);
    usernameLabel_.setText("Username", juce::dontSendNotification);
    passwordLabel_.setText("Password", juce::dontSendNotification);
    serverPickerLabel_.setText("Servers", juce::dontSendNotification);

    addAndMakeVisible(hostLabel_);
    addAndMakeVisible(portLabel_);
    addAndMakeVisible(usernameLabel_);
    addAndMakeVisible(passwordLabel_);
    addAndMakeVisible(serverPickerLabel_);

    serverPickerCombo_.setTextWhenNothingSelected("No servers listed yet");
    serverPickerCombo_.onChange = [this]() {
        if (refreshingServerDiscoveryUi_)
            return;

        const auto index = serverPickerCombo_.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(currentServerDiscoveryUiState_.combinedEntries.size()))
        {
            selectedServerDiscoveryEndpointKey_.clear();
            return;
        }

        const auto& entry = currentServerDiscoveryUiState_.combinedEntries[static_cast<std::size_t>(index)];
        selectedServerDiscoveryEndpointKey_ = makeServerDiscoveryEndpointKey(entry.host, entry.port);
        applyServerDiscoveryEntrySelection(entry);
    };
    addAndMakeVisible(serverPickerCombo_);

    refreshServersButton_.setButtonText("Refresh");
    refreshServersButton_.onClick = [this]() {
        if (serverDiscoveryRefreshHandler_(true))
            refreshServerDiscoveryUi();
    };
    addAndMakeVisible(refreshServersButton_);

    serverDiscoveryStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(serverDiscoveryStatusLabel_);

    addAndMakeVisible(hostEditor_);
    portEditor_.setInputRestrictions(5, "0123456789");
    addAndMakeVisible(portEditor_);
    addAndMakeVisible(usernameEditor_);
    passwordEditor_.setPasswordCharacter('*');
    passwordEditor_.onTextChange = [this]() {
        if (updatingPasswordEditor_)
            return;

        showingRememberedPasswordMask_ = false;
        recalledPassword_.clear();
    };
    addAndMakeVisible(passwordEditor_);

    metronomeToggle_.setButtonText("Metronome");
    metronomeToggle_.onClick = [this]() { metronomeSetter_(metronomeToggle_.getToggleState()); };
    addAndMakeVisible(metronomeToggle_);

    metronomeVolumeSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    metronomeVolumeSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    metronomeVolumeSlider_.setRange(-60.0, 12.0, 0.1);
    metronomeVolumeSlider_.setSkewFactorFromMidPoint(-12.0);
    metronomeVolumeSlider_.onValueChange = [this]() {
        metronomeGainSetter_(static_cast<float>(metronomeVolumeSlider_.getValue()));
    };
    addAndMakeVisible(metronomeVolumeSlider_);

    connectButton_.setButtonText("Connect");
    connectButton_.onClick = [this]() {
        const auto result = applyHandler_(makeDraftFromUi());

        if (! result.applied)
        {
            statusLabel_.setText(result.statusMessage, juce::dontSendNotification);
            return;
        }

        loadFromSettings(settingsGetter_());
        connectHandler_();
        refreshLifecycleStatus();
        refreshTransportStatus();
        refreshMixerStrips();
    };
    addAndMakeVisible(connectButton_);

    disconnectButton_.setButtonText("Disconnect");
    disconnectButton_.onClick = [this]() {
        disconnectHandler_();
        refreshLifecycleStatus();
        refreshTransportStatus();
        refreshMixerStrips();
    };
    addAndMakeVisible(disconnectButton_);

    authStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(authStatusLabel_);

    transportLabel_.setText("Interval: disconnected", juce::dontSendNotification);
    transportLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(transportLabel_);
    addAndMakeVisible(intervalProgressBar_);

    hostSyncAssistTargetLabel_.setJustificationType(juce::Justification::centredLeft);
    hostSyncAssistTargetLabel_.setVisible(false);
    addChildComponent(hostSyncAssistTargetLabel_);

    hostSyncAssistButton_.setButtonText("Arm Sync to Ableton Play");
    hostSyncAssistButton_.onClick = [this]() {
        const auto before = hostSyncAssistUiGetter_();
        (void) hostSyncAssistToggleHandler_();
        const auto after = hostSyncAssistUiGetter_();

        hostSyncAssistLastActionWasCancel_ = (before.armed || before.waitingForHost)
            && ! (after.armed || after.waitingForHost)
            && ! after.blocked && ! after.failed;

        if (after.armed || after.waitingForHost || after.blocked || after.failed)
            hostSyncAssistLastActionWasCancel_ = false;

        refreshHostSyncAssistStatus();
    };
    addAndMakeVisible(hostSyncAssistButton_);

    hostSyncAssistStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hostSyncAssistStatusLabel_);

    roomSectionLabel_.setText("Room Chat", juce::dontSendNotification);
    roomSectionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomSectionLabel_);

    roomTopicLabel_.setVisible(false);
    roomTopicValueLabel_.setVisible(false);

    roomStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomStatusLabel_);

    roomComposerLabel_.setText("Message", juce::dontSendNotification);
    addAndMakeVisible(roomComposerLabel_);

    roomComposerEditor_.setMultiLine(false);
    roomComposerEditor_.onReturnKey = [this]() { (void) submitRoomComposerMessage(); };
    addAndMakeVisible(roomComposerEditor_);

    roomSendButton_.setButtonText("Send");
    roomSendButton_.onClick = [this]() { (void) submitRoomComposerMessage(); };
    addAndMakeVisible(roomSendButton_);

    roomBpmLabel_.setText("BPM", juce::dontSendNotification);
    addAndMakeVisible(roomBpmLabel_);
    roomBpmEditor_.setInputRestrictions(3, "0123456789");
    roomBpmEditor_.onReturnKey = [this]() { (void) submitRoomVote(RoomVoteKind::Bpm); };
    roomBpmEditor_.onTextChange = [this]() { updateRoomVoteInputState(RoomVoteKind::Bpm); };
    addAndMakeVisible(roomBpmEditor_);
    roomBpmStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomBpmStatusLabel_);

    roomBpiLabel_.setText("BPI", juce::dontSendNotification);
    addAndMakeVisible(roomBpiLabel_);
    roomBpiEditor_.setInputRestrictions(2, "0123456789");
    roomBpiEditor_.onReturnKey = [this]() { (void) submitRoomVote(RoomVoteKind::Bpi); };
    roomBpiEditor_.onTextChange = [this]() { updateRoomVoteInputState(RoomVoteKind::Bpi); };
    addAndMakeVisible(roomBpiEditor_);
    roomVoteButton_.setButtonText("Vote");
    roomVoteButton_.onClick = [this]() { (void) submitRoomVote(getActiveRoomVoteKind()); };
    addAndMakeVisible(roomVoteButton_);
    roomBpiStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomBpiStatusLabel_);

    roomFeedViewport_.setViewedComponent(&roomFeedContent_, false);
    roomFeedViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(roomFeedViewport_);

    diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
    diagnosticsToggle_.onClick = [this]() {
        diagnosticsExpanded_ = ! diagnosticsExpanded_;
        diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
        if (diagnosticsExpanded_)
            refreshDiagnosticsUi();
        resized();
    };
    addAndMakeVisible(diagnosticsToggle_);

    diagnosticsEditor_.setMultiLine(true);
    diagnosticsEditor_.setReadOnly(true);
    diagnosticsEditor_.setScrollbarsShown(true);
    diagnosticsEditor_.setCaretVisible(false);
    diagnosticsEditor_.setPopupMenuEnabled(false);
    diagnosticsEditor_.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(diagnosticsEditor_);

    mixerSectionLabel_.setText("Mixer", juce::dontSendNotification);
    mixerSectionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mixerSectionLabel_);

    mixerViewport_.setViewedComponent(&mixerContent_, false);
    mixerViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(mixerViewport_);

    masterOutputLabel_.setText("Master Output", juce::dontSendNotification);
    addAndMakeVisible(masterOutputLabel_);
    masterOutputSlider_.setRange(-60.0, 12.0, 0.1);
    masterOutputSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    masterOutputSlider_.onValueChange = [this]() {
        masterOutputGainSetter_(static_cast<float>(masterOutputSlider_.getValue()));
    };
    addAndMakeVisible(masterOutputSlider_);

    statusLabel_.setText("Ready", juce::dontSendNotification);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    loadFromSettings(settingsGetter_());
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    metronomeVolumeSlider_.setValue(metronomeGainGetter_(), juce::dontSendNotification);
    (void) serverDiscoveryRefreshHandler_(false);
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshServerDiscoveryUi();
    refreshRoomUi();
    refreshDiagnosticsUi();
    refreshMixerStrips();
    startTimerHz(10);
    setSize(1080, 760);
}

FamaLamaJamAudioProcessorEditor::~FamaLamaJamAudioProcessorEditor()
{
    stopTimer();

    serverSettingsToggle_.onClick = {};
    serverPickerCombo_.onChange = {};
    refreshServersButton_.onClick = {};
    metronomeToggle_.onClick = {};
    metronomeVolumeSlider_.onValueChange = {};
    connectButton_.onClick = {};
    disconnectButton_.onClick = {};
    hostSyncAssistButton_.onClick = {};
    diagnosticsToggle_.onClick = {};
    roomSendButton_.onClick = {};
    roomVoteButton_.onClick = {};
    roomComposerEditor_.onReturnKey = {};
    roomComposerEditor_.onEscapeKey = {};
    roomComposerEditor_.onFocusLost = {};
    roomBpmEditor_.onReturnKey = {};
    roomBpmEditor_.onTextChange = {};
    roomBpiEditor_.onReturnKey = {};
    roomBpiEditor_.onTextChange = {};
    masterOutputSlider_.onValueChange = {};

    for (auto& widgets : mixerStripWidgets_)
    {
        widgets->soloToggle.onClick = {};
        widgets->gainSlider.onValueChange = {};
        widgets->panSlider.onValueChange = {};
        widgets->muteToggle.onClick = {};
        widgets->transmitButton.onClick = {};
    }
}

FamaLamaJamAudioProcessorEditor::FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
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
                                                                 DiagnosticsTextGetter diagnosticsTextGetter,
                                                                 FloatGetter masterOutputGainGetter,
                                                                 FloatSetter masterOutputGainSetter,
                                                                 FloatGetter metronomeGainGetter,
                                                                 FloatSetter metronomeGainSetter,
                                                                 CommandHandler transmitToggleHandler,
                                                                 MixerStripSoloSetter mixerStripSoloSetter)
    : FamaLamaJamAudioProcessorEditor(processor,
                                      std::move(settingsGetter),
                                      std::move(applyHandler),
                                      std::move(lifecycleGetter),
                                      std::move(connectHandler),
                                      std::move(disconnectHandler),
                                      std::move(transportUiGetter),
                                      std::move(hostSyncAssistUiGetter),
                                      std::move(hostSyncAssistToggleHandler),
                                      std::move(mixerStripsGetter),
                                      std::move(mixerStripSetter),
                                      std::move(metronomeGetter),
                                      std::move(metronomeSetter),
                                      {},
                                      {},
                                      std::move(roomUiGetter),
                                      std::move(roomMessageHandler),
                                      std::move(roomVoteHandler),
                                      std::move(diagnosticsTextGetter),
                                      std::move(masterOutputGainGetter),
                                      std::move(masterOutputGainSetter),
                                      std::move(metronomeGainGetter),
                                      std::move(metronomeGainSetter),
                                      std::move(transmitToggleHandler),
                                      std::move(mixerStripSoloSetter))
{
}

void FamaLamaJamAudioProcessorEditor::resized()
{
    ++cpuDiagnosticSnapshot_.resizedCalls;
    auto area = getLocalBounds().reduced(12);

    auto footer = area.removeFromBottom(22);
    intervalProgressBar_.setBounds(footer);
    area.removeFromBottom(8);

    titleLabel_.setVisible(false);

    constexpr int kSidebarWidth = 320;
    auto sidebar = area.removeFromRight(juce::jmin(kSidebarWidth, juce::jmax(220, area.getWidth() / 3)));
    area.removeFromRight(12);
    auto left = area;

    auto settingsRow = [&](juce::Label& label, juce::Component& editor) {
        auto current = left.removeFromTop(28);
        label.setBounds(current.removeFromLeft(96));
        editor.setBounds(current);
        left.removeFromTop(4);
    };

    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
    serverSettingsSummaryLabel_.setBounds(left.removeFromTop(22));
    transportLabel_.setVisible(transportLabel_.getText().trim().isNotEmpty());
    if (transportLabel_.isVisible())
        transportLabel_.setBounds(left.removeFromTop(20));
    left.removeFromTop(6);

    auto serverSettingsHeaderRow = left.removeFromTop(28);
    serverSettingsToggle_.setBounds(serverSettingsHeaderRow.removeFromLeft(220));
    diagnosticsToggle_.setBounds(serverSettingsHeaderRow.removeFromLeft(140));
    left.removeFromTop(2);

    serverPickerLabel_.setVisible(serverSettingsExpanded_);
    serverPickerCombo_.setVisible(serverSettingsExpanded_);
    refreshServersButton_.setVisible(serverSettingsExpanded_);
    hostLabel_.setVisible(serverSettingsExpanded_);
    hostEditor_.setVisible(serverSettingsExpanded_);
    portLabel_.setVisible(serverSettingsExpanded_);
    portEditor_.setVisible(serverSettingsExpanded_);
    usernameLabel_.setVisible(serverSettingsExpanded_);
    usernameEditor_.setVisible(serverSettingsExpanded_);
    passwordLabel_.setVisible(serverSettingsExpanded_);
    passwordEditor_.setVisible(serverSettingsExpanded_);

    const auto discoveryStatusText = serverDiscoveryStatusLabel_.getText().trim();
    const bool showDiscoveryStatus = serverSettingsExpanded_ && discoveryStatusText.isNotEmpty();
    serverDiscoveryStatusLabel_.setVisible(showDiscoveryStatus);

    if (serverSettingsExpanded_)
    {
        auto serverRow = left.removeFromTop(28);
        serverPickerLabel_.setBounds(serverRow.removeFromLeft(96));
        refreshServersButton_.setBounds(serverRow.removeFromRight(90));
        serverRow.removeFromRight(8);
        serverPickerCombo_.setBounds(serverRow);
        left.removeFromTop(4);

        if (showDiscoveryStatus)
        {
            serverDiscoveryStatusLabel_.setBounds(left.removeFromTop(22));
            left.removeFromTop(6);
        }

        settingsRow(hostLabel_, hostEditor_);
        settingsRow(portLabel_, portEditor_);
        settingsRow(usernameLabel_, usernameEditor_);
        settingsRow(passwordLabel_, passwordEditor_);
        left.removeFromTop(2);
    }

    auto controls = left.removeFromTop(34);
    connectButton_.setBounds(controls.removeFromLeft(98));
    controls.removeFromLeft(8);
    disconnectButton_.setBounds(controls.removeFromLeft(108));
    controls.removeFromLeft(8);
    hostSyncAssistButton_.setBounds(controls.removeFromLeft(190));
    controls.removeFromLeft(8);
    metronomeToggle_.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(6);
    metronomeVolumeSlider_.setBounds(controls.removeFromLeft(64).reduced(0, -4));
    left.removeFromTop(6);

    authStatusLabel_.setVisible(authStatusLabel_.getText().trim().isNotEmpty());
    if (authStatusLabel_.isVisible())
    {
        authStatusLabel_.setBounds(left.removeFromTop(22));
        left.removeFromTop(4);
    }

    statusLabel_.setVisible(statusLabel_.getText().trim().isNotEmpty());
    if (statusLabel_.isVisible())
    {
        statusLabel_.setBounds(left.removeFromTop(24));
        left.removeFromTop(4);
    }

    hostSyncAssistStatusLabel_.setVisible(hostSyncAssistStatusLabel_.getText().trim().isNotEmpty());
    if (hostSyncAssistStatusLabel_.isVisible())
    {
        hostSyncAssistStatusLabel_.setBounds(left.removeFromTop(22));
        left.removeFromTop(8);
    }
    else
    {
        left.removeFromTop(4);
    }

    mixerSectionLabel_.setVisible(false);
    mixerSectionLabel_.setBounds({ left.getX(), left.getY(), left.getWidth(), 0 });
    auto masterOutputRow = left.removeFromBottom(28);
    masterOutputLabel_.setBounds(masterOutputRow.removeFromLeft(110));
    masterOutputSlider_.setBounds(masterOutputRow);
    left.removeFromBottom(4);
    mixerViewport_.setBounds(left);

    if (diagnosticsExpanded_)
    {
        roomSectionLabel_.setVisible(false);
        roomBpmLabel_.setVisible(false);
        roomBpmEditor_.setVisible(false);
        roomBpmStatusLabel_.setVisible(false);
        roomBpiLabel_.setVisible(false);
        roomBpiEditor_.setVisible(false);
        roomVoteButton_.setVisible(false);
        roomBpiStatusLabel_.setVisible(false);
        roomStatusLabel_.setVisible(false);
        roomComposerLabel_.setVisible(false);
        roomComposerEditor_.setVisible(false);
        roomSendButton_.setVisible(false);
        roomFeedViewport_.setVisible(false);
        diagnosticsEditor_.setVisible(true);
        diagnosticsEditor_.setBounds(sidebar);
    }
    else
    {
        roomSectionLabel_.setVisible(true);
        roomBpmLabel_.setVisible(true);
        roomBpmEditor_.setVisible(true);
        roomBpmStatusLabel_.setVisible(roomBpmStatusLabel_.getText().trim().isNotEmpty());
        roomBpiLabel_.setVisible(true);
        roomBpiEditor_.setVisible(true);
        roomVoteButton_.setVisible(true);
        roomBpiStatusLabel_.setVisible(roomBpiStatusLabel_.getText().trim().isNotEmpty());
        roomStatusLabel_.setVisible(roomStatusLabel_.getText().trim().isNotEmpty());
        roomComposerLabel_.setVisible(true);
        roomComposerEditor_.setVisible(true);
        roomSendButton_.setVisible(true);
        roomFeedViewport_.setVisible(true);
        diagnosticsEditor_.setVisible(false);

        roomSectionLabel_.setBounds(sidebar.removeFromTop(24));
        sidebar.removeFromTop(4);

        auto voteArea = sidebar.removeFromTop(52);
        auto voteButtonArea = voteArea.removeFromRight(96);
        auto bpmRow = voteArea.removeFromTop(24);
        roomBpmLabel_.setBounds(bpmRow.removeFromLeft(32));
        roomBpmEditor_.setBounds(bpmRow);
        voteArea.removeFromTop(4);
        auto bpiRow = voteArea.removeFromTop(24);
        roomBpiLabel_.setBounds(bpiRow.removeFromLeft(32));
        roomBpiEditor_.setBounds(bpiRow);
        roomVoteButton_.setBounds(voteButtonArea);
        sidebar.removeFromTop(2);

        if (roomBpmStatusLabel_.isVisible())
        {
            roomBpmStatusLabel_.setBounds(sidebar.removeFromTop(20));
            sidebar.removeFromTop(2);
        }

        if (roomBpiStatusLabel_.isVisible())
        {
            roomBpiStatusLabel_.setBounds(sidebar.removeFromTop(20));
            sidebar.removeFromTop(2);
        }

        if (roomStatusLabel_.isVisible())
        {
            roomStatusLabel_.setBounds(sidebar.removeFromTop(18));
            sidebar.removeFromTop(6);
        }

        auto composerRow = sidebar.removeFromBottom(28);
        roomComposerLabel_.setBounds(composerRow.removeFromLeft(70));
        roomSendButton_.setBounds(composerRow.removeFromRight(78));
        composerRow.removeFromRight(6);
        roomComposerEditor_.setBounds(composerRow);
        sidebar.removeFromBottom(6);

        roomFeedViewport_.setBounds(sidebar);
    }

    int y = 0;
    const auto roomFeedWidth = juce::jmax(140, roomFeedViewport_.getWidth() - 18);
    for (auto& widget : roomFeedWidgets_)
    {
        juce::AttributedString text;
        text.append(widget->editor.getText(), widget->editor.getFont(), juce::Colours::white);
        juce::TextLayout layout;
        layout.createLayout(text, static_cast<float>(roomFeedWidth - 8));
        const auto height = juce::jmax(28, juce::roundToInt(layout.getHeight()) + 10);
        widget->editor.setBounds(0, y, roomFeedWidth, height);
        y += height + 6;
    }

    roomFeedContent_.setSize(roomFeedWidth, juce::jmax(96, y));

    y = 0;
    const auto mixerContentWidth = juce::jmax(280, mixerViewport_.getWidth() - 16);
    for (auto& widget : mixerStripWidgets_)
    {
        if (widget->showsGroupLabel)
        {
            widget->groupLabel.setBounds(0, y, mixerContentWidth, 20);
            y += 22;
        }

        widget->titleLabel.setBounds(0, y, mixerContentWidth / 2, 22);
        widget->subtitleLabel.setBounds((mixerContentWidth / 2) + 8, y, mixerContentWidth / 2 - 8, 22);
        y += 24;

        widget->statusLabel.setVisible(widget->statusLabel.getText().isNotEmpty());
        if (widget->statusLabel.isVisible())
        {
            widget->statusLabel.setBounds(0, y, mixerContentWidth, 20);
            y += 22;
        }

        auto controlRow = juce::Rectangle<int>(0, y, mixerContentWidth, 42);
        const auto muteWidth = 76;
        const auto soloWidth = 70;
        const auto transmitWidth = 150;
        const auto meterWidth = juce::jlimit(120, 220, mixerContentWidth / 3);
        widget->meter.setBounds(controlRow.removeFromLeft(meterWidth));
        controlRow.removeFromLeft(8);
        widget->gainSlider.setBounds(
            controlRow.removeFromLeft(juce::jmax(120,
                                                 (controlRow.getWidth()
                                                  - muteWidth
                                                  - soloWidth
                                                  - (widget->hasTransmitControl ? transmitWidth : 0)
                                                  - 24)
                                                     / 2)));
        controlRow.removeFromLeft(8);
        widget->panSlider.setBounds(
            controlRow.removeFromLeft(juce::jmax(100,
                                                 controlRow.getWidth()
                                                     - muteWidth
                                                     - soloWidth
                                                     - (widget->hasTransmitControl ? transmitWidth : 0)
                                                     - 16)));
        controlRow.removeFromLeft(8);
        widget->soloToggle.setBounds(controlRow.removeFromLeft(soloWidth));
        controlRow.removeFromLeft(8);
        widget->muteToggle.setBounds(controlRow.removeFromLeft(muteWidth));
        if (widget->hasTransmitControl)
        {
            controlRow.removeFromLeft(8);
            widget->transmitButton.setBounds(controlRow.removeFromLeft(transmitWidth));
        }
        y += 50;
    }

    mixerContent_.setSize(mixerContentWidth, y + 8);
}

void FamaLamaJamAudioProcessorEditor::timerCallback()
{
    ++cpuDiagnosticSnapshot_.timerCallbackCalls;
    ++uiRefreshTick_;
    refreshLifecycleStatus();

    const auto transport = transportUiGetter_();
    const bool beatPulse = ! lastTransportUiInitialized_
        || transport.syncHealth != lastTransportSyncHealth_
        || transport.intervalIndex != lastTransportIntervalIndex_
        || transport.currentBeat != lastTransportBeat_;

    lastTransportUiInitialized_ = true;
    lastTransportSyncHealth_ = transport.syncHealth;
    lastTransportIntervalIndex_ = transport.intervalIndex;
    lastTransportBeat_ = transport.currentBeat;

    const bool hasHealthyTiming = transport.syncHealth == SyncHealth::Healthy;

    if (beatPulse || (! hasHealthyTiming && uiRefreshTick_ % 5 == 0))
        refreshTransportStatus();

    refreshMixerStrips();

    if (beatPulse || (! hasHealthyTiming && uiRefreshTick_ % 10 == 0))
        refreshRoomUi();

    if (uiRefreshTick_ % 10 == 0)
        refreshServerDiscoveryUi();

    if (diagnosticsExpanded_ && uiRefreshTick_ % 10 == 0)
        refreshDiagnosticsUi();
}

app::session::SessionSettings FamaLamaJamAudioProcessorEditor::makeDraftFromUi() const
{
    app::session::SessionSettings draft = settingsGetter_();
    draft.serverHost = hostEditor_.getText().toStdString();
    draft.serverPort = static_cast<std::uint16_t>(portEditor_.getText().getIntValue());
    draft.username = usernameEditor_.getText().toStdString();
    if (showingRememberedPasswordMask_
        && passwordEditor_.getText() == juce::String(kRememberedPasswordMask))
    {
        draft.password = recalledPassword_;
    }
    else
    {
        draft.password = passwordEditor_.getText().toStdString();
    }

    return draft;
}

void FamaLamaJamAudioProcessorEditor::loadFromSettings(const app::session::SessionSettings& settings)
{
    hostEditor_.setText(settings.serverHost, juce::dontSendNotification);
    portEditor_.setText(juce::String(static_cast<int>(settings.serverPort)), juce::dontSendNotification);
    usernameEditor_.setText(settings.username, juce::dontSendNotification);

    if (! settings.password.empty())
        applyRememberedPassword(settings.password);
    else
        clearRememberedPassword();

    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::applyServerDiscoveryEntrySelection(const ServerDiscoveryEntry& entry)
{
    hostEditor_.setText(entry.host, juce::dontSendNotification);
    portEditor_.setText(juce::String(static_cast<int>(entry.port)), juce::dontSendNotification);
    usernameEditor_.setText(entry.username, juce::dontSendNotification);

    if (! entry.password.empty())
        applyRememberedPassword(entry.password);
    else
        clearRememberedPassword();

    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::applyRememberedPassword(const std::string& password)
{
    updatingPasswordEditor_ = true;
    recalledPassword_ = password;
    showingRememberedPasswordMask_ = ! password.empty();
    passwordEditor_.setText(showingRememberedPasswordMask_ ? juce::String(kRememberedPasswordMask) : juce::String(),
                            juce::dontSendNotification);
    updatingPasswordEditor_ = false;
}

void FamaLamaJamAudioProcessorEditor::clearRememberedPassword()
{
    updatingPasswordEditor_ = true;
    recalledPassword_.clear();
    showingRememberedPasswordMask_ = false;
    passwordEditor_.setText({}, juce::dontSendNotification);
    updatingPasswordEditor_ = false;
}

void FamaLamaJamAudioProcessorEditor::updateRoomVoteInputState(RoomVoteKind changedKind)
{
    if (updatingRoomVoteInputs_)
        return;

    auto& changedEditor = changedKind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    auto& otherEditor = changedKind == RoomVoteKind::Bpm ? roomBpiEditor_ : roomBpmEditor_;

    const auto changedText = changedEditor.getText().trim();
    if (changedText.isNotEmpty())
    {
        updatingRoomVoteInputs_ = true;
        otherEditor.setText({}, juce::dontSendNotification);
        updatingRoomVoteInputs_ = false;
        activeRoomVoteKind_ = changedKind;
        return;
    }

    const auto otherText = otherEditor.getText().trim();
    if (otherText.isNotEmpty())
        activeRoomVoteKind_ = changedKind == RoomVoteKind::Bpm ? RoomVoteKind::Bpi : RoomVoteKind::Bpm;
    else
        activeRoomVoteKind_ = changedKind;
}

void FamaLamaJamAudioProcessorEditor::updateTransmitButtonAppearance(MixerStripWidgets& widgets,
                                                                     const MixerStripState& strip)
{
    if (! widgets.hasTransmitControl)
        return;

    juce::Colour colour = juce::Colour::fromRGB(168, 72, 72);
    juce::String text = "Not transmitting";

    switch (strip.transmitState)
    {
        case TransmitState::Disabled:
            break;
        case TransmitState::WarmingUp:
            colour = juce::Colour::fromRGB(196, 152, 72);
            text = "Getting ready";
            break;
        case TransmitState::Active:
            colour = juce::Colour::fromRGB(88, 168, 102);
            text = "Transmitting";
            break;
    }

    widgets.transmitButton.setButtonText(text);
    widgets.transmitButton.setColour(juce::TextButton::buttonColourId, colour);
    widgets.transmitButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

juce::String FamaLamaJamAudioProcessorEditor::getCollapsedServerSummary() const
{
    const auto lifecycle = lifecycleGetter_();
    const auto host = hostEditor_.getText().trim();
    const auto port = portEditor_.getText().trim();
    const auto username = usernameEditor_.getText().trim();
    const auto endpoint = host.isNotEmpty() && port.isNotEmpty() ? host + ":" + port : host;

    if (lifecycle.state == app::session::ConnectionState::Active)
    {
        juce::String summary = username.isNotEmpty() ? "Connected as " + username : "Connected";
        if (endpoint.isNotEmpty())
            summary << " • " << endpoint;
        return summary;
    }

    return endpoint.isNotEmpty() ? "Disconnected from " + endpoint : "Disconnected";
}

juce::String FamaLamaJamAudioProcessorEditor::getCollapsedServerSummaryAscii() const
{
    const auto lifecycle = lifecycleGetter_();
    const auto host = hostEditor_.getText().trim();
    const auto port = portEditor_.getText().trim();
    const auto username = usernameEditor_.getText().trim();
    const auto endpoint = host.isNotEmpty() && port.isNotEmpty() ? host + ":" + port : host;

    if (lifecycle.state == app::session::ConnectionState::Active)
    {
        juce::String summary = username.isNotEmpty() ? "Connected as " + username : "Connected";
        if (endpoint.isNotEmpty())
            summary << " | " << endpoint;
        return summary;
    }

    return endpoint.isNotEmpty() ? "Disconnected from " + endpoint : "Disconnected";
}

juce::String FamaLamaJamAudioProcessorEditor::getDiagnosticsToggleText() const
{
    return diagnosticsExpanded_ ? "Hide Diagnostics" : "Show Diagnostics";
}

juce::String FamaLamaJamAudioProcessorEditor::getServerSettingsToggleText() const
{
    return serverSettingsExpanded_ ? "Server Settings (Hide)" : "Server Settings (Show)";
}

void FamaLamaJamAudioProcessorEditor::refreshLifecycleStatus()
{
    const auto snapshot = lifecycleGetter_();
    const auto status = formatLifecycleStatus(snapshot);

    statusLabel_.setText(juce::String(status), juce::dontSendNotification);
    authStatusLabel_.setText(formatInlineAuthStatus(snapshot), juce::dontSendNotification);
    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
    connectButton_.setEnabled(snapshot.canConnect());
    disconnectButton_.setEnabled(snapshot.canDisconnect());
}

void FamaLamaJamAudioProcessorEditor::refreshTransportStatus()
{
    ++cpuDiagnosticSnapshot_.transportRefreshCalls;
    const auto transport = transportUiGetter_();

    intervalProgressValue_ = (transport.syncHealth == SyncHealth::Healthy && transport.beatsPerInterval > 0)
        ? juce::jlimit(0.0, 1.0, static_cast<double>(transport.currentBeat) / transport.beatsPerInterval)
        : 0.0;
    intervalProgressBar_.setProgress(intervalProgressValue_,
                                     transport.syncHealth == SyncHealth::Healthy ? transport.beatsPerInterval : 0);

    transportLabel_.setText(formatTransportStatus(transport), juce::dontSendNotification);
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    metronomeToggle_.setEnabled(transport.metronomeAvailable);
    metronomeToggle_.setAlpha(transport.metronomeAvailable ? 1.0f : 0.65f);
    metronomeVolumeSlider_.setValue(metronomeGainGetter_(), juce::dontSendNotification);
    metronomeVolumeSlider_.setEnabled(transport.metronomeAvailable);
    metronomeVolumeSlider_.setAlpha(transport.metronomeAvailable ? 1.0f : 0.65f);
    refreshHostSyncAssistStatus();
}

void FamaLamaJamAudioProcessorEditor::refreshHostSyncAssistStatus()
{
    const auto hostSyncAssist = hostSyncAssistUiGetter_();
    if (hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.blocked || hostSyncAssist.failed)
        hostSyncAssistLastActionWasCancel_ = false;

    hostSyncAssistStatusLabel_.setText(formatHostSyncAssistStatus(hostSyncAssist, hostSyncAssistLastActionWasCancel_),
                                       juce::dontSendNotification);
    if (! (hostSyncAssist.armed || hostSyncAssist.waitingForHost)
        && hostSyncAssist.blocked
        && hostSyncAssist.blockReason == HostSyncAssistBlockReason::HostTempoMismatch
        && hostSyncAssist.targetBeatsPerMinute > 0)
    {
        hostSyncAssistButton_.setButtonText("Set DAW tempo to " + juce::String(hostSyncAssist.targetBeatsPerMinute));
    }
    else
    {
        hostSyncAssistButton_.setButtonText(hostSyncAssist.armed || hostSyncAssist.waitingForHost
                                                ? "Cancel Sync to Ableton Play"
                                                : "Arm Sync to Ableton Play");
    }

    const auto enabled = hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.armable;
    hostSyncAssistButton_.setEnabled(enabled);
    hostSyncAssistButton_.setAlpha(enabled ? 1.0f : 0.65f);
}

void FamaLamaJamAudioProcessorEditor::refreshServerDiscoveryUi()
{
    ++cpuDiagnosticSnapshot_.serverDiscoveryRefreshCalls;
    const auto nextServerDiscoveryUiState = serverDiscoveryUiGetter_();
    if (serverDiscoveryStateMatches(nextServerDiscoveryUiState, currentServerDiscoveryUiState_))
    {
        serverDiscoveryStatusLabel_.setText(formatServerDiscoveryStatus(currentServerDiscoveryUiState_),
                                            juce::dontSendNotification);
        serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
        refreshServersButton_.setEnabled(! currentServerDiscoveryUiState_.fetchInProgress);
        return;
    }

    refreshingServerDiscoveryUi_ = true;
    const auto previousSelection = serverPickerCombo_.getSelectedItemIndex();
    auto selectedEndpointKey = selectedServerDiscoveryEndpointKey_;
    if (selectedEndpointKey.empty()
        && previousSelection >= 0
        && previousSelection < static_cast<int>(currentServerDiscoveryUiState_.combinedEntries.size()))
    {
        const auto& previousEntry = currentServerDiscoveryUiState_.combinedEntries[static_cast<std::size_t>(previousSelection)];
        selectedEndpointKey = makeServerDiscoveryEndpointKey(previousEntry.host, previousEntry.port);
    }

    currentServerDiscoveryUiState_ = nextServerDiscoveryUiState;
    serverPickerCombo_.clear(juce::dontSendNotification);

    int itemId = 1;
    for (const auto& entry : currentServerDiscoveryUiState_.combinedEntries)
        serverPickerCombo_.addItem(entry.label, itemId++);

    selectedServerDiscoveryEndpointKey_.clear();
    if (! selectedEndpointKey.empty())
    {
        for (std::size_t index = 0; index < currentServerDiscoveryUiState_.combinedEntries.size(); ++index)
        {
            const auto& entry = currentServerDiscoveryUiState_.combinedEntries[index];
            if (makeServerDiscoveryEndpointKey(entry.host, entry.port) != selectedEndpointKey)
                continue;

            serverPickerCombo_.setSelectedItemIndex(static_cast<int>(index), juce::dontSendNotification);
            selectedServerDiscoveryEndpointKey_ = selectedEndpointKey;
            applyServerDiscoveryEntrySelection(entry);
            break;
        }
    }

    serverPickerCombo_.setTextWhenNothingSelected(serverPickerCombo_.getNumItems() > 0
                                                      ? "Choose a public or recent server"
                                                      : "No servers listed yet");
    serverDiscoveryStatusLabel_.setText(formatServerDiscoveryStatus(currentServerDiscoveryUiState_),
                                        juce::dontSendNotification);
    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
    refreshServersButton_.setEnabled(! currentServerDiscoveryUiState_.fetchInProgress);
    refreshingServerDiscoveryUi_ = false;
}

void FamaLamaJamAudioProcessorEditor::refreshRoomUi()
{
    ++cpuDiagnosticSnapshot_.roomRefreshCalls;
    const auto nextRoomUiState = roomUiGetter_();
    const bool feedChanged = ! roomFeedMatches(nextRoomUiState.visibleFeed, currentRoomUiState_.visibleFeed);
    const bool followRoomFeed = feedChanged && isRoomFeedNearBottom();
    currentRoomUiState_ = nextRoomUiState;

    if (feedChanged)
        rebuildRoomFeedWidgets(currentRoomUiState_.visibleFeed);

    roomStatusLabel_.setText(currentRoomUiState_.connected ? "Room activity" : juce::String {},
                             juce::dontSendNotification);

    const auto canInteract = currentRoomUiState_.connected;
    roomComposerEditor_.setEnabled(canInteract);
    roomSendButton_.setEnabled(canInteract);
    roomBpmEditor_.setEnabled(canInteract);
    roomBpiEditor_.setEnabled(canInteract);
    roomVoteButton_.setEnabled(canInteract);

    auto applyVoteFeedback = [&](RoomVoteKind kind,
                                 const RoomVoteUiState& state,
                                 RoomVoteFeedbackState& feedbackState,
                                 juce::Label& statusLabel) {
        const auto neutralText = neutralVoteStatusText(kind);

        if (state.pending)
        {
            feedbackState.transientFailureTicks = 0;
            feedbackState.transientFailureText = {};
            statusLabel.setText(state.statusText.empty() ? "Vote pending" : state.statusText, juce::dontSendNotification);
        }
        else if (state.failed)
        {
            if (! feedbackState.lastFailed || feedbackState.lastStatusText != state.statusText
                || feedbackState.lastRequestedValue != state.requestedValue)
            {
                feedbackState.transientFailureText = state.statusText.empty() ? "Vote failed" : juce::String(state.statusText);
                feedbackState.transientFailureTicks = 24;
            }

            if (feedbackState.transientFailureTicks > 0)
            {
                statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
                --feedbackState.transientFailureTicks;
            }
            else
            {
                statusLabel.setText(neutralText, juce::dontSendNotification);
            }
        }
        else
        {
            feedbackState.transientFailureTicks = 0;
            feedbackState.transientFailureText = {};
            statusLabel.setText(neutralText, juce::dontSendNotification);
        }

        feedbackState.lastPending = state.pending;
        feedbackState.lastFailed = state.failed;
        feedbackState.lastRequestedValue = state.requestedValue;
        feedbackState.lastStatusText = state.statusText;
    };

    applyVoteFeedback(RoomVoteKind::Bpm, currentRoomUiState_.bpmVote, roomBpmFeedbackState_, roomBpmStatusLabel_);
    applyVoteFeedback(RoomVoteKind::Bpi, currentRoomUiState_.bpiVote, roomBpiFeedbackState_, roomBpiStatusLabel_);

    if (feedChanged && followRoomFeed)
        scrollRoomFeedToBottom();
}

void FamaLamaJamAudioProcessorEditor::refreshDiagnosticsUi()
{
    ++cpuDiagnosticSnapshot_.diagnosticsRefreshCalls;
    if (! diagnosticsExpanded_)
        return;

    const auto nextDiagnosticsText = diagnosticsTextGetter_();
    if (nextDiagnosticsText != lastDiagnosticsText_)
    {
        ++cpuDiagnosticSnapshot_.diagnosticsDocumentUpdateCalls;
        lastDiagnosticsText_ = nextDiagnosticsText;
        diagnosticsEditor_.setText(juce::String(nextDiagnosticsText), juce::dontSendNotification);
    }

    diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
}

void FamaLamaJamAudioProcessorEditor::rebuildRoomFeedWidgets(const std::vector<RoomFeedEntry>& entries)
{
    ++cpuDiagnosticSnapshot_.roomFeedRebuildCalls;
    roomFeedWidgets_.clear();

    if (entries.empty())
    {
        ++cpuDiagnosticSnapshot_.roomFeedEntryWidgetsBuilt;
        auto widgets = std::make_unique<RoomFeedEntryWidgets>();
        widgets->editor.setMultiLine(true);
        widgets->editor.setReadOnly(true);
        widgets->editor.setScrollbarsShown(false);
        widgets->editor.setCaretVisible(false);
        widgets->editor.setPopupMenuEnabled(false);
        widgets->editor.setText("No room activity yet.", juce::dontSendNotification);
        widgets->editor.setAlpha(0.75f);
        roomFeedContent_.addAndMakeVisible(widgets->editor);
        roomFeedWidgets_.push_back(std::move(widgets));
        resized();
        return;
    }

    for (const auto& entry : entries)
    {
        auto widgets = std::make_unique<RoomFeedEntryWidgets>();
        widgets->editor.setMultiLine(true);
        widgets->editor.setReadOnly(true);
        widgets->editor.setScrollbarsShown(false);
        widgets->editor.setCaretVisible(false);
        widgets->editor.setPopupMenuEnabled(false);
        widgets->editor.setText(formatRoomFeedEntryText(entry), juce::dontSendNotification);
        widgets->editor.setColour(juce::TextEditor::textColourId, roomFeedEntryColour(entry));
        widgets->editor.setAlpha(entry.subdued ? 0.7f : 1.0f);
        roomFeedContent_.addAndMakeVisible(widgets->editor);
        roomFeedWidgets_.push_back(std::move(widgets));
    }

    cpuDiagnosticSnapshot_.roomFeedEntryWidgetsBuilt += entries.size();

    resized();
}

bool FamaLamaJamAudioProcessorEditor::submitRoomComposerMessage()
{
    if (! currentRoomUiState_.connected)
        return false;

    const auto message = roomComposerEditor_.getText().trim();
    if (message.isEmpty())
        return false;

    if (! roomMessageHandler_(message.toStdString()))
        return false;

    roomComposerEditor_.clear();
    refreshRoomUi();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::submitRoomVote(RoomVoteKind kind)
{
    auto& editor = kind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    auto& statusLabel = kind == RoomVoteKind::Bpm ? roomBpmStatusLabel_ : roomBpiStatusLabel_;
    auto& feedbackState = kind == RoomVoteKind::Bpm ? roomBpmFeedbackState_ : roomBpiFeedbackState_;

    if (! currentRoomUiState_.connected)
        return false;

    const auto value = editor.getText().trim().getIntValue();
    if (! isVoteValueInRange(kind, value))
    {
        feedbackState.transientFailureText = invalidVoteStatusText(kind);
        feedbackState.transientFailureTicks = 24;
        statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
        return false;
    }

    if (! roomVoteHandler_(kind, value))
    {
        feedbackState.transientFailureText = "Couldn't send vote. Try again.";
        feedbackState.transientFailureTicks = 24;
        statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
        return false;
    }

    refreshRoomUi();
    return true;
}

FamaLamaJamAudioProcessorEditor::RoomVoteKind FamaLamaJamAudioProcessorEditor::getActiveRoomVoteKind() const noexcept
{
    if (roomBpiEditor_.getText().trim().isNotEmpty())
        return RoomVoteKind::Bpi;

    if (roomBpmEditor_.getText().trim().isNotEmpty())
        return RoomVoteKind::Bpm;

    return activeRoomVoteKind_;
}

bool FamaLamaJamAudioProcessorEditor::isRoomFeedNearBottom() const noexcept
{
    const auto* viewed = roomFeedViewport_.getViewedComponent();
    if (viewed == nullptr)
        return true;

    const auto maxScrollY = juce::jmax(0, viewed->getHeight() - roomFeedViewport_.getHeight());
    return roomFeedViewport_.getViewPositionY() >= juce::jmax(0, maxScrollY - 24);
}

void FamaLamaJamAudioProcessorEditor::scrollRoomFeedToBottom()
{
    const auto* viewed = roomFeedViewport_.getViewedComponent();
    if (viewed == nullptr)
        return;

    const auto maxScrollY = juce::jmax(0, viewed->getHeight() - roomFeedViewport_.getHeight());
    roomFeedViewport_.setViewPosition(0, maxScrollY);
}

void FamaLamaJamAudioProcessorEditor::refreshMixerStrips()
{
    ++cpuDiagnosticSnapshot_.mixerRefreshCalls;
    const auto allStrips = mixerStripsGetter_();
    std::vector<MixerStripState> visibleStrips;
    visibleStrips.reserve(allStrips.size());

    for (const auto& strip : allStrips)
    {
        if (strip.visible)
            visibleStrips.push_back(strip);
    }

    std::vector<std::string> nextOrder;
    nextOrder.reserve(visibleStrips.size());
    for (const auto& strip : visibleStrips)
        nextOrder.push_back(strip.sourceId);

    if (nextOrder != visibleMixerStripOrder_)
        rebuildMixerStripWidgets(visibleStrips);

    for (std::size_t index = 0; index < visibleStrips.size() && index < mixerStripWidgets_.size(); ++index)
    {
        const auto& strip = visibleStrips[index];
        auto& widgets = *mixerStripWidgets_[index];
        const bool meterChanged = ! juce::approximatelyEqual(strip.meterLeft, widgets.lastMeterLeft)
            || ! juce::approximatelyEqual(strip.meterRight, widgets.lastMeterRight);

        if (meterChanged)
        {
            ++cpuDiagnosticSnapshot_.mixerMeterUpdateCalls;
            widgets.lastMeterLeft = strip.meterLeft;
            widgets.lastMeterRight = strip.meterRight;
            widgets.meter.setLevels(strip.meterLeft, strip.meterRight);
        }

        const bool stripChanged = index >= currentVisibleMixerStrips_.size()
            || ! mixerStripStateMatches(strip, currentVisibleMixerStrips_[index]);
        if (! stripChanged)
            continue;

        ++cpuDiagnosticSnapshot_.mixerStripUpdateCalls;

        widgets.groupLabel.setText(strip.groupLabel, juce::dontSendNotification);
        widgets.titleLabel.setText(strip.displayName, juce::dontSendNotification);
        widgets.subtitleLabel.setText(strip.subtitle, juce::dontSendNotification);
        widgets.titleLabel.setColour(juce::Label::textColourId, stripAccentColour(strip.kind, strip.active));
        widgets.subtitleLabel.setAlpha(strip.active ? 1.0f : 0.7f);
        widgets.statusLabel.setText(strip.statusText, juce::dontSendNotification);
        widgets.statusLabel.setColour(juce::Label::textColourId,
                                      strip.unsupportedVoiceMode ? juce::Colour::fromRGB(214, 141, 72)
                                                                 : juce::Colour::fromRGB(188, 196, 210));
        widgets.statusLabel.setAlpha(strip.statusText.empty() ? 0.0f : 1.0f);

        if (! widgets.gainSlider.isMouseButtonDown())
            widgets.gainSlider.setValue(strip.gainDb, juce::dontSendNotification);
        if (! widgets.panSlider.isMouseButtonDown())
            widgets.panSlider.setValue(strip.pan, juce::dontSendNotification);
        widgets.muteToggle.setToggleState(strip.muted, juce::dontSendNotification);
        widgets.soloToggle.setToggleState(strip.soloed, juce::dontSendNotification);
        widgets.soloToggle.setButtonText(strip.soloed ? "Soloed" : "Solo");
        widgets.soloToggle.setAlpha(strip.active || strip.kind == MixerStripKind::LocalMonitor ? 1.0f : 0.7f);
        widgets.hasTransmitControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets.transmitButton.setVisible(widgets.hasTransmitControl);
        updateTransmitButtonAppearance(widgets, strip);
    }

    if (! masterOutputSlider_.isMouseButtonDown())
        masterOutputSlider_.setValue(masterOutputGainGetter_(), juce::dontSendNotification);

    currentVisibleMixerStrips_ = visibleStrips;
}

void FamaLamaJamAudioProcessorEditor::rebuildMixerStripWidgets(const std::vector<MixerStripState>& visibleStrips)
{
    cpuDiagnosticSnapshot_.mixerStripWidgetBuildCount += visibleStrips.size();

    for (auto& widgets : mixerStripWidgets_)
    {
        widgets->soloToggle.onClick = {};
        widgets->gainSlider.onValueChange = {};
        widgets->panSlider.onValueChange = {};
        widgets->muteToggle.onClick = {};
        widgets->transmitButton.onClick = {};
    }

    mixerStripWidgets_.clear();
    visibleMixerStripOrder_.clear();
    currentVisibleMixerStrips_.clear();

    std::string previousGroupId;
    for (const auto& strip : visibleStrips)
    {
        auto widgets = std::make_unique<MixerStripWidgets>();
        widgets->sourceId = strip.sourceId;
        widgets->groupId = strip.groupId;
        widgets->showsGroupLabel = previousGroupId != strip.groupId
                                    && strip.groupLabel != strip.displayName;
        previousGroupId = strip.groupId;

        widgets->groupLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->groupLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        mixerContent_.addAndMakeVisible(widgets->groupLabel);
        widgets->groupLabel.setVisible(widgets->showsGroupLabel);

        widgets->titleLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->titleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        mixerContent_.addAndMakeVisible(widgets->titleLabel);

        widgets->subtitleLabel.setJustificationType(juce::Justification::centredLeft);
        mixerContent_.addAndMakeVisible(widgets->subtitleLabel);

        widgets->statusLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->statusLabel.setFont(juce::FontOptions(13.0f));
        mixerContent_.addAndMakeVisible(widgets->statusLabel);

        mixerContent_.addAndMakeVisible(widgets->meter);

        widgets->gainSlider.setRange(-60.0, 12.0, 0.1);
        widgets->gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        widgets->gainSlider.onValueChange = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->gainSlider);

        widgets->panSlider.setRange(-1.0, 1.0, 0.01);
        widgets->panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        widgets->panSlider.onValueChange = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->panSlider);

        widgets->muteToggle.setButtonText("Mute");
        widgets->muteToggle.onClick = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->muteToggle);

        widgets->soloToggle.setClickingTogglesState(true);
        widgets->soloToggle.setButtonText("Solo");
        widgets->soloToggle.onClick = [this, raw = widgets.get()] {
            const auto targetState = raw->soloToggle.getToggleState();
            if (! mixerStripSoloSetter_(raw->sourceId, targetState))
                raw->soloToggle.setToggleState(! targetState, juce::dontSendNotification);
        };
        mixerContent_.addAndMakeVisible(widgets->soloToggle);

        widgets->hasTransmitControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets->transmitButton.onClick = [this]() {
            (void) transmitToggleHandler_();
            refreshMixerStrips();
        };
        widgets->transmitButton.setVisible(widgets->hasTransmitControl);
        widgets->lastMeterLeft = strip.meterLeft;
        widgets->lastMeterRight = strip.meterRight;
        widgets->meter.setLevels(strip.meterLeft, strip.meterRight);
        mixerContent_.addAndMakeVisible(widgets->transmitButton);

        visibleMixerStripOrder_.push_back(strip.sourceId);
        mixerStripWidgets_.push_back(std::move(widgets));
    }

    resized();
}

juce::String FamaLamaJamAudioProcessorEditor::getTransportStatusTextForTesting() const
{
    return transportLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostSyncAssistStatusTextForTesting() const
{
    return hostSyncAssistStatusLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostSyncAssistButtonTextForTesting() const
{
    return hostSyncAssistButton_.getButtonText();
}

bool FamaLamaJamAudioProcessorEditor::isHostSyncAssistEnabledForTesting() const noexcept
{
    return hostSyncAssistButton_.isEnabled();
}

double FamaLamaJamAudioProcessorEditor::getIntervalProgressForTesting() const noexcept
{
    return intervalProgressBar_.getProgressForTesting();
}

int FamaLamaJamAudioProcessorEditor::getIntervalBeatDivisionsForTesting() const noexcept
{
    return intervalProgressBar_.getBeatDivisionsForTesting();
}

bool FamaLamaJamAudioProcessorEditor::isMetronomeToggleEnabledForTesting() const noexcept
{
    return metronomeToggle_.isEnabled();
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomTopicTextForTesting() const
{
    return roomTopicValueLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomStatusTextForTesting() const
{
    return roomStatusLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getServerDiscoveryStatusTextForTesting() const
{
    return serverDiscoveryStatusLabel_.getText();
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleServerDiscoveryLabelsForTesting() const
{
    std::vector<juce::String> labels;
    labels.reserve(currentServerDiscoveryUiState_.combinedEntries.size());

    for (const auto& entry : currentServerDiscoveryUiState_.combinedEntries)
        labels.push_back(entry.label);

    return labels;
}

juce::String FamaLamaJamAudioProcessorEditor::getSelectedServerDiscoveryLabelForTesting() const
{
    return serverPickerCombo_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostTextForTesting() const
{
    return hostEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getPortTextForTesting() const
{
    return portEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getUsernameTextForTesting() const
{
    return usernameEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getPasswordTextForTesting() const
{
    return passwordEditor_.getText();
}

std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry> FamaLamaJamAudioProcessorEditor::getVisibleRoomFeedForTesting() const
{
    return currentRoomUiState_.visibleFeed;
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomVoteStatusTextForTesting(RoomVoteKind kind) const
{
    return kind == RoomVoteKind::Bpm ? roomBpmStatusLabel_.getText() : roomBpiStatusLabel_.getText();
}

bool FamaLamaJamAudioProcessorEditor::isRoomComposerEnabledForTesting() const noexcept
{
    return roomComposerEditor_.isEnabled() && roomSendButton_.isEnabled();
}

bool FamaLamaJamAudioProcessorEditor::isRoomVoteEnabledForTesting(RoomVoteKind kind) const noexcept
{
    juce::ignoreUnused(kind);
    return roomVoteButton_.isEnabled();
}

bool FamaLamaJamAudioProcessorEditor::isRoomSectionAboveMixerForTesting() const noexcept
{
    return roomSectionLabel_.getX() <= mixerSectionLabel_.getX()
        && roomSectionLabel_.getBottom() < mixerSectionLabel_.getY();
}

bool FamaLamaJamAudioProcessorEditor::hasRoomFeedViewportForTesting() const noexcept
{
    return roomFeedViewport_.isVisible() && roomFeedViewport_.getViewedComponent() == &roomFeedContent_;
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomComposerTextForTesting() const
{
    return roomComposerEditor_.getText();
}

void FamaLamaJamAudioProcessorEditor::setRoomComposerTextForTesting(const juce::String& text)
{
    roomComposerEditor_.setText(text, juce::dontSendNotification);
}

bool FamaLamaJamAudioProcessorEditor::selectServerDiscoveryEntryForTesting(int index)
{
    if (index < 0 || index >= serverPickerCombo_.getNumItems())
        return false;

    serverPickerCombo_.setSelectedItemIndex(index, juce::sendNotificationSync);
    return true;
}

bool FamaLamaJamAudioProcessorEditor::clickServerDiscoveryRefreshForTesting()
{
    if (refreshServersButton_.onClick == nullptr)
        return false;

    refreshServersButton_.onClick();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::submitRoomComposerForTesting(bool useReturnKey)
{
    if (useReturnKey)
    {
        if (roomComposerEditor_.onReturnKey == nullptr)
            return false;

        roomComposerEditor_.onReturnKey();
        return true;
    }

    return submitRoomComposerMessage();
}

void FamaLamaJamAudioProcessorEditor::setRoomVoteValueForTesting(RoomVoteKind kind, int value)
{
    auto& editor = kind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    editor.setText(juce::String(value), juce::dontSendNotification);
    updateRoomVoteInputState(kind);
}

bool FamaLamaJamAudioProcessorEditor::submitRoomVoteForTesting(RoomVoteKind kind)
{
    activeRoomVoteKind_ = kind;
    return submitRoomVote(kind);
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleMixerGroupLabelsForTesting() const
{
    std::vector<juce::String> labels;
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->showsGroupLabel)
            labels.push_back(widgets->groupLabel.getText());
    }
    return labels;
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleMixerStripLabelsForTesting() const
{
    std::vector<juce::String> labels;
    for (const auto& widgets : mixerStripWidgets_)
        labels.push_back(widgets->titleLabel.getText());
    return labels;
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripStatusTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
            return widgets->statusLabel.getText();
    }

    return {};
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripTransmitButtonTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasTransmitControl)
            return widgets->transmitButton.getButtonText();
    }

    return {};
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripSoloStateForTesting(const juce::String& sourceId, bool& soloed) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            soloed = widgets->soloToggle.getToggleState();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripControlStateForTesting(const juce::String& sourceId,
                                                                          double& gain,
                                                                          double& pan,
                                                                          bool& muted) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            gain = widgets->gainSlider.getValue();
            pan = widgets->panSlider.getValue();
            muted = widgets->muteToggle.getToggleState();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripMeterLevelsForTesting(const juce::String& sourceId,
                                                                         float& left,
                                                                         float& right) const
{
    const auto source = sourceId.toStdString();
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId != source)
            continue;

        left = widgets->meter.getLeftLevelForTesting();
        right = widgets->meter.getRightLevelForTesting();
        return true;
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::setMixerStripControlStateForTesting(const juce::String& sourceId,
                                                                          double gain,
                                                                          double pan,
                                                                          bool muted)
{
    if (! mixerStripSetter_(sourceId.toStdString(), static_cast<float>(gain), static_cast<float>(pan), muted))
        return false;

    refreshMixerStrips();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::clickMixerStripTransmitForTesting(const juce::String& sourceId)
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasTransmitControl)
        {
            if (widgets->transmitButton.onClick == nullptr)
                return false;

            widgets->transmitButton.onClick();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::setMixerStripSoloStateForTesting(const juce::String& sourceId, bool soloed)
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            widgets->soloToggle.setToggleState(soloed, juce::dontSendNotification);
            if (widgets->soloToggle.onClick == nullptr)
                return false;

            widgets->soloToggle.onClick();
            return true;
        }
    }

    return false;
}

FamaLamaJamAudioProcessorEditor::CpuDiagnosticSnapshot
FamaLamaJamAudioProcessorEditor::getCpuDiagnosticSnapshotForTesting() const noexcept
{
    return cpuDiagnosticSnapshot_;
}

void FamaLamaJamAudioProcessorEditor::resetCpuDiagnosticSnapshotForTesting() noexcept
{
    cpuDiagnosticSnapshot_ = {};
}

juce::String FamaLamaJamAudioProcessorEditor::getDiagnosticsTextForTesting() const
{
    return diagnosticsEditor_.getText();
}

bool FamaLamaJamAudioProcessorEditor::isDiagnosticsExpandedForTesting() const noexcept
{
    return diagnosticsExpanded_;
}

juce::String FamaLamaJamAudioProcessorEditor::getServerSettingsSummaryForTesting() const
{
    return serverSettingsSummaryLabel_.getText();
}

void FamaLamaJamAudioProcessorEditor::setSettingsDraftForTesting(const app::session::SessionSettings& settings)
{
    loadFromSettings(settings);
}

void FamaLamaJamAudioProcessorEditor::setPasswordTextForTesting(const juce::String& text)
{
    passwordEditor_.setText(text, juce::dontSendNotification);
    if (passwordEditor_.onTextChange != nullptr)
        passwordEditor_.onTextChange();
}

void FamaLamaJamAudioProcessorEditor::clickConnectForTesting()
{
    if (connectButton_.onClick != nullptr)
        connectButton_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickHostSyncAssistForTesting()
{
    if (hostSyncAssistButton_.onClick != nullptr)
        hostSyncAssistButton_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickDiagnosticsToggleForTesting()
{
    if (diagnosticsToggle_.onClick != nullptr)
        diagnosticsToggle_.onClick();
}

void FamaLamaJamAudioProcessorEditor::runTimerTickForTesting()
{
    timerCallback();
}

void FamaLamaJamAudioProcessorEditor::refreshForTesting()
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshServerDiscoveryUi();
    refreshRoomUi();
    refreshDiagnosticsUi();
    refreshMixerStrips();
}
} // namespace famalamajam::plugin
