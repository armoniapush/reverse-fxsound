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

    sideAirHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 4200.0f);
    sideAirHP.prepare (spec);
    sideAirHP.reset();

    diffusionLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 8500.0f);
    diffusionLowPass.prepare (spec);
    diffusionLowPass.reset();

    // 3. Schroeder Diffuse All-Pass Chain (Prime delay times in ms: 1.7, 3.1, 5.3, 8.9)
    constexpr float primeDelaysMs[numAllPass] = { 1.7f, 3.1f, 5.3f, 8.9f };
    constexpr float allPassGains[numAllPass]  = { 0.50f, 0.50f, 0.45f, 0.40f };

    for (int i = 0; i < numAllPass; ++i)
    {
        int delaySamples = static_cast<int> (sampleRate * primeDelaysMs[i] * 0.001f);
        allPassChain[i].prepare (delaySamples, allPassGains[i]);
    }

    // Diffusion Pre-Delay Buffer (~18ms)
    diffusionPreDelaySamples = static_cast<int> (sampleRate * 0.018f);
    diffusionPreDelayBuffer.assign (juce::jmax (1024, diffusionPreDelaySamples + 1), 0.0f);
    diffusionWritePos = 0;

    // 4. Ballistics for Crest Factor & Transient Detector
    alphaFastAttack  = std::exp (-1.0f / (0.002f * static_cast<float> (sampleRate)));
    alphaFastRelease = std::exp (-1.0f / (0.035f * static_cast<float> (sampleRate)));
    alphaRms         = std::exp (-1.0f / (0.070f * static_cast<float> (sampleRate)));

    peakFastMid = 0.0f;
    rmsSlowMid  = 0.0f;

    // 5. Envelope-Follower RMS Correlation Guard
    midEnergyRms = 0.0f;
    sideEnergyRms = 0.0f;
    guardGainSmoothed = 1.0f;

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

    // 1. Measure Input Energy for Perceptual Auto-Gain
    float inEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inEnergy += inL[i] * inL[i] + inR[i] * inR[i];
    float inRms = std::sqrt (inEnergy / (2.0f * numSamples + 1e-9f));
    inRmsSmoothed = (inRmsSmoothed < 1e-6f) ? inRms : (0.97f * inRmsSmoothed + 0.03f * inRms);

    float* outL = buffer.getWritePointer (0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer (1) : nullptr;

    const int preDelayBufSize = static_cast<int> (diffusionPreDelayBuffer.size());
    const float spatialNorm = spatialMacro * 0.10f; // 0.0 to 1.0

    // Coefficients for envelope followers
    constexpr float alphaEnvAttack  = 0.985f;
    constexpr float alphaEnvRelease = 0.9992f;
    constexpr float alphaGuardFast  = 0.980f;
    constexpr float alphaGuardSlow  = 0.999f;

    for (int i = 0; i < numSamples; ++i)
    {
        float xL = inL[i];
        float xR = inR[i];

        // M/S Encoding
        float M = (xL + xR) * 0.5f;
        float S = (xL - xR) * 0.5f;

        // --- 1. DYNAMIC PHASE MID PROCESSING (Dry Path Intact) ---
        // Crest Factor Detection
        float absM = std::abs (M);
        peakFastMid = (absM > peakFastMid)
                    ? (alphaFastAttack * peakFastMid + (1.0f - alphaFastAttack) * absM)
                    : (alphaFastRelease * peakFastMid);
        rmsSlowMid  = alphaRms * rmsSlowMid + (1.0f - alphaRms) * (absM * absM);
        float rmsMid = std::sqrt (rmsSlowMid + 1e-7f);
        float crestFactor = peakFastMid / (rmsMid + 1e-4f);

        // Transient Punch Boost (Mid) & Transient Ducking (Side)
        float midPunch = 1.0f;
        float sideDucking = 1.0f;
        if (crestFactor > 1.20f)
        {
            float excess = juce::jlimit (0.0f, 2.5f, crestFactor - 1.20f);
            midPunch += excess * (vocalPunchParam * 0.06f);
            // Duck side slightly during attack so vocal/snare cuts dead center
            sideDucking -= juce::jlimit (0.0f, 0.30f, excess * 0.08f * (vocalPunchParam * 0.1f));
        }

        // Sub Bass Density enhancement on Mid
        if (bassBoostParam > 0.1f)
        {
            // Subtle even harmonic warmth on bass without boominess
            float bassWarmth = 0.03f * (bassBoostParam * 0.1f) * (M * M * (M > 0.0f ? 1.0f : -1.0f));
            M += bassWarmth;
        }

        float enhancedM = cleanAnalogSaturation (M * midPunch);

        // 1. Mid cede ligeramente al subir Spatial para dar contraste y profundidad (-1.4 dB máx)
        float midContrast = 1.0f - (spatialNorm * 0.15f);
        enhancedM *= midContrast;

        // --- 2. DYNAMIC PHASE SIDE PROCESSING (Elliptical + Delta Bands + Schroeder Diffusion) ---
        // Elliptical High-Pass filter on Side channel ONLY
        // Below mono_freq, S_mono -> 0, guaranteeing mono bass compatibility
        float sMono = sideEllipticalHPF.processSample (0, S);

        // Presence and Air Delta expansions
        float deltaPresence = sidePresenceBP.processSample (sMono);
        float deltaAir      = sideAirHP.processSample (sMono);

        // 2. Alimenta el Pre-Delay + Red Allpass con (M * 0.45f + sMono * 0.55f)
        //    para crear un halo 3D real incluso alrededor de elementos centrados (100% mono compatible)
        float diffuseInput = (M * 0.45f) + (sMono * 0.55f);
        diffusionPreDelayBuffer[diffusionWritePos] = diffuseInput;
        int readPos = (diffusionWritePos - diffusionPreDelaySamples + preDelayBufSize) % preDelayBufSize;
        float delayedS = diffusionPreDelayBuffer[readPos];
        diffusionWritePos = (diffusionWritePos + 1) % preDelayBufSize;

        float apOut = delayedS;
        for (int k = 0; k < numAllPass; ++k)
            apOut = allPassChain[k].processSample (apOut);

        float deltaDiffuse = diffusionLowPass.processSample (apOut);

        // 3. Ganancia Side banda ancha + bandas Delta con empuje real estilo Billboard
        float sideBaseGain = 1.0f + (spatialNorm * 1.35f); // hasta 2.35x (+7.4 dB)
        float deltaS = deltaPresence * (spatialNorm * 0.45f)
                     + deltaAir      * (spatialNorm * 0.70f + clarityAirParam * 0.04f)
                     + deltaDiffuse  * (spatialNorm * 0.35f);

        float sCandidate = (sMono * sideBaseGain + deltaS) * sideDucking;

        // --- 3. ENVELOPE-FOLLOWER RMS CORRELATION GUARD ---
        // Mathematical limit: E_S_max = 0.775 * E_M ensures rho >= +0.25
        float absMidVal  = std::abs (enhancedM);
        float absSideVal = std::abs (sCandidate);

        midEnergyRms  = (absMidVal > midEnergyRms)
                      ? (alphaEnvAttack * midEnergyRms + (1.0f - alphaEnvAttack) * absMidVal)
                      : (alphaEnvRelease * midEnergyRms + (1.0f - alphaEnvRelease) * absMidVal);

        sideEnergyRms = (absSideVal > sideEnergyRms)
                      ? (alphaEnvAttack * sideEnergyRms + (1.0f - alphaEnvAttack) * absSideVal)
                      : (alphaEnvRelease * sideEnergyRms + (1.0f - alphaEnvRelease) * absSideVal);

        float targetGuard = 1.0f;
        if (guardActive && midEnergyRms > 1e-4f)
        {
            float maxAllowedSideEnergy = 0.775f * midEnergyRms;
            if (sideEnergyRms > maxAllowedSideEnergy && sideEnergyRms > 1e-5f)
                targetGuard = maxAllowedSideEnergy / sideEnergyRms;
        }

        // Smooth guard gain transition
        if (targetGuard < guardGainSmoothed)
            guardGainSmoothed = alphaGuardFast * guardGainSmoothed + (1.0f - alphaGuardFast) * targetGuard;
        else
            guardGainSmoothed = alphaGuardSlow * guardGainSmoothed + (1.0f - alphaGuardSlow) * targetGuard;

        float finalS = sCandidate * guardGainSmoothed;

        // --- 4. M/S RECONSTRUCTION ---
        float finalL = enhancedM + finalS;
        float finalR = enhancedM - finalS;

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

    // 5. Headless Auto-Gain Perceptual Matching
    float outEnergy = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        outEnergy += outL[i] * outL[i] + (outR != nullptr ? outR[i] * outR[i] : outL[i] * outL[i]);
    float outRms = std::sqrt (outEnergy / (2.0f * numSamples + 1e-9f));
    outRmsSmoothed = (outRmsSmoothed < 1e-6f) ? outRms : (0.97f * outRmsSmoothed + 0.03f * outRms);

    if (inRmsSmoothed > 1e-4f && outRmsSmoothed > 1e-4f)
    {
        float targetAutoGain = inRmsSmoothed / outRmsSmoothed;
        targetAutoGain = juce::jlimit (0.70f, 1.25f, targetAutoGain);
        autoGainFactor = 0.98f * autoGainFactor + 0.02f * targetAutoGain;
    }

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
