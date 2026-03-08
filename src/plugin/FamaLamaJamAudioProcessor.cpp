#include <JuceHeader.h>

namespace famalamajam
{
class FamaLamaJamAudioProcessor final : public juce::AudioProcessor
{
public:
    FamaLamaJamAudioProcessor()
        : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                                .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "FamaLamaJam"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
            buffer.clear(channel, 0, buffer.getNumSamples());
    }

    bool hasEditor() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
} // namespace famalamajam

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new famalamajam::FamaLamaJamAudioProcessor();
}
