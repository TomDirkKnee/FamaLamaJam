#pragma once

#include <functional>

#include <JuceHeader.h>

#include "app/session/ConnectionLifecycle.h"
#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"

namespace famalamajam::plugin
{
class FamaLamaJamAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    using SettingsGetter = std::function<app::session::SessionSettings()>;
    using ApplyHandler = std::function<app::session::SessionSettingsController::ApplyResult(app::session::SessionSettings)>;
    using LifecycleGetter = std::function<app::session::ConnectionLifecycleSnapshot()>;
    using CommandHandler = std::function<bool()>;

    FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
                                    SettingsGetter settingsGetter,
                                    ApplyHandler applyHandler,
                                    LifecycleGetter lifecycleGetter,
                                    CommandHandler connectHandler,
                                    CommandHandler disconnectHandler);

    void resized() override;

private:
    app::session::SessionSettings makeDraftFromUi() const;
    void loadFromSettings(const app::session::SessionSettings& settings);
    void refreshLifecycleStatus();

    SettingsGetter settingsGetter_;
    ApplyHandler applyHandler_;
    LifecycleGetter lifecycleGetter_;
    CommandHandler connectHandler_;
    CommandHandler disconnectHandler_;

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
    juce::TextButton applyButton_;
    juce::TextButton connectButton_;
    juce::TextButton disconnectButton_;
    juce::Label statusLabel_;
};
} // namespace famalamajam::plugin
