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
    lowPassL.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowPassR.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highPassL.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    highPassR.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

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
    // Configure FxSound DSP Engine
    dfxDsp.setSignalFormat (32, 2, static_cast<int> (sampleRate), 32);
    dfxDsp.powerOn (true);
    dfxDsp.eqOn (true);

    interleavedDfxBuffer.resize (static_cast<size_t> (samplesPerBlock * 2));
    lowBuffer.setSize (2, samplesPerBlock);
    highBuffer.setSize (2, samplesPerBlock);

    // Prepare Linkwitz-Riley Crossover Filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 1;

    lowPassL.prepare (spec);
    lowPassR.prepare (spec);
    highPassL.prepare (spec);
    highPassR.prepare (spec);

    float initialCrossover = *apvts.getRawParameterValue ("mono_freq");
    lowPassL.setCutoffFrequency (initialCrossover);
    lowPassR.setCutoffFrequency (initialCrossover);
    highPassL.setCutoffFrequency (initialCrossover);
    highPassR.setCutoffFrequency (initialCrossover);

    // Prepare Vocal Mid-Channel Bandpass (Spectre-style: ~935 Hz, Q=0.71)
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 935.0f, 0.71f);
    midBandpass.coefficients = coeffs;
    midBandpass.reset();

    // Transient & Sustain Dual Envelope Follower (Fast ~2ms, Slow ~60ms)
    alphaFast = std::exp (-1.0f / (0.002f * static_cast<float> (sampleRate)));
    alphaSlow = std::exp (-1.0f / (0.060f * static_cast<float> (sampleRate)));
    envFast = 0.0f;
    envSlow = 0.0f;

    // Headless Auto-Gain & Phase Guard initial state
    inRmsSmoothed = 0.0f;
    outRmsSmoothed = 0.0f;
    autoGainFactor = 1.0f;
    phaseGuardDucking = 1.0f;

    liveCorrelation.store (1.0f);
    smoothedCorrelation = 1.0f;
    sumDot = 0.0f;
    sumL2 = 0.0f;
    sumR2 = 0.0f;
    correlationSampleCount = 0;
}

