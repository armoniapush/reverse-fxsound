#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class FxSoundAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxSoundAudioProcessorEditor (FxSoundAudioProcessor&);
    ~FxSoundAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FxSoundAudioProcessor& audioProcessor;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxSoundAudioProcessorEditor)
};
