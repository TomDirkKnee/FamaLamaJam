#include "FamaLamaJamAudioProcessorEditor.h"

#include <algorithm>

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
            if (! snapshot.statusMessage.empty())
                return snapshot.statusMessage;
            return "Connected";

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
    switch (state.syncHealth)
    {
        case FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected:
            return "Disconnected from interval timing.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming:
            return "Waiting for interval timing. Start when the beat appears.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Reconnecting:
            return "Interval timing paused while reconnecting.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost:
            return "Interval timing lost. Wait for timing or reconnect.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy:
            break;
    }

    const auto beat = juce::jlimit(1, state.beatsPerInterval, state.currentBeat > 0 ? state.currentBeat : 1);
    const auto progress = juce::jlimit(0.0f, 1.0f, state.intervalProgress);
    const auto progressPercent = juce::roundToInt(progress * 100.0f);

    return "Beat " + std::to_string(beat) + "/" + std::to_string(state.beatsPerInterval)
         + " | " + std::to_string(state.beatsPerMinute) + " BPM"
         + " | " + std::to_string(progressPercent) + "%";
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
                if (state.targetBeatsPerMinute > 0)
                    return "Set Ableton to " + std::to_string(state.targetBeatsPerMinute)
                         + " BPM to arm this room sync.";
                return "Set Ableton tempo to match the room before arming sync assist.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::None:
                break;
        }
    }

    if (lastActionWasCancel)
        return "Sync assist canceled. Arm again when you want Ableton Play to start aligned.";

    if (state.targetBeatsPerMinute > 0 && state.targetBeatsPerInterval > 0)
    {
        return "Ready for " + std::to_string(state.targetBeatsPerMinute) + " BPM / "
             + std::to_string(state.targetBeatsPerInterval)
             + " BPI room timing. Arm sync when Ableton is stopped.";
    }

    return "Sync assist will be ready when room timing appears.";
}

[[nodiscard]] juce::Colour stripAccentColour(FamaLamaJamAudioProcessorEditor::MixerStripKind kind, bool active)
{
    if (kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
        return active ? juce::Colour::fromRGB(214, 141, 72) : juce::Colour::fromRGB(132, 102, 74);

    return active ? juce::Colour::fromRGB(80, 150, 204) : juce::Colour::fromRGB(78, 90, 110);
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
                                                                 BoolSetter metronomeSetter)
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
    , metronomeGetter_(std::move(metronomeGetter))
    , metronomeSetter_(std::move(metronomeSetter))
{
    titleLabel_.setText("FamaLamaJam Session Settings", juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    hostLabel_.setText("Host", juce::dontSendNotification);
    portLabel_.setText("Port", juce::dontSendNotification);
    usernameLabel_.setText("Username", juce::dontSendNotification);
    gainLabel_.setText("Default Gain (dB)", juce::dontSendNotification);
    panLabel_.setText("Default Pan", juce::dontSendNotification);

    addAndMakeVisible(hostLabel_);
    addAndMakeVisible(portLabel_);
    addAndMakeVisible(usernameLabel_);
    addAndMakeVisible(gainLabel_);
    addAndMakeVisible(panLabel_);

    addAndMakeVisible(hostEditor_);
    portEditor_.setInputRestrictions(5, "0123456789");
    addAndMakeVisible(portEditor_);
    addAndMakeVisible(usernameEditor_);

    gainSlider_.setRange(-60.0, 12.0, 0.1);
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    addAndMakeVisible(gainSlider_);

    panSlider_.setRange(-1.0, 1.0, 0.01);
    panSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    addAndMakeVisible(panSlider_);

    muteToggle_.setButtonText("Default Muted");
    addAndMakeVisible(muteToggle_);

    metronomeToggle_.setButtonText("Metronome");
    metronomeToggle_.onClick = [this]() { metronomeSetter_(metronomeToggle_.getToggleState()); };
    addAndMakeVisible(metronomeToggle_);

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

    transportLabel_.setText("Interval: disconnected", juce::dontSendNotification);
    transportLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(transportLabel_);
    addAndMakeVisible(intervalProgressBar_);

    hostSyncAssistTargetLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hostSyncAssistTargetLabel_);

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

    mixerSectionLabel_.setText("Mixer", juce::dontSendNotification);
    mixerSectionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mixerSectionLabel_);

    mixerViewport_.setViewedComponent(&mixerContent_, false);
    mixerViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(mixerViewport_);

    statusLabel_.setText("Ready", juce::dontSendNotification);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    loadFromSettings(settingsGetter_());
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshMixerStrips();
    startTimerHz(20);
    setSize(760, 760);
}

void FamaLamaJamAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    titleLabel_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    auto row = [&](juce::Label& label, juce::Component& editor) {
        auto current = area.removeFromTop(28);
        label.setBounds(current.removeFromLeft(160));
        editor.setBounds(current);
        area.removeFromTop(4);
    };

    row(hostLabel_, hostEditor_);
    row(portLabel_, portEditor_);
    row(usernameLabel_, usernameEditor_);
    row(gainLabel_, gainSlider_);
    row(panLabel_, panSlider_);

    auto toggles = area.removeFromTop(28);
    muteToggle_.setBounds(toggles.removeFromLeft(170));
    toggles.removeFromLeft(12);
    metronomeToggle_.setBounds(toggles.removeFromLeft(220));
    area.removeFromTop(8);

    statusLabel_.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    transportLabel_.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    intervalProgressBar_.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    auto syncAssistRow = area.removeFromTop(28);
    hostSyncAssistTargetLabel_.setBounds(syncAssistRow.removeFromLeft(310));
    syncAssistRow.removeFromLeft(12);
    hostSyncAssistButton_.setBounds(syncAssistRow.removeFromLeft(220));
    area.removeFromTop(4);
    hostSyncAssistStatusLabel_.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);

    auto controls = area.removeFromTop(32);
    connectButton_.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(8);
    disconnectButton_.setBounds(controls.removeFromLeft(120));

    area.removeFromTop(10);
    mixerSectionLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);

    mixerViewport_.setBounds(area);

    int y = 0;
    for (auto& widget : mixerStripWidgets_)
    {
        if (widget->showsGroupLabel)
        {
            widget->groupLabel.setBounds(0, y, mixerViewport_.getWidth() - 24, 20);
            y += 22;
        }

        widget->titleLabel.setBounds(0, y, 280, 22);
        widget->subtitleLabel.setBounds(286, y, 250, 22);
        y += 24;

        widget->meter.setBounds(0, y, 250, 42);
        widget->gainSlider.setBounds(260, y, 210, 42);
        widget->panSlider.setBounds(478, y, 180, 42);
        widget->muteToggle.setBounds(666, y + 10, 90, 22);
        y += 50;
    }

    mixerContent_.setSize(juce::jmax(120, mixerViewport_.getWidth() - 16), y + 8);
}

void FamaLamaJamAudioProcessorEditor::timerCallback()
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshMixerStrips();
}

app::session::SessionSettings FamaLamaJamAudioProcessorEditor::makeDraftFromUi() const
{
    app::session::SessionSettings draft = settingsGetter_();
    draft.serverHost = hostEditor_.getText().toStdString();
    draft.serverPort = static_cast<std::uint16_t>(portEditor_.getText().getIntValue());
    draft.username = usernameEditor_.getText().toStdString();
    draft.defaultChannelGainDb = static_cast<float>(gainSlider_.getValue());
    draft.defaultChannelPan = static_cast<float>(panSlider_.getValue());
    draft.defaultChannelMuted = muteToggle_.getToggleState();
    return draft;
}