void FxSoundAudioProcessor::releaseResources()
{
    interleavedDfxBuffer.clear();
    lowBuffer.setSize (0, 0);
    highBuffer.setSize (0, 0);
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
    const float spatialMacro   = *apvts.getRawParameterValue ("spatial");       // 0.0 to 10.0
    const float vocalPunchParam = *apvts.getRawParameterValue ("vocal_punch");  // 0.0 to 10.0
    const float monoFreqParam  = *apvts.getRawParameterValue ("mono_freq");    // 80 to 250 Hz
    const float clarityAirParam = *apvts.getRawParameterValue ("clarity_air");  // 0.0 to 10.0
    const float bassBoostParam = *apvts.getRawParameterValue ("bass_boost");    // 0.0 to 10.0
    const float outputGainDb   = *apvts.getRawParameterValue ("output_gain");   // -12 to +12 dB
    const bool guardEnabled    = *apvts.getRawParameterValue ("guard_active") > 0.5f;

    // Update Crossover Frequencies
    lowPassL.setCutoffFrequency (monoFreqParam);
    lowPassR.setCutoffFrequency (monoFreqParam);
    highPassL.setCutoffFrequency (monoFreqParam);
    highPassR.setCutoffFrequency (monoFreqParam);

    // Configure FxSound Engine
    // Map spatialMacro to FxSound Surround + Ambience + Dynamic Boost
    const float fxSurround = juce::jmin (10.0f, spatialMacro * 0.85f);
    const float fxAmbience = juce::jmin (10.0f, spatialMacro * 0.55f);
    const float fxClarity  = juce::jmin (10.0f, (spatialMacro * 0.4f) + (clarityAirParam * 0.6f));
    const float fxDynamic  = juce::jmin (10.0f, spatialMacro * 0.4f);
    const float fxBass     = juce::jmin (10.0f, bassBoostParam);

    dfxDsp.setEffectValue (DfxDsp::Effect::Surround,     fxSurround);
    dfxDsp.setEffectValue (DfxDsp::Effect::Ambience,     fxAmbience);
    dfxDsp.setEffectValue (DfxDsp::Effect::Fidelity,     fxClarity);
    dfxDsp.setEffectValue (DfxDsp::Effect::DynamicBoost, fxDynamic);
    dfxDsp.setEffectValue (DfxDsp::Effect::Bass,         fxBass);
    dfxDsp.setMasterGain (outputGainDb);

    // Measure Incoming RMS for Headless Auto-Gain Equal-Loudness matching
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : inL;

    float inEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inEnergy += inL[i] * inL[i] + inR[i] * inR[i];
    float inRms = std::sqrt (inEnergy / (2.0f * numSamples + 1e-9f));
    inRmsSmoothed = (inRmsSmoothed < 1e-6f) ? inRms : (0.94f * inRmsSmoothed + 0.06f * inRms);

    // Prepare Multiband Split Buffers
    if (lowBuffer.getNumSamples() < numSamples)
    {
        lowBuffer.setSize (2, numSamples, false, false, true);
        highBuffer.setSize (2, numSamples, false, false, true);
    }

    float* lowL = lowBuffer.getWritePointer (0);
    float* lowR = lowBuffer.getWritePointer (1);
    float* highL = highBuffer.getWritePointer (0);
    float* highR = highBuffer.getWritePointer (1);

    // 1. Crossover Filtering: Linkwitz-Riley 4th Order (Per-sample phase aligned)
    for (int i = 0; i < numSamples; ++i)
    {
        lowL[i]  = lowPassL.processSample (0, inL[i]);
        lowR[i]  = lowPassR.processSample (0, inR[i]);
        highL[i] = highPassL.processSample (0, inL[i]);
        highR[i] = highPassR.processSample (0, inR[i]);

        // Collapse Lows to Pure Mono (100% Phase Correlation Anchor)
        float monoLow = (lowL[i] + lowR[i]) * 0.5f;
        lowL[i] = monoLow;
        lowR[i] = monoLow;
    }

    // 2. High-Frequency Band: Interleave for FxSound processing
    if (interleavedDfxBuffer.size() < static_cast<size_t> (numSamples * 2))
        interleavedDfxBuffer.resize (static_cast<size_t> (numSamples * 2));

    for (int i = 0; i < numSamples; ++i)
    {
        interleavedDfxBuffer[i * 2]     = highL[i];
        interleavedDfxBuffer[i * 2 + 1] = highR[i];
    }

    // Process Mid-Highs with FxSound 3D Surround & Ambience
    dfxDsp.processAudio (reinterpret_cast<short int*> (interleavedDfxBuffer.data()),
                         reinterpret_cast<short int*> (interleavedDfxBuffer.data()),
                         numSamples,
                         0);

    for (int i = 0; i < numSamples; ++i)
    {
        highL[i] = interleavedDfxBuffer[i * 2];
        highR[i] = interleavedDfxBuffer[i * 2 + 1];
    }

    // 3. Spectral Transient / Sustain Mid-Side Processing (Ozone 12 & Pro-MB architecture)
    // Headless Adaptive Phase Guard calculation
    float targetDucking = 1.0f;
    if (smoothedCorrelation < 0.45f)
    {
        // Smoothly duck side channel width if correlation approaches danger zone
        targetDucking = juce::jlimit (0.25f, 1.0f, (smoothedCorrelation - 0.20f) / 0.25f);
    }
    phaseGuardDucking = 0.90f * phaseGuardDucking + 0.10f * targetDucking;

    // Side channel width multiplier (guarded against extreme phase divergence)
    const float baseWidthMultiplier = 1.0f + (spatialMacro * 0.05f); // 1.0x to 1.5x controlled width
    const float effectiveSideWidth  = 1.0f + ((baseWidthMultiplier - 1.0f) * phaseGuardDucking);

    // Mid channel saturation drive
    const float solidDrive = 1.0f + (spatialMacro * 0.12f) + (vocalPunchParam * 0.20f);
    const float solidMix   = juce::jmin (1.0f, 0.25f + (spatialMacro * 0.05f) + (vocalPunchParam * 0.05f));

    float* outL = buffer.getWritePointer (0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // High band M/S
        float m = (highL[i] + highR[i]) * 0.5f;
        float s = (highL[i] - highR[i]) * 0.5f;

        // Dual-Ballistic Envelope Follower (Fast ~2ms vs Slow ~60ms)
        float absM = std::abs (m);
        envFast = (absM > envFast) ? (0.35f * envFast + 0.65f * absM) : (alphaFast * envFast);
        envSlow = (absM > envSlow) ? (0.05f * envSlow + 0.95f * absM) : (alphaSlow * envSlow);

        float transientEnv = juce::jmax (0.0f, envFast - envSlow);

        // MID CHANNEL: Inject Transient Punch & Solid Saturation on vocal band
        float vocalBand = midBandpass.processSample (m);
        float transientPunchBoost = 1.0f + (transientEnv * (vocalPunchParam * 0.6f));
        float saturatedVocal = applySolidSaturation (vocalBand * transientPunchBoost, solidDrive);
        float harmonicDelta = saturatedVocal - vocalBand;
        float enhancedM = m + harmonicDelta * solidMix;

        // SIDE CHANNEL: Sustain-focused expansion with Adaptive Phase Guard
        float enhancedS = s * effectiveSideWidth;

        // Reconstruct Mid-High Stereo
        float finalHighL = enhancedM + enhancedS;
        float finalHighR = enhancedM - enhancedS;

        // Sum with Mono Low Anchor
        float sL = lowL[i] + finalHighL;
        float sR = lowR[i] + finalHighR;

        outL[i] = sL;
        if (outR != nullptr)
            outR[i] = sR;

        // Accumulate statistics for correlation measurement
        sumDot += sL * sR;
        sumL2  += sL * sL;
        sumR2  += sR * sR;
        correlationSampleCount++;
    }

    // 4. Headless Perceptual Auto-Gain Compensator (Matches Output to Input RMS)
    float outEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        outEnergy += outL[i] * outL[i] + (outR != nullptr ? outR[i] * outR[i] : outL[i] * outL[i]);
    float outRms = std::sqrt (outEnergy / (2.0f * numSamples + 1e-9f));
    outRmsSmoothed = (outRmsSmoothed < 1e-6f) ? outRms : (0.94f * outRmsSmoothed + 0.06f * outRms);

    if (inRmsSmoothed > 1e-4f && outRmsSmoothed > 1e-4f)
    {
        float targetAutoGain = inRmsSmoothed / outRmsSmoothed;
        targetAutoGain = juce::jlimit (0.25f, 2.0f, targetAutoGain);
        autoGainFactor = 0.94f * autoGainFactor + 0.06f * targetAutoGain;
    }

    // Apply Auto-Gain and Master Output Gain Trim
    float masterTrim = juce::Decibels::decibelsToGain (outputGainDb);
    buffer.applyGain (autoGainFactor * masterTrim);

    // 5. Real-Time Phase Correlation Calculation
    if (correlationSampleCount >= 512)
    {
        float denom = std::sqrt (sumL2 * sumR2);
        float rawCorr = (denom > 1e-6f) ? (sumDot / denom) : 1.0f;
        rawCorr = juce::jlimit (-1.0f, 1.0f, rawCorr);

        // Smooth correlation over time (leaky integrator)
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
