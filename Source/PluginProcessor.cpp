#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout FxSoundAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("power", 1), "Power", true));

    // Master Macro Knob
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("spatial", 1), "Billboard Spatial",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    // Secondary Fine-Tuning Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("vocal_punch", 1), "Vocal Solid Punch",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("mono_freq", 1), "Mono Bass Crossover",
        juce::NormalisableRange<float> (80.0f, 250.0f, 1.0f, 0.5f), 140.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("clarity_air", 1), "Clarity Air",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("bass_boost", 1), "Sub Bass Density",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 3.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("output_gain", 1), "Output Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("guard_active", 1), "Correlation Guard", true));

    return { params.begin(), params.end() };
}

FxSoundAudioProcessor::FxSoundAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    xo1LowL.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xo1LowR.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xo1HighL.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    xo1HighR.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    xo2LowL.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xo2LowR.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xo2HighL.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    xo2HighR.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
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
    lowBuffer.setSize (2, samplesPerBlock);
    midBuffer.setSize (2, samplesPerBlock);
    highBuffer.setSize (2, samplesPerBlock);
    upperBuffer.setSize (2, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 1;

    xo1LowL.prepare (spec);
    xo1LowR.prepare (spec);
    xo1HighL.prepare (spec);
    xo1HighR.prepare (spec);

    xo2LowL.prepare (spec);
    xo2LowR.prepare (spec);
    xo2HighL.prepare (spec);
    xo2HighR.prepare (spec);

    float initialCrossover = *apvts.getRawParameterValue ("mono_freq");
    xo1LowL.setCutoffFrequency (initialCrossover);
    xo1LowR.setCutoffFrequency (initialCrossover);
    xo1HighL.setCutoffFrequency (initialCrossover);
    xo1HighR.setCutoffFrequency (initialCrossover);

    constexpr float airCrossoverFreq = 3500.0f;
    xo2LowL.setCutoffFrequency (airCrossoverFreq);
    xo2LowR.setCutoffFrequency (airCrossoverFreq);
    xo2HighL.setCutoffFrequency (airCrossoverFreq);
    xo2HighR.setCutoffFrequency (airCrossoverFreq);

    // Ballistics for Crest Factor & Transient Detector
    alphaFastAttack  = std::exp (-1.0f / (0.0015f * static_cast<float> (sampleRate)));
    alphaFastRelease = std::exp (-1.0f / (0.025f * static_cast<float> (sampleRate)));
    alphaRms         = std::exp (-1.0f / (0.060f * static_cast<float> (sampleRate)));

    peakFastMid = 0.0f;
    rmsSlowMid  = 0.0f;

    inRmsSmoothed  = 0.0f;
    outRmsSmoothed = 0.0f;
    autoGainFactor = 1.0f;

    liveCorrelation.store (1.0f);
    smoothedCorrelation = 1.0f;
    sumDot = 0.0f;
    sumL2 = 0.0f;
    sumR2 = 0.0f;
    correlationSampleCount = 0;
}