void FamaLamaJamAudioProcessorEditor::loadFromSettings(const app::session::SessionSettings& settings)
{
    hostEditor_.setText(settings.serverHost, juce::dontSendNotification);
    portEditor_.setText(juce::String(static_cast<int>(settings.serverPort)), juce::dontSendNotification);
    usernameEditor_.setText(settings.username, juce::dontSendNotification);
    gainSlider_.setValue(settings.defaultChannelGainDb, juce::dontSendNotification);
    panSlider_.setValue(settings.defaultChannelPan, juce::dontSendNotification);
    muteToggle_.setToggleState(settings.defaultChannelMuted, juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::refreshLifecycleStatus()
{
    const auto snapshot = lifecycleGetter_();
    const auto status = formatLifecycleStatus(snapshot);

    statusLabel_.setText(juce::String(status), juce::dontSendNotification);
    connectButton_.setEnabled(snapshot.canConnect());
    disconnectButton_.setEnabled(snapshot.canDisconnect());
}

void FamaLamaJamAudioProcessorEditor::refreshTransportStatus()
{
    const auto transport = transportUiGetter_();

    intervalProgressValue_ = (transport.syncHealth == SyncHealth::Healthy)
        ? juce::jlimit(0.0, 1.0, static_cast<double>(transport.intervalProgress))
        : 0.0;
    intervalProgressBar_.setProgress(intervalProgressValue_,
                                     transport.syncHealth == SyncHealth::Healthy ? transport.beatsPerInterval : 0);

    transportLabel_.setText(formatTransportStatus(transport), juce::dontSendNotification);
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    metronomeToggle_.setEnabled(transport.metronomeAvailable);
    metronomeToggle_.setAlpha(transport.metronomeAvailable ? 1.0f : 0.65f);
    refreshHostSyncAssistStatus();
}

void FamaLamaJamAudioProcessorEditor::refreshHostSyncAssistStatus()
{
    const auto hostSyncAssist = hostSyncAssistUiGetter_();
    if (hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.blocked || hostSyncAssist.failed)
        hostSyncAssistLastActionWasCancel_ = false;

    hostSyncAssistTargetLabel_.setText(formatHostSyncAssistTarget(hostSyncAssist), juce::dontSendNotification);
    hostSyncAssistStatusLabel_.setText(formatHostSyncAssistStatus(hostSyncAssist, hostSyncAssistLastActionWasCancel_),
                                       juce::dontSendNotification);
    hostSyncAssistButton_.setButtonText(hostSyncAssist.armed || hostSyncAssist.waitingForHost
                                            ? "Cancel Sync to Ableton Play"
                                            : "Arm Sync to Ableton Play");

    const auto enabled = hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.armable;
    hostSyncAssistButton_.setEnabled(enabled);
    hostSyncAssistButton_.setAlpha(enabled ? 1.0f : 0.65f);
}

void FamaLamaJamAudioProcessorEditor::refreshMixerStrips()
{
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

        widgets.groupLabel.setText(strip.groupLabel, juce::dontSendNotification);
        widgets.titleLabel.setText(strip.displayName, juce::dontSendNotification);
        widgets.subtitleLabel.setText(strip.subtitle, juce::dontSendNotification);
        widgets.titleLabel.setColour(juce::Label::textColourId, stripAccentColour(strip.kind, strip.active));
        widgets.subtitleLabel.setAlpha(strip.active ? 1.0f : 0.7f);
        widgets.meter.setLevels(strip.meterLeft, strip.meterRight);

        if (! widgets.gainSlider.isMouseButtonDown())
            widgets.gainSlider.setValue(strip.gainDb, juce::dontSendNotification);
        if (! widgets.panSlider.isMouseButtonDown())
            widgets.panSlider.setValue(strip.pan, juce::dontSendNotification);
        widgets.muteToggle.setToggleState(strip.muted, juce::dontSendNotification);
    }
}

void FamaLamaJamAudioProcessorEditor::rebuildMixerStripWidgets(const std::vector<MixerStripState>& visibleStrips)
{
    mixerContent_.removeAllChildren();
    mixerStripWidgets_.clear();
    visibleMixerStripOrder_.clear();

    std::string previousGroupId;
    for (const auto& strip : visibleStrips)
    {
        auto widgets = std::make_unique<MixerStripWidgets>();
        widgets->sourceId = strip.sourceId;
        widgets->groupId = strip.groupId;
        widgets->showsGroupLabel = previousGroupId != strip.groupId;
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

        visibleMixerStripOrder_.push_back(strip.sourceId);
        mixerStripWidgets_.push_back(std::move(widgets));
    }

    resized();
    refreshMixerStrips();
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

void FamaLamaJamAudioProcessorEditor::setSettingsDraftForTesting(const app::session::SessionSettings& settings)
{
    loadFromSettings(settings);
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

void FamaLamaJamAudioProcessorEditor::refreshForTesting()
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshMixerStrips();
}
} // namespace famalamajam::plugin
