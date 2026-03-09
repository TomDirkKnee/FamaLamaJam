#include "FamaLamaJamAudioProcessorEditor.h"

namespace famalamajam::plugin
{
FamaLamaJamAudioProcessorEditor::FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
                                                                 SettingsGetter settingsGetter,
                                                                 ApplyHandler applyHandler,
                                                                 LifecycleGetter lifecycleGetter,
                                                                 CommandHandler connectHandler,
                                                                 CommandHandler disconnectHandler)
    : juce::AudioProcessorEditor(processor)
    , settingsGetter_(std::move(settingsGetter))
    , applyHandler_(std::move(applyHandler))
    , lifecycleGetter_(std::move(lifecycleGetter))
    , connectHandler_(std::move(connectHandler))
    , disconnectHandler_(std::move(disconnectHandler))
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

    applyButton_.setButtonText("Apply");
    applyButton_.onClick = [this]() {
        const auto result = applyHandler_(makeDraftFromUi());

        if (result.applied)
        {
            loadFromSettings(settingsGetter_());
            refreshLifecycleStatus();
            return;
        }

        statusLabel_.setText(result.statusMessage, juce::dontSendNotification);
    };
    addAndMakeVisible(applyButton_);

    connectButton_.setButtonText("Connect");
    connectButton_.onClick = [this]() {
        connectHandler_();
        refreshLifecycleStatus();
    };
    addAndMakeVisible(connectButton_);

    disconnectButton_.setButtonText("Disconnect");
    disconnectButton_.onClick = [this]() {
        disconnectHandler_();
        refreshLifecycleStatus();
    };
    addAndMakeVisible(disconnectButton_);

    statusLabel_.setText("Ready", juce::dontSendNotification);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    loadFromSettings(settingsGetter_());
    refreshLifecycleStatus();
    setSize(620, 340);
}

void FamaLamaJamAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    titleLabel_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    auto row = [&](juce::Label& label, juce::Component& editor) {
        auto current = area.removeFromTop(30);
        label.setBounds(current.removeFromLeft(160));
        editor.setBounds(current);
        area.removeFromTop(6);
    };

    row(hostLabel_, hostEditor_);
    row(portLabel_, portEditor_);
    row(usernameLabel_, usernameEditor_);
    row(gainLabel_, gainSlider_);
    row(panLabel_, panSlider_);

    muteToggle_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    auto controls = area.removeFromTop(32);
    connectButton_.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(8);
    disconnectButton_.setBounds(controls.removeFromLeft(120));
    controls.removeFromLeft(8);
    applyButton_.setBounds(controls.removeFromLeft(100));

    area.removeFromTop(8);
    statusLabel_.setBounds(area.removeFromTop(32));
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

    const auto status = snapshot.statusMessage.empty() ? app::session::toString(snapshot.state)
                                                       : snapshot.statusMessage;

    statusLabel_.setText(juce::String(status), juce::dontSendNotification);
    connectButton_.setEnabled(snapshot.canConnect());
    disconnectButton_.setEnabled(snapshot.canDisconnect());
}
} // namespace famalamajam::plugin
