#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

class ModernSpatialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernSpatialLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
};

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

    // Header
    juce::Label titleLabel;
    juce::Label badgeLabel;
    juce::ToggleButton powerButton { "POWER" };
    std::unique_ptr<ButtonAttachment> powerAttachment;

    // Master Macro Knob
    juce::Slider masterSpatialSlider;
    juce::Label masterSpatialLabel;
    juce::Label masterValueLabel;
    std::unique_ptr<SliderAttachment> spatialAttachment;

    // Real-Time Stylized FFT Spectrum Visualizer
    SpectrumVisualizerComponent spectrumVisualizer;

    // Live Phase Correlation Meter
    CorrelationMeterComponent correlationMeter;
    juce::Label correlationTitleLabel;
    juce::Label correlationReadoutLabel;

    // Bottom Quick-Tweak Controls
    struct MiniControl
    {
        juce::Slider slider;
        juce::Label title;
        juce::Label valLabel;
        std::unique_ptr<SliderAttachment> attachment;
    };

    MiniControl vocalPunchCtrl;
    MiniControl monoFreqCtrl;
    MiniControl clarityAirCtrl;
    MiniControl outputGainCtrl;

    void setupMiniDial (MiniControl& ctrl, const juce::String& paramId, const juce::String& title, const juce::String& suffix = "");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessorEditor)
};
