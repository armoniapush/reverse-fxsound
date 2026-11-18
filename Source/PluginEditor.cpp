#include "PluginProcessor.h"
#include "PluginEditor.h"

CustomRotaryLookAndFeel::CustomRotaryLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff00d2ff));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00d2ff));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2d323c));
    setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00d2ff));
}

void CustomRotaryLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                float sliderPosProportional, float rotaryStartAngle,
                                                float rotaryEndAngle, juce::Slider& slider)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Track Background
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff252a34));
    g.strokePath (backgroundArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value Arc (Glow Cyan / Orange)
    if (sliderPosProportional > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.isEnabled() ? juce::Colour (0xff00d2ff) : juce::Colours::grey);
        g.strokePath (valueArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Dial Body
    g.setColour (juce::Colour (0xff1c2027));
    g.fillEllipse (rx + 5.0f, ry + 5.0f, rw - 10.0f, rw - 10.0f);
    g.setColour (juce::Colour (0xff383e4a));
    g.drawEllipse (rx + 5.0f, ry + 5.0f, rw - 10.0f, rw - 10.0f, 1.5f);

    // Pointer Needle
    juce::Path pointer;
    auto pointerLength = radius * 0.55f;
    auto pointerThickness = 3.0f;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius + 8.0f, pointerThickness, pointerLength, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (slider.isEnabled() ? juce::Colours::white : juce::Colours::grey);
    g.fillPath (pointer);
}

void CustomRotaryLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                float sliderPos, float minSliderPos, float maxSliderPos,
                                                const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style);

    auto trackWidth = 4.0f;
    auto trackX = (float) x + ((float) width - trackWidth) * 0.5f;

    // Center zero line for EQ (-12 to +12)
    auto midY = (float) y + (float) height * 0.5f;

    // Track
    g.setColour (juce::Colour (0xff252a34));
    g.fillRoundedRectangle (trackX, (float) y + 6.0f, trackWidth, (float) height - 12.0f, 2.0f);

    // Active fill from midY to sliderPos
    g.setColour (slider.isEnabled() ? juce::Colour (0xff00d2ff).withAlpha (0.75f) : juce::Colours::grey);
    if (sliderPos < midY)
        g.fillRoundedRectangle (trackX, sliderPos, trackWidth, midY - sliderPos, 2.0f);
    else
        g.fillRoundedRectangle (trackX, midY, trackWidth, sliderPos - midY, 2.0f);

    // Thumb Cap
    auto thumbWidth = (float) width - 10.0f;
    auto thumbHeight = 12.0f;
    auto thumbX = (float) x + 5.0f;
    auto thumbY = sliderPos - thumbHeight * 0.5f;

    g.setColour (juce::Colour (0xff14171d));
    g.fillRoundedRectangle (thumbX, thumbY, thumbWidth, thumbHeight, 3.0f);
    g.setColour (slider.isEnabled() ? juce::Colour (0xff00d2ff) : juce::Colours::grey);
    g.drawRoundedRectangle (thumbX, thumbY, thumbWidth, thumbHeight, 3.0f, 1.5f);

    // Center indicator on thumb
    g.setColour (slider.isEnabled() ? juce::Colours::white : juce::Colours::grey);
    g.fillRect (thumbX + 4.0f, sliderPos - 0.5f, thumbWidth - 8.0f, 1.0f);
}

//==============================================================================

FxSoundAudioProcessorEditor::FxSoundAudioProcessorEditor (FxSoundAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLookAndFeel);

    // Title / Header
    titleLabel.setText ("FXSOUND", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d2ff));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("DSP AUDIO ENGINE (VST3 / CLAP)", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a5));
    addAndMakeVisible (subtitleLabel);

    // Power Toggle
    powerButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    powerButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e676));
    addAndMakeVisible (powerButton);
    powerAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getAPVTS(), "power", powerButton);

    // 5 Signature Effects + Output Gain
    setupRotaryControl (clarityCtrl,      "clarity",       "CLARITY");
    setupRotaryControl (ambienceCtrl,     "ambience",      "AMBIENCE");
    setupRotaryControl (surroundCtrl,     "surround",      "SURROUND");
    setupRotaryControl (dynamicBoostCtrl, "dynamic_boost", "DYNAMIC");
    setupRotaryControl (bassBoostCtrl,    "bass_boost",    "BASS BOOST");
    setupRotaryControl (outputGainCtrl,   "output_gain",   "OUTPUT", " dB");

    // Equalizer Toggle
    eqToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffcfd8dc));
    eqToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00d2ff));
    addAndMakeVisible (eqToggle);
    eqAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getAPVTS(), "eq_on", eqToggle);

    // 9 EQ Bands
    const juce::StringArray bandFrequencies = {
        "32Hz", "64Hz", "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz", "8kHz"
    };

    auto numBands = audioProcessor.getNumBands();
    for (int i = 0; i < numBands && i < bandFrequencies.size(); ++i)
    {
        setupEqBand (i, bandFrequencies[i]);
    }

    setSize (720, 520);
}

