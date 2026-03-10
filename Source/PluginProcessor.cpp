#include "PluginProcessor.h"
#include "PluginEditor.h"

CockReverbAudioProcessor::CockReverbAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

void CockReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverbEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

void CockReverbAudioProcessor::releaseResources()
{
}

bool CockReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled())
        return false;

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}

void CockReverbAudioProcessor::updateEngineParams()
{
    reverbEngine.size = apvts.getRawParameterValue("size")->load();
    reverbEngine.mix = apvts.getRawParameterValue("mix")->load();
    reverbEngine.tone = apvts.getRawParameterValue("tone")->load();
    reverbEngine.predelayMs = apvts.getRawParameterValue("predelay")->load();
    reverbEngine.modDepth = apvts.getRawParameterValue("modDepth")->load();
    reverbEngine.hall = apvts.getRawParameterValue("hall")->load();
    reverbEngine.drip = apvts.getRawParameterValue("drip")->load();
    reverbEngine.massive = apvts.getRawParameterValue("massive")->load() > 0.5f;
}

void CockReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (apvts.getRawParameterValue("bypass")->load() > 0.5f)
        return;

    updateEngineParams();
    reverbEngine.process(buffer);
}

void CockReverbAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
}

juce::AudioProcessorEditor* CockReverbAudioProcessor::createEditor()
{
    return new CockReverbAudioProcessorEditor(*this);
}

void CockReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void CockReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout CockReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "size", "SIZE",
        juce::NormalisableRange<float>(0.1f, 1.0f, 0.001f), 0.55f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "MIX",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "tone", "TONE",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.50f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "predelay", "PRE-DELAY",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "modDepth", "MOD",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hall", "CHAPEL",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.50f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "drip", "DRIP",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "massive", "MASSIVE", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bypass", "POWER", false));

    return layout;
}
