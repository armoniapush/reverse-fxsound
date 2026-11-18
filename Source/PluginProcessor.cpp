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
    sideEllipticalHPF.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 1;

    // 1. Elliptical Filter (High-Pass applied ONLY to Side channel)
    sideEllipticalHPF.prepare (spec);
    float initialCrossover = *apvts.getRawParameterValue ("mono_freq");
    sideEllipticalHPF.setCutoffFrequency (initialCrossover);

    // 2. Parallel Delta Filters on Side channel
    sidePresenceBP.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 1800.0f, 0.707f);
    sidePresenceBP.prepare (spec);
    sidePresenceBP.reset();

    sideAirHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 8000.0f);
    sideAirHP.prepare (spec);
    sideAirHP.reset();

    diffusionLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 8500.0f);
    diffusionLowPass.prepare (spec);
    diffusionLowPass.reset();

    // 3. Dynamic DiSP All-Pass Chain (0.4, 0.9, 1.4, 2.1 ms, LFO 0.17-0.34 Hz, excursion 0.20 ms)
    constexpr float primeDelaysMs[numAllPass] = { 0.4f, 0.9f, 1.4f, 2.1f };
    constexpr float lfoFreqsHz[numAllPass]    = { 0.17f, 0.23f, 0.29f, 0.34f };
    constexpr float initPhases[numAllPass]    = { 0.0f, 1.5708f, 3.1416f, 4.7124f };
    constexpr float allPassGains[numAllPass]  = { 0.52f, 0.48f, 0.45f, 0.42f };

    for (int i = 0; i < numAllPass; ++i)
        allPassChain[i].prepare (sampleRate, primeDelaysMs[i], 0.20f, lfoFreqsHz[i], initPhases[i], allPassGains[i]);

    // 3b. Missing Fundamental Sub-Bass Harmonic Synthesis
    subBassLPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 100.0f);
    subBassLPF.prepare (spec);
    subBassLPF.reset();

    subHarmBPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 230.0f, 0.85f);
    subHarmBPF.prepare (spec);
    subHarmBPF.reset();

    // 1-pole HPF at 2.5 kHz for clean Mid->Side high-frequency surround halo
    alphaMidAirHp = std::exp (-2.0f * juce::MathConstants<float>::pi * 2500.0f / static_cast<float> (sampleRate));
    midAirLpState = 0.0f;

    // Diffusion Pre-Delay Buffer kept minimal (unused when direct dispersive is active)
    diffusionPreDelaySamples = static_cast<int> (sampleRate * 0.018f);
    diffusionPreDelayBuffer.assign (juce::jmax (1024, diffusionPreDelaySamples + 1), 0.0f);
    diffusionWritePos = 0;

    // 4. Ballistics for Crest Factor & Transient Detector
    alphaFastAttack  = std::exp (-1.0f / (0.002f * static_cast<float> (sampleRate)));
    alphaFastRelease = std::exp (-1.0f / (0.035f * static_cast<float> (sampleRate)));
    alphaRms         = std::exp (-1.0f / (0.070f * static_cast<float> (sampleRate)));

    alphaPunchAttack  = std::exp (-1.0f / (0.003f * static_cast<float> (sampleRate))); // 3 ms
    alphaPunchRelease = std::exp (-1.0f / (0.045f * static_cast<float> (sampleRate))); // 45 ms
    midPunchSmoothed    = 1.0f;
    sideDuckingSmoothed = 1.0f;

    alphaMidAir8k  = std::exp (-2.0f * juce::MathConstants<float>::pi * 8000.0f / static_cast<float> (sampleRate));
    midAir8kLpState = 0.0f;

    peakFastMid = 0.0f;
    rmsSlowMid  = 0.0f;

    // 5. Envelope-Follower L2 RMS Correlation Guard (150ms RMS, 25ms attack, 400ms release)
    midEnergyRms = 0.0f;
    sideEnergyRms = 0.0f;
    guardGainSmoothed = 1.0f;
    alphaEnvSlow      = std::exp (-1.0f / (0.150f * static_cast<float> (sampleRate))); // 150 ms L2 RMS
    alphaGuardAttack  = std::exp (-1.0f / (0.025f * static_cast<float> (sampleRate))); // 25 ms attack
    alphaGuardRelease = std::exp (-1.0f / (0.400f * static_cast<float> (sampleRate))); // 400 ms release

    // 6. Auto-Gain Perceptual Matching
    inRmsSmoothed  = 0.0f;
    outRmsSmoothed = 0.0f;
    autoGainFactor = 1.0f;

    // 7. FFT Spectrum Analysis (1024 points)
    fifoIndex = 0;
    std::fill (std::begin (fifo), std::end (fifo), 0.0f);
    std::fill (std::begin (fftData), std::end (fftData), 0.0f);
    scopeLevels.fill (0.0f);
    nextFFTBlockReady.store (false);

    liveCorrelation.store (1.0f);
    smoothedCorrelation = 1.0f;
    sumDot = 0.0f;
    sumL2 = 0.0f;
    sumR2 = 0.0f;
    correlationSampleCount = 0;
}

void FxSoundAudioProcessor::pushNextSampleIntoFifo (float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (! nextFFTBlockReady.load (std::memory_order_relaxed))
        {
            std::fill (std::begin (fftData), std::end (fftData), 0.0f);
            std::copy (std::begin (fifo), std::end (fifo), std::begin (fftData));
            nextFFTBlockReady.store (true, std::memory_order_release);
        }
        fifoIndex = 0;
    }
    fifo[fifoIndex++] = sample;
}

