#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout FxSoundAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("power", 1), "Power", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("clarity", 1), "Clarity",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("ambience", 1), "Ambience",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 3.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("surround", 1), "Surround",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 2.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("dynamic_boost", 1), "Dynamic Boost",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 4.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("bass_boost", 1), "Bass Boost",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("output_gain", 1), "Output Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("eq_on", 1), "EQ On", true));

    for (int b = 0; b < 10; ++b)
    {
        auto paramId = "eq_band_" + juce::String (b);
        auto paramName = "EQ Band " + juce::String (b + 1);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (paramId, 1), paramName,
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f));
    }

    return { params.begin(), params.end() };
}

FxSoundAudioProcessor::FxSoundAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    numEqBands = dfxDsp.getNumEqBands();
    if (numEqBands <= 0 || numEqBands > 10)
        numEqBands = 9;

    dfxDsp.powerOn (true);
    dfxDsp.eqOn (true);
}

FxSoundAudioProcessor::~FxSoundAudioProcessor()
{
}

const juce::String FxSoundAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FxSoundAudioProcessor::acceptsMidi() const
{
    return false;
}

bool FxSoundAudioProcessor::producesMidi() const
{
    return false;
}

bool FxSoundAudioProcessor::isMidiEffect() const
{
    return false;
}

double FxSoundAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FxSoundAudioProcessor::getNumPrograms()
{
    return 1;
}

int FxSoundAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FxSoundAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String FxSoundAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void FxSoundAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void FxSoundAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    interleavedBuffer.resize (static_cast<size_t> (samplesPerBlock * 2));

    // Configure FxSound DSP for 32-bit floating point, stereo, at host sample rate
    dfxDsp.setSignalFormat (32, 2, static_cast<int> (sampleRate), 32);
    dfxDsp.powerOn (true);
    dfxDsp.eqOn (true);
}

void FxSoundAudioProcessor::releaseResources()
{
    interleavedBuffer.clear();
}

bool FxSoundAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void FxSoundAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // Check master bypass / power switch
    const bool isPowerOn = *apvts.getRawParameterValue ("power") > 0.5f;
    if (!isPowerOn)
        return;

    // Push parameters into DfxDsp engine
    dfxDsp.setEffectValue (DfxDsp::Effect::Fidelity,     *apvts.getRawParameterValue ("clarity"));
    dfxDsp.setEffectValue (DfxDsp::Effect::Ambience,     *apvts.getRawParameterValue ("ambience"));
    dfxDsp.setEffectValue (DfxDsp::Effect::Surround,     *apvts.getRawParameterValue ("surround"));
    dfxDsp.setEffectValue (DfxDsp::Effect::DynamicBoost, *apvts.getRawParameterValue ("dynamic_boost"));
    dfxDsp.setEffectValue (DfxDsp::Effect::Bass,         *apvts.getRawParameterValue ("bass_boost"));
    dfxDsp.setMasterGain (*apvts.getRawParameterValue ("output_gain"));

    const bool eqEnabled = *apvts.getRawParameterValue ("eq_on") > 0.5f;
    dfxDsp.eqOn (eqEnabled);

    if (eqEnabled)
    {
        for (int b = 0; b < numEqBands; ++b)
        {
            auto paramId = "eq_band_" + juce::String (b);
            if (auto* param = apvts.getRawParameterValue (paramId))
                dfxDsp.setEqBandBoostCut (b, *param);
        }
    }

    // Interleave JUCE planar float buffer into L0, R0, L1, R1...
    if (interleavedBuffer.size() < static_cast<size_t> (numSamples * 2))
        interleavedBuffer.resize (static_cast<size_t> (numSamples * 2));

    const float* leftIn  = buffer.getReadPointer (0);
    const float* rightIn = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : leftIn;

    for (int i = 0; i < numSamples; ++i)
    {
        interleavedBuffer[i * 2]     = leftIn[i];
        interleavedBuffer[i * 2 + 1] = rightIn[i];
    }

    // Process through the actual FxSound DSP library
    dfxDsp.processAudio (reinterpret_cast<short int*> (interleavedBuffer.data()),
                         reinterpret_cast<short int*> (interleavedBuffer.data()),
                         numSamples,
                         0);

    // De-interleave processed samples back to audio buffer
    float* leftOut  = buffer.getWritePointer (0);
    float* rightOut = (buffer.getNumChannels() > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        leftOut[i] = interleavedBuffer[i * 2];
        if (rightOut != nullptr)
            rightOut[i] = interleavedBuffer[i * 2 + 1];
    }
}

bool FxSoundAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FxSoundAudioProcessor::createEditor()
{
    return new FxSoundAudioProcessorEditor (*this);
}

void FxSoundAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FxSoundAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FxSoundAudioProcessor();
}
