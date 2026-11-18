#include "PluginProcessor.h"
#include "PluginEditor.h"

ModernSpatialLookAndFeel::ModernSpatialLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff20242c));
    setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e676));
}

void ModernSpatialLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPosProportional, float rotaryStartAngle,
                                                 float rotaryEndAngle, juce::Slider& slider)
{
    bool isMaster = (width > 120);

    auto radius = (float) juce::jmin (width / 2, height / 2) - (isMaster ? 8.0f : 4.0f);
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Track Background Arc
    juce::Path backgroundArc;
    float trackThickness = isMaster ? 10.0f : 5.0f;
    backgroundArc.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff1f242d));
    g.strokePath (backgroundArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active Glowing Value Arc
    if (sliderPosProportional > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                0.0f, rotaryStartAngle, angle, true);

        if (isMaster)
        {
            // Neon Cyan to Magenta Gradient for Master Spatial
            juce::ColourGradient grad (juce::Colour (0xff00e5ff), centreX - radius, centreY,
                                       juce::Colour (0xffd500f9), centreX + radius, centreY, false);
            g.setGradientFill (grad);
        }
        else
        {
            g.setColour (slider.isEnabled() ? juce::Colour (0xff00e5ff) : juce::Colours::grey);
        }
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Inner Dial Body
    float dialInset = isMaster ? 14.0f : 7.0f;
    g.setColour (juce::Colour (0xff14171d));
    g.fillEllipse (rx + dialInset, ry + dialInset, rw - dialInset * 2.0f, rw - dialInset * 2.0f);

    g.setColour (juce::Colour (0xff2d3340));
    g.drawEllipse (rx + dialInset, ry + dialInset, rw - dialInset * 2.0f, rw - dialInset * 2.0f, isMaster ? 2.0f : 1.0f);

    // Indicator Dot / Line
    juce::Path pointer;
    auto pointerLength = isMaster ? 16.0f : 8.0f;
    auto pointerThickness = isMaster ? 4.0f : 2.5f;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius + dialInset + 2.0f, pointerThickness, pointerLength, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (slider.isEnabled() ? juce::Colours::white : juce::Colours::grey);
    g.fillPath (pointer);
}

//==============================================================================
// Correlation Meter
//==============================================================================

void CorrelationMeterComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float h = b.getHeight();
    float w = b.getWidth();

    // Background track
    g.setColour (juce::Colour (0xff15181e));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (juce::Colour (0xff252a34));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    // Center divider (0.0 correlation)
    float zeroX = w * 0.5f;
    g.setColour (juce::Colour (0xff383e4a));
    g.drawVerticalLine (static_cast<int> (zeroX), 0.0f, h);

    // Zone background guides
    // -1 to 0 (danger red tint)
    g.setColour (juce::Colour (0xffe53935).withAlpha (0.08f));
    g.fillRect (0.0f, 0.0f, zeroX, h);

    // 0 to +1 (safe green/cyan tint)
    g.setColour (juce::Colour (0xff00e676).withAlpha (0.08f));
    g.fillRect (zeroX, 0.0f, zeroX, h);

    // Correlation Indicator Marker
    // Mapping: -1.0 -> 0.0, 0.0 -> zeroX, +1.0 -> w
    float markerX = ((currentValue + 1.0f) * 0.5f) * w;
    markerX = juce::jlimit (4.0f, w - 4.0f, markerX);

    // Color based on correlation value
    juce::Colour markerColor;
    if (currentValue >= 0.6f)
        markerColor = juce::Colour (0xff00e676); // Safe Billboard Green
    else if (currentValue >= 0.0f)
        markerColor = juce::Colour (0xffffb300); // Amber Caution
    else
        markerColor = juce::Colour (0xffff1744); // Phase Danger Red

    // Active fill from 0 to marker
    if (markerX >= zeroX)
    {
        g.setColour (markerColor.withAlpha (0.35f));
        g.fillRect (zeroX, 2.0f, markerX - zeroX, h - 4.0f);
    }
    else
    {
        g.setColour (markerColor.withAlpha (0.45f));
        g.fillRect (markerX, 2.0f, zeroX - markerX, h - 4.0f);
    }

    // Needle
    g.setColour (markerColor);
    g.fillRoundedRectangle (markerX - 2.5f, 1.0f, 5.0f, h - 2.0f, 2.0f);
}

