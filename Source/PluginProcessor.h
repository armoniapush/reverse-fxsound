#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

// Schroeder All-Pass Filter for Phase-Coherent Spatial Diffusion
struct SchroederAllPass
{
    std::vector<float> buffer;
    int maxDelay = 0;
    int writeIndex = 0;
    float g = 0.5f;

    void prepare (int delaySamples, float gain)
    {
        maxDelay = juce::jmax (1, delaySamples);
        buffer.assign (maxDelay + 1, 0.0f);
        g = gain;
        writeIndex = 0;
    }

    inline float processSample (float in) noexcept
    {
        int readIndex = (writeIndex - maxDelay + static_cast<int> (buffer.size())) % static_cast<int> (buffer.size());
        float bufOut = buffer[readIndex];
        float out = -g * in + bufOut;
        buffer[writeIndex] = in + g * bufOut;
        writeIndex = (writeIndex + 1) % static_cast<int> (buffer.size());
        return out;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }
};

class FxSoundAudioProcessor : public juce::AudioProcessor
{
public:
    FxSoundAudioProcessor();
    ~FxSoundAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    float getLiveCorrelation() const { return liveCorrelation.load (std::memory_order_relaxed); }

    // Visual FFT Data for UI
    void copyScopeData (std::array<float, 64>& dest);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // APVTS
    juce::AudioProcessorValueTreeState apvts;

    // --- Dynamic Phase Architecture (FabFilter Pro-MB Style) ---
    // 1. Elliptical Filter: High-Pass applied ONLY to Side channel
    juce::dsp::LinkwitzRileyFilter<float> sideEllipticalHPF;

    // 2. Parallel Delta Filters on Side channel
    juce::dsp::IIR::Filter<float> sidePresenceBP; // 400Hz - 3.5kHz
    juce::dsp::IIR::Filter<float> sideAirHP;       // > 4kHz

    // 3. Schroeder Diffuse All-Pass Chain for Spatial Depth
    static constexpr int numAllPass = 4;
    std::array<SchroederAllPass, numAllPass> allPassChain;
    std::vector<float> diffusionPreDelayBuffer;
    int diffusionPreDelaySamples = 0;
    int diffusionWritePos = 0;
    juce::dsp::IIR::Filter<float> diffusionLowPass; // ~9kHz smooth damping

    // 4. Crest Factor Transient Detector on Mid channel
    float peakFastMid = 0.0f;
    float rmsSlowMid  = 0.0f;
    float alphaFastAttack = 0.0f;
    float alphaFastRelease = 0.0f;
    float alphaRms = 0.0f;

    // 5. Envelope-Follower RMS Correlation Guard (Sample-rate dependent musical ballistics)
    float midEnergyRms = 0.0f;
    float sideEnergyRms = 0.0f;
    float guardGainSmoothed = 1.0f;
    float alphaEnvSlow = 0.0f;
    float alphaGuardAttack = 0.0f;
    float alphaGuardRelease = 0.0f;

    // 6. Headless Auto-Gain Perceptual Matching
    float inRmsSmoothed = 0.0f;
    float outRmsSmoothed = 0.0f;
    float autoGainFactor = 1.0f;

    // 7. FFT Spectrum Analysis (1024 points)
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    float fifo [fftSize] {};
    float fftData [2 * fftSize] {};
    int fifoIndex = 0;
    std::atomic<bool> nextFFTBlockReady { false };
    std::array<float, 64> scopeLevels {};
    juce::SpinLock scopeLock;

    void pushNextSampleIntoFifo (float sample) noexcept;
    void processFFT();

    // Real-Time Pearson Correlation
    std::atomic<float> liveCorrelation { 1.0f };
    float smoothedCorrelation = 1.0f;
    float sumDot = 0.0f;
    float sumL2 = 0.0f;
    float sumR2 = 0.0f;
    int correlationSampleCount = 0;

    // Mastering-Grade Analog Soft Knee
    static inline float cleanAnalogSaturation (float x)
    {
        constexpr float threshold = 0.70f;
        float absX = std::abs (x);
        if (absX <= threshold)
            return x;
        float excess = absX - threshold;
        float compressed = threshold + 0.28f * std::tanh (excess / 0.28f);
        return (x > 0.0f) ? compressed : -compressed;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessor)
};