FxSoundAudioProcessorEditor::~FxSoundAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FxSoundAudioProcessorEditor::setupRotaryControl (EffectControl& ctrl, const juce::String& paramId, const juce::String& name, const juce::String& suffix)
{
    ctrl.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    ctrl.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (ctrl.slider);

    ctrl.label.setText (name, juce::dontSendNotification);
    ctrl.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    ctrl.label.setJustificationType (juce::Justification::centred);
    ctrl.label.setColour (juce::Label::textColourId, juce::Colour (0xffc4cdd8));
    addAndMakeVisible (ctrl.label);

    ctrl.valLabel.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    ctrl.valLabel.setJustificationType (juce::Justification::centred);
    ctrl.valLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d2ff));
    addAndMakeVisible (ctrl.valLabel);

    ctrl.slider.onValueChange = [&ctrl, suffix]() {
        ctrl.valLabel.setText (juce::String (ctrl.slider.getValue(), 1) + suffix, juce::dontSendNotification);
    };

    ctrl.attachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), paramId, ctrl.slider);
    ctrl.slider.onValueChange();
}

void FxSoundAudioProcessorEditor::setupEqBand (int index, const juce::String& freqText)
{
    auto band = std::make_unique<EqBandControl>();
    auto paramId = "eq_band_" + juce::String (index);

    band->slider.setSliderStyle (juce::Slider::LinearVertical);
    band->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (band->slider);

    band->freqLabel.setText (freqText, juce::dontSendNotification);
    band->freqLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    band->freqLabel.setJustificationType (juce::Justification::centred);
    band->freqLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a5));
    addAndMakeVisible (band->freqLabel);

    band->valLabel.setFont (juce::FontOptions (10.0f, juce::Font::plain));
    band->valLabel.setJustificationType (juce::Justification::centred);
    band->valLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d2ff));
    addAndMakeVisible (band->valLabel);

    band->slider.onValueChange = [b = band.get()]() {
        auto val = b->slider.getValue();
        b->valLabel.setText ((val > 0.0 ? "+" : "") + juce::String (val, 1) + " dB", juce::dontSendNotification);
    };

    band->attachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), paramId, band->slider);
    band->slider.onValueChange();

    eqBands.push_back (std::move (band));
}

void FxSoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient
    g.fillAll (juce::Colour (0xff121418));

    // Top Header Bar
    g.setColour (juce::Colour (0xff181b22));
    g.fillRect (0, 0, getWidth(), 60);
    g.setColour (juce::Colour (0xff252a34));
    g.drawHorizontalLine (60, 0.0f, (float) getWidth());

    // Section 1: Effects Panel
    auto effectsPanel = juce::Rectangle<int> (16, 75, getWidth() - 32, 175);
    g.setColour (juce::Colour (0xff181b22));
    g.fillRoundedRectangle (effectsPanel.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff2a303c));
    g.drawRoundedRectangle (effectsPanel.toFloat(), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff5f6b80));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ENHANCEMENT EFFECTS", effectsPanel.reduced (12, 6), juce::Justification::topLeft, true);

    // Section 2: Equalizer Panel
    auto eqPanel = juce::Rectangle<int> (16, 265, getWidth() - 32, 235);
    g.setColour (juce::Colour (0xff181b22));
    g.fillRoundedRectangle (eqPanel.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff2a303c));
    g.drawRoundedRectangle (eqPanel.toFloat(), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff5f6b80));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("PARAMETRIC GRAPHIC EQUALIZER", eqPanel.reduced (12, 6), juce::Justification::topLeft, true);
}

void FxSoundAudioProcessorEditor::resized()
{
    // Header
    titleLabel.setBounds (20, 10, 140, 26);
    subtitleLabel.setBounds (22, 34, 250, 18);
    powerButton.setBounds (getWidth() - 120, 16, 100, 30);

    // 6 Rotary Knobs (5 Effects + 1 Master)
    std::vector<EffectControl*> controls = {
        &clarityCtrl, &ambienceCtrl, &surroundCtrl, &dynamicBoostCtrl, &bassBoostCtrl, &outputGainCtrl
    };

    int effectsTop = 105;
    int knobWidth = (getWidth() - 64) / static_cast<int> (controls.size());

    for (size_t i = 0; i < controls.size(); ++i)
    {
        int x = 24 + static_cast<int> (i) * knobWidth;
        controls[i]->label.setBounds (x, effectsTop, knobWidth, 16);
        controls[i]->slider.setBounds (x + (knobWidth - 75) / 2, effectsTop + 20, 75, 75);
        controls[i]->valLabel.setBounds (x, effectsTop + 100, knobWidth, 16);
    }

    // EQ Toggle
    eqToggle.setBounds (getWidth() - 190, 272, 170, 24);

    // EQ Sliders
    int eqTop = 305;
    int eqHeight = 155;
    auto numBands = eqBands.size();
    if (numBands > 0)
    {
        int bandWidth = (getWidth() - 64) / static_cast<int> (numBands);
        for (size_t i = 0; i < numBands; ++i)
        {
            int x = 24 + static_cast<int> (i) * bandWidth;
            eqBands[i]->valLabel.setBounds (x, eqTop, bandWidth, 14);
            eqBands[i]->slider.setBounds (x + (bandWidth - 28) / 2, eqTop + 16, 28, eqHeight - 40);
            eqBands[i]->freqLabel.setBounds (x, eqTop + eqHeight - 20, bandWidth, 16);
        }
    }
}