//==============================================================================
// Editor
//==============================================================================

FxSoundAudioProcessorEditor::FxSoundAudioProcessorEditor (FxSoundAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Header Title
    titleLabel.setText ("FXSOUND", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (titleLabel);

    badgeLabel.setText ("BILLBOARD SPATIAL V0.0.2", juce::dontSendNotification);
    badgeLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    badgeLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd500f9));
    addAndMakeVisible (badgeLabel);

    // Power Button
    powerButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    powerButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e676));
    addAndMakeVisible (powerButton);
    powerAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getAPVTS(), "power", powerButton);

    // Master Spatial Slider
    masterSpatialSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSpatialSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterSpatialSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (masterSpatialSlider);

    masterSpatialLabel.setText ("SPATIAL", juce::dontSendNotification);
    masterSpatialLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    masterSpatialLabel.setJustificationType (juce::Justification::centred);
    masterSpatialLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (masterSpatialLabel);

    masterValueLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    masterValueLabel.setJustificationType (juce::Justification::centred);
    masterValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (masterValueLabel);

    masterSpatialSlider.onValueChange = [this]() {
        float val = static_cast<float> (masterSpatialSlider.getValue());
        masterValueLabel.setText (juce::String (val * 10.0f, 0) + "%", juce::dontSendNotification);
    };

    spatialAttachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), "spatial", masterSpatialSlider);
    masterSpatialSlider.onValueChange();

    // Correlation Meter
    addAndMakeVisible (correlationMeter);

    correlationTitleLabel.setText ("PHASE CORRELATION", juce::dontSendNotification);
    correlationTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    correlationTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a5));
    addAndMakeVisible (correlationTitleLabel);

    correlationReadoutLabel.setText ("+1.00 SAFE", juce::dontSendNotification);
    correlationReadoutLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    correlationReadoutLabel.setJustificationType (juce::Justification::right);
    correlationReadoutLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e676));
    addAndMakeVisible (correlationReadoutLabel);

    // Bottom Quick-Tweak Controls
    setupMiniDial (vocalPunchCtrl, "vocal_punch", "VOCAL PUNCH", "");
    setupMiniDial (monoFreqCtrl,   "mono_freq",   "MONO BASS",    " Hz");
    setupMiniDial (clarityAirCtrl, "clarity_air", "CLARITY AIR",  "");
    setupMiniDial (outputGainCtrl, "output_gain", "OUTPUT",       " dB");

    setSize (580, 500);
    startTimerHz (30); // 30 FPS update for phase meter
}

FxSoundAudioProcessorEditor::~FxSoundAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FxSoundAudioProcessorEditor::setupMiniDial (MiniControl& ctrl, const juce::String& paramId, const juce::String& title, const juce::String& suffix)
{
    ctrl.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    ctrl.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (ctrl.slider);

    ctrl.title.setText (title, juce::dontSendNotification);
    ctrl.title.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    ctrl.title.setJustificationType (juce::Justification::centred);
    ctrl.title.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a5));
    addAndMakeVisible (ctrl.title);

    ctrl.valLabel.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    ctrl.valLabel.setJustificationType (juce::Justification::centred);
    ctrl.valLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (ctrl.valLabel);

    ctrl.slider.onValueChange = [&ctrl, suffix]() {
        float val = static_cast<float> (ctrl.slider.getValue());
        if (suffix == " Hz")
            ctrl.valLabel.setText (juce::String (static_cast<int> (val)) + suffix, juce::dontSendNotification);
        else if (suffix == " dB")
            ctrl.valLabel.setText ((val > 0.0f ? "+" : "") + juce::String (val, 1) + suffix, juce::dontSendNotification);
        else
            ctrl.valLabel.setText (juce::String (val, 1) + suffix, juce::dontSendNotification);
    };

    ctrl.attachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), paramId, ctrl.slider);
    ctrl.slider.onValueChange();
}

