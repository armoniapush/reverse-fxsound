#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

class CustomRotaryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomRotaryLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

class FxSoundAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxSoundAudioProcessorEditor (FxSoundAudioProcessor&);
    ~FxSoundAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    FxSoundAudioProcessor& audioProcessor;
    CustomRotaryLookAndFeel customLookAndFeel;

    // Header Components
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton powerButton { "POWER" };
    std::unique_ptr<ButtonAttachment> powerAttachment;

    // 5 Signature Effects + Output Gain
    struct EffectControl
    {
        juce::Slider slider;
        juce::Label label;
        juce::Label valLabel;
        std::unique_ptr<SliderAttachment> attachment;
    };

    EffectControl clarityCtrl;
    EffectControl ambienceCtrl;
    EffectControl surroundCtrl;
    EffectControl dynamicBoostCtrl;
    EffectControl bassBoostCtrl;
    EffectControl outputGainCtrl;

    // EQ Section
    juce::ToggleButton eqToggle { "EQUALIZER ACTIVE" };
    std::unique_ptr<ButtonAttachment> eqAttachment;

    struct EqBandControl
    {
        juce::Slider slider;
        juce::Label freqLabel;
        juce::Label valLabel;
        std::unique_ptr<SliderAttachment> attachment;
    };
    std::vector<std::unique_ptr<EqBandControl>> eqBands;

    void setupRotaryControl (EffectControl& ctrl, const juce::String& paramId, const juce::String& name, const juce::String& suffix = "");
    void setupEqBand (int index, const juce::String& freqText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessorEditor)
};
