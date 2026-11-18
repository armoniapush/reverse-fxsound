#include "PluginProcessor.h"
#include "PluginEditor.h"

FxSoundAudioProcessorEditor::FxSoundAudioProcessorEditor (FxSoundAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("FxSound VST3 / CLAP", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible (titleLabel);

    setSize (450, 300);
}

FxSoundAudioProcessorEditor::~FxSoundAudioProcessorEditor()
{
}

void FxSoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1e23));

    g.setColour (juce::Colours::grey.withAlpha (0.3f));
    g.drawRect (getLocalBounds(), 1);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (14.0f);
    g.drawText ("DSP Engine: FxSound Submodule Ready", getLocalBounds().reduced (20), juce::Justification::centredBottom, true);
}

void FxSoundAudioProcessorEditor::resized()
{
    titleLabel.setBounds (0, 30, getWidth(), 40);
}
