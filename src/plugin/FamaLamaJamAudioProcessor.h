#pragma once

#include <string>

#include <JuceHeader.h>

#include "app/session/ConnectionLifecycleController.h"
#include "app/session/SessionSettings.h"
#include "app/session/SessionSettingsController.h"

namespace famalamajam::plugin
{
class FamaLamaJamAudioProcessor final : public juce::AudioProcessor
{
public:
    FamaLamaJamAudioProcessor();

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

    app::session::SessionSettings getActiveSettings() const;
    app::session::ConnectionLifecycleSnapshot getLifecycleSnapshot() const;
    std::string getLastStatusMessage() const;
    bool isSessionConnected() const noexcept;

private:
    app::session::SessionSettingsStore settingsStore_;
    app::session::SessionSettingsController settingsController_;
    app::session::ConnectionLifecycleController lifecycleController_;
    std::string lastStatusMessage_ { "Ready" };
};
} // namespace famalamajam::plugin
