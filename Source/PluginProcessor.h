#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <DfxDsp.h>
#include <vector>
#include <atomic>

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

    // DFX DSP Engine
    DfxDsp dfxDsp;
    std::vector<float> interleavedDfxBuffer;

    // APVTS
    juce::AudioProcessorValueTreeState apvts;

    // Linkwitz-Riley 4th-Order Tri-Band Crossovers (LR4)
    juce::dsp::LinkwitzRileyFilter<float> xo1LowL, xo1LowR;
    juce::dsp::LinkwitzRileyFilter<float> xo1HighL, xo1HighR;
    juce::dsp::LinkwitzRileyFilter<float> xo2LowL, xo2LowR;
    juce::dsp::LinkwitzRileyFilter<float> xo2HighL, xo2HighR;

    // Audio Buffers
    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> midBuffer;
    juce::AudioBuffer<float> highBuffer;
    juce::AudioBuffer<float> upperBuffer;

    // Stereoize Phase-Coherent Decorrelator (Ozone Imager Architecture)
    std::vector<float> decorrBufferL;
    std::vector<float> decorrBufferR;
    int decorrWritePos = 0;
    int decorrDelayL = 0;
    int decorrDelayR = 0;
    int decorrBufferSize = 0;

    // FFT Analysis (1024 points, 64 logarithmic display bins)
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

    // Real-Time Correlation Calculation
    std::atomic<float> liveCorrelation { 1.0f };
    float smoothedCorrelation = 1.0f;
    float sumDot = 0.0f;
    float sumL2 = 0.0f;
    float sumR2 = 0.0f;
    int correlationSampleCount = 0;

    // Crest Factor & Transient Envelope (Ozone Impact Architecture)
    float peakFastMid = 0.0f;
    float rmsSlowMid = 0.0f;
    float alphaFastAttack = 0.0f;
    float alphaFastRelease = 0.0f;
    float alphaRms = 0.0f;

    // Headless Auto-Gain Perceptual Matching (RMS Leveling)
    float inRmsSmoothed = 0.0f;
    float outRmsSmoothed = 0.0f;
    float autoGainFactor = 1.0f;

    // Mastering-Grade Analog Soft Knee (Zero harsh clipping)
    static inline float cleanAnalogSaturation (float x)
    {
        constexpr float threshold = 0.65f;
        float absX = std::abs (x);
        if (absX <= threshold)
            return x;
        float excess = absX - threshold;
        float compressed = threshold + 0.30f * std::tanh (excess / 0.30f);
        return (x > 0.0f) ? compressed : -compressed;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessor)
};
