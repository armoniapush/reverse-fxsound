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

    // 8. STFT + PAE (PCA 2x2) + Binaural Projection Initialization
    for (int n = 0; n < paeFftSize; ++n)
        paeWindow[n] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * static_cast<float> (n) / static_cast<float> (paeFftSize)));

    paeInL.fill (0.0f);
    paeInR.fill (0.0f);
    paeOutL.fill (0.0f);
    paeOutR.fill (0.0f);
    paeFftBufL.fill (0.0f);
    paeFftBufR.fill (0.0f);
    scmLL.fill (0.0f);
    scmRR.fill (0.0f);
    scmLR_re.fill (0.0f);
    scmLR_im.fill (0.0f);
    paeInWritePos = 0;
    paeOutReadPos = 0;
    paeHopCounter = 0;

    setLatencySamples (paeFftSize - paeHopSize);
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

void FxSoundAudioProcessor::processPaeFrame (float spatialNorm, float monoFreqHz, float clarityAir)
{
    const float sr = static_cast<float> (getSampleRate() > 0.0 ? getSampleRate() : 44100.0);
    const float binHz = sr / static_cast<float> (paeFftSize);

    // 1. Copy windowed input frame from ring buffer
    paeFftBufL.fill (0.0f);
    paeFftBufR.fill (0.0f);
    for (int n = 0; n < paeFftSize; ++n)
    {
        int idx = (paeInWritePos + n) % paeFftSize;
        paeFftBufL[n] = paeInL[idx] * paeWindow[n];
        paeFftBufR[n] = paeInR[idx] * paeWindow[n];
    }

    // 2. Forward Real-to-Complex FFT (interleaved complex format in JUCE)
    paeFft.performRealOnlyForwardTransform (paeFftBufL.data(), true);
    paeFft.performRealOnlyForwardTransform (paeFftBufR.data(), true);

    constexpr float scmAlpha = 0.85f;
    constexpr float scmBeta  = 1.0f - scmAlpha;
    constexpr float headRadius = 0.0875f;
    constexpr float speedOfSound = 343.0f;
    constexpr float eps = 1e-12f;

    // 3. Per-bin PAE (2x2 PCA) + Binaural ITD/ILD Projection
    for (int k = 0; k < paeNumBins; ++k)
    {
        float freq = static_cast<float> (k) * binHz;
        float xL_re = paeFftBufL[2 * k];
        float xL_im = paeFftBufL[2 * k + 1];
        float xR_re = paeFftBufR[2 * k];
        float xR_im = paeFftBufR[2 * k + 1];

        // Update smoothed 2x2 Spatial Covariance Matrix R(k, m)
        float pLL = xL_re * xL_re + xL_im * xL_im;
        float pRR = xR_re * xR_re + xR_im * xR_im;
        float pLR_re = xL_re * xR_re + xL_im * xR_im;
        float pLR_im = xL_im * xR_re - xL_re * xR_im;

        float rLL = scmAlpha * scmLL[k] + scmBeta * pLL;
        float rRR = scmAlpha * scmRR[k] + scmBeta * pRR;
        float rLR_re = scmAlpha * scmLR_re[k] + scmBeta * pLR_re;
        float rLR_im = scmAlpha * scmLR_im[k] + scmBeta * pLR_im;

        scmLL[k] = rLL;
        scmRR[k] = rRR;
        scmLR_re[k] = rLR_re;
        scmLR_im[k] = rLR_im;

        // Keep sub-bass below monoFreqHz untouched (mono-safe)
        if (freq < monoFreqHz || spatialNorm <= 0.001f)
            continue;

        // Dominant eigenvector v1 of 2x2 Hermitian matrix R
        float tr = rLL + rRR;
        float diff = rLL - rRR;
        float magLR2 = rLR_re * rLR_re + rLR_im * rLR_im;
        float disc = std::sqrt (diff * diff + 4.0f * magLR2);
        float lambda1 = 0.5f * (tr + disc);

        float v1L_re = 1.0f, v1L_im = 0.0f;
        float v1R_re = 0.0f, v1R_im = 0.0f;

        if (magLR2 > eps)
        {
            float a = lambda1 - rRR;
            // v1 = [a, conj(rLR)]
            float norm = std::sqrt (a * a + magLR2 + eps);
            v1L_re = a / norm;
            v1L_im = 0.0f;
            v1R_re = rLR_re / norm;
            v1R_im = -rLR_im / norm;
        }
        else if (rRR > rLL)
        {
            v1L_re = 0.0f;
            v1R_re = 1.0f;
        }

        // Primary scalar projection: p_s = v1^H * x
        float ps_re = (v1L_re * xL_re + v1L_im * xL_im) + (v1R_re * xR_re + v1R_im * xR_im);
        float ps_im = (v1L_re * xL_im - v1L_im * xL_re) + (v1R_re * xR_im - v1R_im * xR_re);

        // Primary vector p = v1 * p_s
        float pL_re = v1L_re * ps_re - v1L_im * ps_im;
        float pL_im = v1L_re * ps_im + v1L_im * ps_re;
        float pR_re = v1R_re * ps_re - v1R_im * ps_im;
        float pR_im = v1R_re * ps_im + v1R_im * ps_re;

        // Ambient component a = x - p
        float aL_re = xL_re - pL_re;
        float aL_im = xL_im - pL_im;
        float aR_re = xR_re - pR_re;
        float aR_im = xR_im - pR_im;

        // Panning index in [-1, 1]
        float eL = v1L_re * v1L_re + v1L_im * v1L_im;
        float eR = v1R_re * v1R_re + v1R_im * v1R_im;
        float pan = (eR - eL) / (eL + eR + eps);
        float absPan = std::abs (pan);

        // Protect center objects (|pan| < 0.18: vocals, kick, bass)
        float lateralMask = juce::jlimit (0.0f, 1.0f, (absPan - 0.18f) / 0.45f);

        // Woodworth ITD & ILD Binaural Projection for lateral objects
        float theta = pan * spatialNorm * (juce::MathConstants<float>::halfPi * 0.85f) * lateralMask;
        float itdSec = (headRadius / speedOfSound) * (theta + std::sin (theta));
        float halfPhase = juce::MathConstants<float>::pi * freq * itdSec;

        float cosP = std::cos (halfPhase);
        float sinP = std::sin (halfPhase);

        // Frequency-dependent ILD (head shadow above ~1.5 kHz)
        float ildShelf = freq / (freq + 1500.0f);
        float ildDb = 3.5f * spatialNorm * lateralMask * ildShelf * pan;
        float gainL = std::pow (10.0f, -ildDb * 0.05f);
        float gainR = std::pow (10.0f,  ildDb * 0.05f);

        // Apply +halfPhase to L, -halfPhase to R (or vice versa via sign of theta)
        float pL_rot_re = (pL_re * cosP - pL_im * sinP) * gainL;
        float pL_rot_im = (pL_re * sinP + pL_im * cosP) * gainL;
        float pR_rot_re = (pR_re * cosP + pR_im * sinP) * gainR;
        float pR_rot_im = (pR_im * cosP - pR_re * sinP) * gainR;

        // Ambient expansion + gentle high-frequency air boost
        float airBoost = 1.0f + (freq > 4000.0f ? clarityAir * 0.03f : 0.0f);
        float ambGain = (1.0f + spatialNorm * 1.35f) * airBoost;

        paeFftBufL[2 * k]     = pL_rot_re + aL_re * ambGain;
        paeFftBufL[2 * k + 1] = pL_rot_im + aL_im * ambGain;
        paeFftBufR[2 * k]     = pR_rot_re + aR_re * ambGain;
        paeFftBufR[2 * k + 1] = pR_rot_im + aR_im * ambGain;
    }

    // 4. Inverse Real-Only FFT
    paeFft.performRealOnlyInverseTransform (paeFftBufL.data());
    paeFft.performRealOnlyInverseTransform (paeFftBufR.data());

    // 5. Overlap-Add (COLA normalization for 75% overlap Hann^2 = 1.5)
    constexpr float olaNorm = 1.0f / 1.5f;
    for (int n = 0; n < paeFftSize; ++n)
    {
        int idx = (paeOutReadPos + n) % paeFftSize;
        paeOutL[idx] += paeFftBufL[n] * paeWindow[n] * olaNorm;
        paeOutR[idx] += paeFftBufR[n] * paeWindow[n] * olaNorm;
    }
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

        // 1. Feed STFT input ring buffer & read overlap-add output
        paeInL[paeInWritePos] = xL;
        paeInR[paeInWritePos] = xR;
        paeInWritePos = (paeInWritePos + 1) % paeFftSize;

        float rawL = paeOutL[paeOutReadPos];
        float rawR = paeOutR[paeOutReadPos];
        paeOutL[paeOutReadPos] = 0.0f;
        paeOutR[paeOutReadPos] = 0.0f;
        paeOutReadPos = (paeOutReadPos + 1) % paeFftSize;

        if (++paeHopCounter >= paeHopSize)
        {
            paeHopCounter = 0;
            processPaeFrame (spatialNorm, monoFreqParam, clarityAirParam);
        }

        // 2. Optional RMS L2 Correlation Guard on M/S
        if (guardActive)
        {
            float M = (rawL + rawR) * 0.5f;
            float S = (rawL - rawR) * 0.5f;

            midEnergyRms  = alphaEnvSlow * midEnergyRms  + (1.0f - alphaEnvSlow) * (M * M);
            sideEnergyRms = alphaEnvSlow * sideEnergyRms + (1.0f - alphaEnvSlow) * (S * S);

            float midRms  = std::sqrt (midEnergyRms + 1e-9f);
            float sideRms = std::sqrt (sideEnergyRms + 1e-9f);
            float maxSide = 0.694f * midRms;

            float targetGuard = (midRms > 1e-4f && sideRms > maxSide && sideRms > 1e-5f)
                              ? (maxSide / sideRms)
                              : 1.0f;
            float aG = (targetGuard < guardGainSmoothed) ? alphaGuardAttack : alphaGuardRelease;
            guardGainSmoothed = aG * guardGainSmoothed + (1.0f - aG) * targetGuard;

            float finalS = S * guardGainSmoothed;
            rawL = M + finalS;
            rawR = M - finalS;
        }

        // 3. Soft-clipper de seguridad a -0.35 dBFS (cero clipping digital)
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