void FxSoundAudioProcessor::processFFT()
{
    if (nextFFTBlockReady.load (std::memory_order_acquire))
    {
        window.multiplyWithWindowingTable (fftData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform (fftData);

        juce::SpinLock::ScopedLockType sl (scopeLock);
        constexpr int numBins = fftSize / 2;
        for (int i = 0; i < 64; ++i)
        {
            float proportion = static_cast<float> (i) / 63.0f;
            int binIndex = juce::jlimit (1, numBins - 1, static_cast<int> (std::pow (proportion, 2.2f) * (numBins - 1) + 1));

            float magnitude = fftData[binIndex];
            float levelDb = juce::Decibels::gainToDecibels (magnitude) - 18.0f;
            float normalized = juce::jlimit (0.0f, 1.0f, juce::jmap (levelDb, -60.0f, 6.0f, 0.0f, 1.0f));

            scopeLevels[i] = juce::jmax (normalized, scopeLevels[i] * 0.85f);
        }
        nextFFTBlockReady.store (false, std::memory_order_release);
    }
}

void FxSoundAudioProcessor::copyScopeData (std::array<float, 64>& dest)
{
    processFFT();
    juce::SpinLock::ScopedLockType sl (scopeLock);
    dest = scopeLevels;
}

void FxSoundAudioProcessor::releaseResources()
{
    diffusionPreDelayBuffer.clear();
    for (auto& ap : allPassChain)
        ap.reset();
    sidePresenceBP.reset();
    sideAirHP.reset();
    diffusionLowPass.reset();
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
    const float bassBoostParam  = *apvts.getRawParameterValue ("bass_boost");   // 0.0 to 10.0 (Sub Bass Density)
    const float outputGainDb    = *apvts.getRawParameterValue ("output_gain");   // -12 to +12 dB
    const bool  guardActive     = *apvts.getRawParameterValue ("guard_active") > 0.5f;

    // Update Elliptical Filter Cutoff smoothly
    sideEllipticalHPF.setCutoffFrequency (monoFreqParam);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : inL;

    float* outL = buffer.getWritePointer (0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer (1) : nullptr;

    const float spatialNorm = spatialMacro * 0.10f; // 0.0 to 1.0

    for (int i = 0; i < numSamples; ++i)
    {
        float xL = inL[i];
        float xR = inR[i];

        // M/S Encoding
        float M = (xL + xR) * 0.5f;
        float S = (xL - xR) * 0.5f;

        // 1. Mid 100% limpio (sin cleanAnalogSaturation que distorsione masters)
        float enhancedM = M;

        // 2. Side limpio por encima de mono_freq (sin ducking mT/mD_factor)
        float sMono = sideEllipticalHPF.processSample (0, S);

        // 3. Bandas paralelas + Armonicos estereo de subgrave (130-380 Hz)
        float deltaPresence = sidePresenceBP.processSample (sMono);
        float deltaAir      = sideAirHP.processSample (sMono);
        float bSub  = subBassLPF.processSample (M);
        float bHarm = subHarmBPF.processSample (std::tanh (2.5f * bSub) - bSub);

        // Escalado musical estable (cero ducking)
        float sExpanded = sMono * (1.0f + spatialNorm * 0.85f)
                        + deltaPresence * (spatialNorm * 0.25f)
                        + deltaAir      * (spatialNorm * 0.40f + clarityAirParam * 0.02f)
                        + bHarm         * (spatialNorm * 0.45f);

        // 4. Guardia RMS L2 real (rho >= +0.35 garantizado, sin bombeo)
        midEnergyRms  = alphaEnvSlow * midEnergyRms  + (1.0f - alphaEnvSlow) * (enhancedM * enhancedM);
        sideEnergyRms = alphaEnvSlow * sideEnergyRms + (1.0f - alphaEnvSlow) * (sExpanded * sExpanded);

        float midRms  = std::sqrt (midEnergyRms + 1e-9f);
        float sideRms = std::sqrt (sideEnergyRms + 1e-9f);
        float maxSide = 0.694f * midRms;

        float targetGuard = (guardActive && midRms > 1e-4f && sideRms > maxSide && sideRms > 1e-5f)
                          ? (maxSide / sideRms)
                          : 1.0f;
        float aG = (targetGuard < guardGainSmoothed) ? alphaGuardAttack : alphaGuardRelease;
        guardGainSmoothed = aG * guardGainSmoothed + (1.0f - aG) * targetGuard;

        float finalS = sExpanded * guardGainSmoothed;
        float rawL = enhancedM + finalS;
        float rawR = enhancedM - finalS;

        // 5. Soft-clipper de seguridad a -0.35 dBFS (cero clipping digital)
        auto softLimit = [] (float x) noexcept {
            constexpr float thresh = 0.88f;
            float ax = std::abs (x);
            if (ax <= thresh) return x;
            float over = thresh + 0.08f * std::tanh ((ax - thresh) / 0.08f);
            return (x > 0.0f) ? over : -over;
        };

        float finalL = softLimit (rawL);
        float finalR = softLimit (rawR);

        outL[i] = finalL;
        if (outR != nullptr)
            outR[i] = finalR;

        // Push to FFT Spectrum Visualizer FIFO
        pushNextSampleIntoFifo ((finalL + finalR) * 0.5f);

        // Pearson Correlation Accumulation
        sumDot += finalL * finalR;
        sumL2  += finalL * finalL;
        sumR2  += finalR * finalR;
        correlationSampleCount++;
    }

    // 5. Output Master Trim (Dynamic Auto-Gain is removed to prevent all pumping)
    float masterTrim = juce::Decibels::decibelsToGain (outputGainDb);
    if (std::abs (outputGainDb) > 0.01f)
        buffer.applyGain (masterTrim);

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
