#include "FamaLamaJamAudioProcessor.h"

#include "infra/state/SessionSettingsSerializer.h"
#include "plugin/FamaLamaJamAudioProcessorEditor.h"

namespace famalamajam::plugin
{
namespace
{
const juce::Identifier kPluginStateType("famalamajam.plugin.state");
const juce::Identifier kPluginStateSchemaVersion("schemaVersion");
const juce::Identifier kPluginStateLastErrorContext("lastErrorContext");
constexpr int kPluginStateSchema = 1;
constexpr std::size_t kMaxLastErrorContextLength = 64;

[[nodiscard]] std::string shortenLastErrorContext(const std::string& context)
{
    if (context.size() <= kMaxLastErrorContextLength)
        return context;

    return context.substr(0, kMaxLastErrorContextLength - 3) + "...";
}

[[nodiscard]] std::string makeRestoreStatusMessage(bool usedFallback, const std::string& lastErrorContext)
{
    if (usedFallback)
        return "State invalid. Defaults restored.";

    if (lastErrorContext.empty())
        return "Settings restored";

    return "Settings restored. Last error: " + lastErrorContext;
}

[[nodiscard]] juce::ValueTree parseValueTree(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return {};

    return juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
}
} // namespace

FamaLamaJamAudioProcessor::FamaLamaJamAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , settingsController_(settingsStore_)
{
}

FamaLamaJamAudioProcessor::~FamaLamaJamAudioProcessor()
{
    clearReconnectTimer();
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
    auto apply = [this](app::session::SessionSettings draft) { return applySettingsDraft(std::move(draft)); };
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
    juce::MemoryBlock settingsPayload;
    infra::state::SessionSettingsSerializer::serialize(settingsStore_.getActiveSettings(), settingsPayload);

    juce::ValueTree stateTree(kPluginStateType);
    stateTree.setProperty(kPluginStateSchemaVersion, kPluginStateSchema, nullptr);

    const auto settingsTree = parseValueTree(settingsPayload.getData(), static_cast<int>(settingsPayload.getSize()));
    if (settingsTree.isValid())
        stateTree.addChild(settingsTree.createCopy(), -1, nullptr);

    const auto lastErrorContext = shortenLastErrorContext(lifecycleController_.getSnapshot().lastError);
    if (! lastErrorContext.empty())
        stateTree.setProperty(kPluginStateLastErrorContext, juce::String(lastErrorContext), nullptr);

    juce::MemoryOutputStream stream(destData, false);
    stateTree.writeToStream(stream);
}

void FamaLamaJamAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryBlock settingsPayload;
    std::string restoredLastErrorContext;

    const auto stateTree = parseValueTree(data, sizeInBytes);
    const bool hasWrappedState = stateTree.isValid() && stateTree.hasType(kPluginStateType)
        && static_cast<int>(stateTree.getProperty(kPluginStateSchemaVersion, -1)) == kPluginStateSchema;

    if (hasWrappedState)
    {
        if (stateTree.getNumChildren() > 0)
        {
            juce::MemoryOutputStream settingsStream(settingsPayload, false);
            stateTree.getChild(0).writeToStream(settingsStream);
        }

        restoredLastErrorContext = shortenLastErrorContext(
            stateTree.getProperty(kPluginStateLastErrorContext, juce::String()).toString().toStdString());
    }

    const void* settingsData = data;
    int settingsSize = sizeInBytes;

    if (settingsPayload.getSize() > 0)
    {
        settingsData = settingsPayload.getData();
        settingsSize = static_cast<int>(settingsPayload.getSize());
    }

    bool usedFallback = false;
    const auto restored = infra::state::SessionSettingsSerializer::deserializeOrDefault(settingsData,
                                                                                         settingsSize,
                                                                                         &usedFallback);

    app::session::SessionSettingsValidationResult validation;
    settingsStore_.applyCandidate(restored, &validation);

    applyLifecycleTransition(lifecycleController_.resetToIdle());
    lastStatusMessage_ = makeRestoreStatusMessage(usedFallback, restoredLastErrorContext);
}

app::session::SessionSettingsController::ApplyResult FamaLamaJamAudioProcessor::applySettingsDraft(
    app::session::SessionSettings candidate)
{
    const bool wasErrorState = lifecycleController_.getSnapshot().state == app::session::ConnectionState::Error;
    auto result = settingsController_.applyDraft(std::move(candidate));

    if (! result.applied)
    {
        lastStatusMessage_ = result.statusMessage;
        return result;
    }

    if (wasErrorState)
    {
        auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::ApplySettings);
        applyLifecycleTransition(transition);

        if (transition.changed)
            result.statusMessage = transition.snapshot.statusMessage;
        else
            lastStatusMessage_ = result.statusMessage;

        return result;
    }

    lastStatusMessage_ = result.statusMessage;
    return result;
}

bool FamaLamaJamAudioProcessor::applySettingsFromUi(const app::session::SessionSettings& candidate,
                                                     app::session::SessionSettingsValidationResult* validation)
{
    auto result = applySettingsDraft(candidate);

    if (validation != nullptr)
        *validation = result.validation;

    return result.applied;
}

bool FamaLamaJamAudioProcessor::requestConnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Connect);
    applyLifecycleTransition(transition);
    return transition.changed;
}

bool FamaLamaJamAudioProcessor::requestDisconnect()
{
    const auto transition = lifecycleController_.handleCommand(app::session::ConnectionCommand::Disconnect);
    applyLifecycleTransition(transition);
    return transition.changed;
}

void FamaLamaJamAudioProcessor::handleConnectionEvent(const app::session::ConnectionEvent& event)
{
    const auto transition = lifecycleController_.handleEvent(event);
    applyLifecycleTransition(transition);
}

bool FamaLamaJamAudioProcessor::triggerScheduledReconnectForTesting()
{
    const auto transition = lifecycleController_.triggerScheduledReconnect();
    applyLifecycleTransition(transition);
    return transition.changed;
}

int FamaLamaJamAudioProcessor::getScheduledReconnectDelayMs() const noexcept
{
    return scheduledReconnectDelayMs_;
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

void FamaLamaJamAudioProcessor::timerCallback()
{
    triggerScheduledReconnectForTesting();
}

void FamaLamaJamAudioProcessor::applyLifecycleTransition(const app::session::ConnectionLifecycleTransition& transition)
{
    if (! transition.changed)
        return;

    lastStatusMessage_ = transition.snapshot.statusMessage;

    if (transition.effect == app::session::LifecycleEffect::ScheduleReconnect)
    {
        scheduleReconnectTimer(transition.snapshot.nextRetryDelayMs);
        return;
    }

    clearReconnectTimer();
}

void FamaLamaJamAudioProcessor::clearReconnectTimer()
{
    reconnectScheduled_ = false;
    scheduledReconnectDelayMs_ = 0;
    stopTimer();
}

void FamaLamaJamAudioProcessor::scheduleReconnectTimer(int delayMs)
{
    const auto boundedDelay = juce::jmax(1, delayMs);
    reconnectScheduled_ = true;
    scheduledReconnectDelayMs_ = boundedDelay;
    startTimer(boundedDelay);
}
} // namespace famalamajam::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new famalamajam::plugin::FamaLamaJamAudioProcessor();
}
