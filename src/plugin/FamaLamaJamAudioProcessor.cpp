#include "FamaLamaJamAudioProcessor.h"

#include "infra/state/SessionSettingsSerializer.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"

namespace famalamajam::plugin
{
FamaLamaJamAudioProcessor::FamaLamaJamAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , settingsController_(settingsStore_)
{
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

void FamaLamaJamAudioProcessor::prepareToPlay(double, int) {}
void FamaLamaJamAudioProcessor::releaseResources() {}

bool FamaLamaJamAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void FamaLamaJamAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

bool FamaLamaJamAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* FamaLamaJamAudioProcessor::createEditor()
{
    auto settingsGetter = [this]() { return settingsStore_.getActiveSettings(); };
    auto apply = [this](app::session::SessionSettings draft) { return settingsController_.applyDraft(std::move(draft)); };
    auto lifecycleGetter = [this]() { return getLifecycleSnapshot(); };
    auto connect = [this]() { return requestConnect(); };
    auto disconnect = [this]() { return requestDisconnect(); };

    return new FamaLamaJamAudioProcessorEditor(*this,
                                               std::move(settingsGetter),
                                               std::move(apply),
                                               std::move(lifecycleGetter),
                                               std::move(connect),
                                               std::move(disconnect));
}

void FamaLamaJamAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    infra::state::SessionSettingsSerializer::serialize(settingsStore_.getActiveSettings(), destData);
}

void FamaLamaJamAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    bool usedFallback = false;
    const auto restored = infra::state::SessionSettingsSerializer::deserializeOrDefault(data, sizeInBytes, &usedFallback);

    app::session::SessionSettingsValidationResult validation;
    settingsStore_.applyCandidate(restored, &validation);

    lifecycleController_.resetToIdle();
    lastStatusMessage_ = usedFallback ? "State invalid. Defaults restored." : "Settings restored";
}

bool FamaLamaJamAudioProcessor::applySettingsFromUi(const app::session::SessionSettings& candidate,
                                                     app::session::SessionSettingsValidationResult* validation)
{
    auto result = settingsController_.applyDraft(candidate);

    if (validation != nullptr)
        *validation = result.validation;

    lastStatusMessage_ = result.statusMessage;
    return result.applied;
}

bool FamaLamaJamAudioProcessor::requestConnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Connect);

    if (transition.changed)
        lastStatusMessage_ = transition.snapshot.statusMessage;

    return transition.changed;
}

bool FamaLamaJamAudioProcessor::requestDisconnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Disconnect);

    if (transition.changed)
        lastStatusMessage_ = transition.snapshot.statusMessage;

    return transition.changed;
}

void FamaLamaJamAudioProcessor::handleConnectionEvent(const app::session::ConnectionEvent& event)
{
    const auto transition = lifecycleController_.handleEvent(event);

    if (transition.changed)
        lastStatusMessage_ = transition.snapshot.statusMessage;
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
} // namespace famalamajam::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new famalamajam::plugin::FamaLamaJamAudioProcessor();
}