void FxSoundAudioProcessorEditor::timerCallback()
{
    float corr = audioProcessor.getLiveCorrelation();
    correlationMeter.setCorrelation (corr);

    juce::String statusText;
    juce::Colour statusColor;
    if (corr >= 0.70f)
    {
        statusText = (corr >= 0.0f ? "+" : "") + juce::String (corr, 2) + " (OPTIMAL MONO SAFE)";
        statusColor = juce::Colour (0xff00e676);
    }
    else if (corr >= 0.35f)
    {
        statusText = (corr >= 0.0f ? "+" : "") + juce::String (corr, 2) + " (WIDE STEREO)";
        statusColor = juce::Colour (0xffffb300);
    }
    else
    {
        statusText = juce::String (corr, 2) + " (PHASE ATTENTION)";
        statusColor = juce::Colour (0xffff1744);
    }

    correlationReadoutLabel.setText (statusText, juce::dontSendNotification);
    correlationReadoutLabel.setColour (juce::Label::textColourId, statusColor);
}

void FxSoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colour (0xff0f1115));

    // Top Header
    g.setColour (juce::Colour (0xff14171d));
    g.fillRect (0, 0, getWidth(), 60);
    g.setColour (juce::Colour (0xff1e232c));
    g.drawHorizontalLine (60, 0.0f, static_cast<float> (getWidth()));

    // Master Hero Section
    auto heroRect = juce::Rectangle<int> (20, 75, getWidth() - 40, 240);
    g.setColour (juce::Colour (0xff14171d));
    g.fillRoundedRectangle (heroRect.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff252a34));
    g.drawRoundedRectangle (heroRect.toFloat(), 10.0f, 1.0f);

    // Bottom Tweak Section
    auto tweakRect = juce::Rectangle<int> (20, 365, getWidth() - 40, 115);
    g.setColour (juce::Colour (0xff14171d));
    g.fillRoundedRectangle (tweakRect.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff252a34));
    g.drawRoundedRectangle (tweakRect.toFloat(), 10.0f, 1.0f);
}

void FxSoundAudioProcessorEditor::resized()
{
    // Header
    titleLabel.setBounds (20, 10, 140, 24);
    badgeLabel.setBounds (22, 34, 200, 16);
    powerButton.setBounds (getWidth() - 110, 16, 90, 28);

    // Master Knob
    int knobSize = 160;
    int centerX = getWidth() / 2;
    masterSpatialSlider.setBounds (centerX - knobSize / 2, 90, knobSize, knobSize);
    masterValueLabel.setBounds (centerX - 50, 155, 100, 26);
    masterSpatialLabel.setBounds (centerX - 60, 255, 120, 20);

    // Phase Correlation Meter
    int meterY = 325;
    correlationTitleLabel.setBounds (24, meterY, 150, 16);
    correlationReadoutLabel.setBounds (getWidth() - 250, meterY, 226, 16);
    correlationMeter.setBounds (24, meterY + 20, getWidth() - 48, 12);

    // Bottom Controls (4 Dials)
    int dockY = 378;
    int colWidth = (getWidth() - 40) / 4;

    std::vector<MiniControl*> dials = { &vocalPunchCtrl, &monoFreqCtrl, &clarityAirCtrl, &outputGainCtrl };
    for (size_t i = 0; i < dials.size(); ++i)
    {
        int x = 20 + static_cast<int> (i) * colWidth;
        dials[i]->title.setBounds (x, dockY, colWidth, 14);
        dials[i]->slider.setBounds (x + (colWidth - 56) / 2, dockY + 16, 56, 56);
        dials[i]->valLabel.setBounds (x, dockY + 74, colWidth, 14);
    }
}
