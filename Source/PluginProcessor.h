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

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DFX DSP Engine
    DfxDsp dfxDsp;
    std::vector<float> interleavedDfxBuffer;

    // APVTS
    juce::AudioProcessorValueTreeState apvts;

    // Linkwitz-Riley 4th-Order Crossover (LR4)
    juce::dsp::LinkwitzRileyFilter<float> lowPassL, lowPassR;
    juce::dsp::LinkwitzRileyFilter<float> highPassL, highPassR;

    // Vocal Mid-Channel Bandpass (Spectre-style)
    juce::dsp::IIR::Filter<float> midBandpass;

    // Audio Buffers for multiband routing
    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> highBuffer;

    // Real-Time Correlation Calculation
    std::atomic<float> liveCorrelation { 1.0f };
    float smoothedCorrelation = 1.0f;
    float sumDot = 0.0f;
    float sumL2 = 0.0f;
    float sumR2 = 0.0f;
    int correlationSampleCount = 0;

    // Transient & Sustain Dual Envelope Follower (Ozone 12 architecture)
    float envFast = 0.0f;
    float envSlow = 0.0f;
    float alphaFast = 0.0f;
    float alphaSlow = 0.0f;

    // Headless Auto-Gain Perceptual Matching (RMS Leveling)
    float inRmsSmoothed = 0.0f;
    float outRmsSmoothed = 0.0f;
    float autoGainFactor = 1.0f;

    // Headless Adaptive Phase Guard
    float phaseGuardDucking = 1.0f;

    // Solid Saturation Function
    static inline float applySolidSaturation (float sample, float drive)
    {
        float in = sample * drive;
        // Transistor soft-knee transfer function with 3rd-order harmonic dominance
        float sat = std::tanh (in) + 0.12f * (in * in * (in > 0.0f ? 1.0f : -1.0f));
        return sat * 0.88f;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessor)
};