void FxSoundAudioProcessor::releaseResources()
{
    lowBuffer.setSize (0, 0);
    midBuffer.setSize (0, 0);
    highBuffer.setSize (0, 0);
    upperBuffer.setSize (0, 0);
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

    // Master Bypass
    const bool isPowerOn = *apvts.getRawParameterValue ("power") > 0.5f;
    if (!isPowerOn)
        return;

    // Read APVTS Macro Parameters
    const float spatialMacro    = *apvts.getRawParameterValue ("spatial");       // 0.0 to 10.0
    const float vocalPunchParam = *apvts.getRawParameterValue ("vocal_punch");  // 0.0 to 10.0 (Transient Punch)
    const float monoFreqParam   = *apvts.getRawParameterValue ("mono_freq");    // 80 to 250 Hz
    const float clarityAirParam = *apvts.getRawParameterValue ("clarity_air");  // 0.0 to 10.0
    const float outputGainDb    = *apvts.getRawParameterValue ("output_gain");   // -12 to +12 dB

    // Update Crossover 1 Frequency
    xo1LowL.setCutoffFrequency (monoFreqParam);
    xo1LowR.setCutoffFrequency (monoFreqParam);
    xo1HighL.setCutoffFrequency (monoFreqParam);
    xo1HighR.setCutoffFrequency (monoFreqParam);

    // Resize buffers if necessary
    if (lowBuffer.getNumSamples() < numSamples)
    {
        lowBuffer.setSize (2, numSamples, false, false, true);
        midBuffer.setSize (2, numSamples, false, false, true);
        highBuffer.setSize (2, numSamples, false, false, true);
        upperBuffer.setSize (2, numSamples, false, false, true);
    }

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : inL;

    // 1. Measure Dry Input RMS for Headless Auto-Gain Loudness Lock
    float inEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inEnergy += inL[i] * inL[i] + inR[i] * inR[i];
    float inRms = std::sqrt (inEnergy / (2.0f * numSamples + 1e-9f));
    inRmsSmoothed = (inRmsSmoothed < 1e-6f) ? inRms : (0.94f * inRmsSmoothed + 0.06f * inRms);

    float* lowL   = lowBuffer.getWritePointer (0);
    float* lowR   = lowBuffer.getWritePointer (1);
    float* upperL = upperBuffer.getWritePointer (0);
    float* upperR = upperBuffer.getWritePointer (1);
    float* midL   = midBuffer.getWritePointer (0);
    float* midR   = midBuffer.getWritePointer (1);
    float* highL  = highBuffer.getWritePointer (0);
    float* highR  = highBuffer.getWritePointer (1);

    // 2. Stage 1 Crossover: Separate Low (< mono_freq) and Upper (> mono_freq)
    for (int i = 0; i < numSamples; ++i)
    {
        lowL[i]   = xo1LowL.processSample (0, inL[i]);
        lowR[i]   = xo1LowR.processSample (0, inR[i]);
        upperL[i] = xo1HighL.processSample (0, inL[i]);
        upperR[i] = xo1HighR.processSample (0, inR[i]);

        // Pure mono anchor for sub-bass
        float monoLow = (lowL[i] + lowR[i]) * 0.5f;
        lowL[i] = monoLow;
        lowR[i] = monoLow;
    }

    // 3. Stage 2 Crossover: Separate Upper into Mid (mono_freq - 3.5kHz) and Air (> 3.5kHz)
    for (int i = 0; i < numSamples; ++i)
    {
        midL[i]  = xo2LowL.processSample (0, upperL[i]);
        midR[i]  = xo2LowR.processSample (0, upperR[i]);
        highL[i] = xo2HighL.processSample (0, upperL[i]);
        highR[i] = xo2HighR.processSample (0, upperR[i]);
    }

    // 4. Spectral Mid-Side Processing:
    float* outL = buffer.getWritePointer (0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer (1) : nullptr;

    const float midPunchScale = vocalPunchParam * 0.08f;
    const float airWidthScale = 1.0f + (spatialMacro * 0.06f) + (clarityAirParam * 0.03f); // 1.0x to 1.9x width in air

    for (int i = 0; i < numSamples; ++i)
    {
        // --- A. MID BAND PROCESSING (Vocal & Snare Core) ---
        float mMid = (midL[i] + midR[i]) * 0.5f;
        float sMid = (midL[i] - midR[i]) * 0.5f;

        // Crest Factor Detection on Mid channel
        float absMMid = std::abs (mMid);
        peakFastMid = (absMMid > peakFastMid)
                    ? (0.55f * peakFastMid + 0.45f * absMMid)
                    : (alphaFastRelease * peakFastMid);
        rmsSlowMid  = alphaRms * rmsSlowMid + (1.0f - alphaRms) * (absMMid * absMMid);
        float rmsMid = std::sqrt (rmsSlowMid + 1e-7f);
        float crestFactor = peakFastMid / (rmsMid + 1e-4f);

        // Dynamic Transient Gain (Linear punch, zero clipping)
        float punchBoost = 1.0f;
        if (crestFactor > 1.15f)
        {
            float transientExcess = juce::jlimit (0.0f, 1.8f, crestFactor - 1.15f);
            punchBoost += transientExcess * midPunchScale;
        }

        // Apply clean analog soft-knee saturation (prevents clipping peaks transparently)
        float enhancedMMid = cleanAnalogSaturation (mMid * punchBoost);

        // Mid Band Side Limiter (Ensures Mid channel always dominates center, preserving correlation)
        float desiredSMid = sMid * (1.0f + spatialMacro * 0.015f);
        float maxAllowedSMid = 0.50f * (std::abs (enhancedMMid) + 1e-5f);
        float enhancedSMid = juce::jlimit (-maxAllowedSMid, maxAllowedSMid, desiredSMid);

        // Reconstruct Mid Band Stereo
        float midOutL = enhancedMMid + enhancedSMid;
        float midOutR = enhancedMMid - enhancedSMid;

        // --- B. AIR BAND PROCESSING (Spatial Bloom & Shimmer > 3.5kHz) ---
        float mHigh = (highL[i] + highR[i]) * 0.5f;
        float sHigh = (highL[i] - highR[i]) * 0.5f;

        // Expand Side Channel in Air band
        float desiredSHigh = sHigh * airWidthScale;
        // High Band Side Limiter (Guarantees Air band also never breaks phase)
        float maxAllowedSHigh = 0.65f * (std::abs (mHigh) + 1e-5f);
        float enhancedSHigh = juce::jlimit (-maxAllowedSHigh, maxAllowedSHigh, desiredSHigh);

        // Reconstruct Air Band Stereo
        float highOutL = mHigh + enhancedSHigh;
        float highOutR = mHigh - enhancedSHigh;

        // --- C. SPECTRAL SUMMATION ---
        float finalL = lowL[i] + midOutL + highOutL;
        float finalR = lowR[i] + midOutR + highOutR;

        outL[i] = finalL;
        if (outR != nullptr)
            outR[i] = finalR;

        // Accumulate Pearson Correlation Statistics
        sumDot += finalL * finalR;
        sumL2  += finalL * finalL;
        sumR2  += finalR * finalR;
        correlationSampleCount++;
    }

    // 5. Headless Perceptual Auto-Gain (Equal-Loudness Match)
    float outEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        outEnergy += outL[i] * outL[i] + (outR != nullptr ? outR[i] * outR[i] : outL[i] * outL[i]);
    float outRms = std::sqrt (outEnergy / (2.0f * numSamples + 1e-9f));
    outRmsSmoothed = (outRmsSmoothed < 1e-6f) ? outRms : (0.94f * outRmsSmoothed + 0.06f * outRms);

    if (inRmsSmoothed > 1e-4f && outRmsSmoothed > 1e-4f)
    {
        float targetAutoGain = inRmsSmoothed / outRmsSmoothed;
        targetAutoGain = juce::jlimit (0.35f, 1.8f, targetAutoGain);
        autoGainFactor = 0.94f * autoGainFactor + 0.06f * targetAutoGain;
    }

    // Apply Auto-Gain and Master Output Gain Trim
    float masterTrim = juce::Decibels::decibelsToGain (outputGainDb);
    buffer.applyGain (autoGainFactor * masterTrim);

    // 6. Real-Time Phase Correlation Calculation
    if (correlationSampleCount >= 512)
    {
        float denom = std::sqrt (sumL2 * sumR2);
        float rawCorr = (denom > 1e-6f) ? (sumDot / denom) : 1.0f;
        rawCorr = juce::jlimit (-1.0f, 1.0f, rawCorr);

        smoothedCorrelation = 0.85f * smoothedCorrelation + 0.15f * rawCorr;
        liveCorrelation.store (smoothedCorrelation, std::memory_order_relaxed);

        sumDot = 0.0f;
        sumL2  = 0.0f;
        sumR2  = 0.0f;
        correlationSampleCount = 0;
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
