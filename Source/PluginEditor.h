#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

//==============================================================================
// Voger Design Inspired LookAndFeel for Hardware-Grade Skeuomorphic Dials
//==============================================================================
class ModernSpatialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernSpatialLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

//==============================================================================
// Precision Phase Correlation Bar with Calibration Ticks
//==============================================================================
class CorrelationMeterComponent : public juce::Component
{
public:
    CorrelationMeterComponent() = default;
    void setCorrelation (float val)
    {
        currentValue = juce::jlimit (-1.0f, 1.0f, val);
        repaint();
    }

    void paint (juce::Graphics& g) override;

private:
    float currentValue = 1.0f;
};

//==============================================================================
// Holographic Smoked-Glass FFT Spectrum Analyzer
//==============================================================================
class SpectrumVisualizerComponent : public juce::Component
{
public:
    SpectrumVisualizerComponent() = default;
    void updateData (const std::array<float, 64>& data, float correlation)
    {
        levels = data;
        currentCorrelation = correlation;
        repaint();
    }

    void paint (juce::Graphics& g) override;

private:
    std::array<float, 64> levels {};
    float currentCorrelation = 1.0f;
};

//==============================================================================
// Main Editor
//==============================================================================
class FxSoundAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FxSoundAudioProcessorEditor (FxSoundAudioProcessor&);
    ~FxSoundAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;

    FxSoundAudioProcessor& audioProcessor;
    ModernSpatialLookAndFeel lookAndFeel;

    // Header Console
    juce::Label titleLabel;
    juce::Label badgeLabel;
    juce::ToggleButton powerButton { "POWER" };
    std::unique_ptr<ButtonAttachment> powerAttachment;

    // Hero 3D Spatial Knob
    juce::Slider masterSpatialSlider;
    juce::Label masterSpatialLabel;
    juce::Label masterValueLabel;
    std::unique_ptr<SliderAttachment> spatialAttachment;

    // Glass Analyzer Chamber (FFT Spectrum + Phase Bar)
    SpectrumVisualizerComponent spectrumVisualizer;
    CorrelationMeterComponent correlationMeter;
    juce::Label correlationTitleLabel;
    juce::Label correlationReadoutLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessorEditor)
};
