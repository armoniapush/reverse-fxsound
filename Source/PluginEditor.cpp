#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// ModernSpatialLookAndFeel (Voger Design: Light, Materials & Skeuomorphism)
//==============================================================================

ModernSpatialLookAndFeel::ModernSpatialLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff1c222e));
    setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e676));
}

void ModernSpatialLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    bool isOn = button.getToggleState();

    // Sleek anodized aluminum pill button
    g.setColour (juce::Colour (0xff121620));
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    // Bevel rim (Top-down lighting)
    juce::ColourGradient borderGrad (juce::Colour (0xff2d374a), bounds.getX(), bounds.getY(),
                                    juce::Colour (0xff080a0f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (borderGrad);
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.2f);

    // LED Dot indicator
    float ledRadius = 3.5f;
    float ledX = bounds.getX() + 14.0f;
    float ledY = bounds.getCentreY();

    juce::Colour ledColor = isOn ? juce::Colour (0xff00e676) : juce::Colour (0xff475467);
    if (isOn)
    {
        // LED bloom glow
        g.setColour (ledColor.withAlpha (0.35f));
        g.fillEllipse (ledX - ledRadius - 3.0f, ledY - ledRadius - 3.0f, (ledRadius + 3.0f) * 2.0f, (ledRadius + 3.0f) * 2.0f);
    }
    g.setColour (ledColor);
    g.fillEllipse (ledX - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

    // Text Label
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.setColour (isOn ? juce::Colours::white : juce::Colour (0xff667085));
    g.drawText (button.getButtonText(), bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft, true);
}

void ModernSpatialLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPosProportional, float rotaryStartAngle,
                                                 float rotaryEndAngle, juce::Slider& slider)
{
    bool isMaster = (width > 110);

    auto radius = (float) juce::jmin (width / 2, height / 2) - (isMaster ? 8.0f : 4.0f);
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // --- 1. THE GRID & LIGHT: Recessed CNC Dial Well (Outer Depression) ---
    if (isMaster)
    {
        juce::ColourGradient wellGrad (juce::Colour (0xff080a0f), centreX, centreY,
                                       juce::Colour (0xff181e28), centreX, centreY + radius + 10.0f, true);
        g.setGradientFill (wellGrad);
        g.fillEllipse (rx - 6.0f, ry - 6.0f, rw + 12.0f, rw + 12.0f);

        g.setColour (juce::Colour (0xff242d3d).withAlpha (0.6f));
        g.drawEllipse (rx - 6.0f, ry - 6.0f, rw + 12.0f, rw + 12.0f, 1.0f);

        // Precision Radial Calibration Ticks (Voger Design Principle 3 & 5)
        constexpr int numTicks = 25;
        for (int i = 0; i < numTicks; ++i)
        {
            float t = static_cast<float> (i) / (numTicks - 1);
            float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            bool isActive = (t <= sliderPosProportional);

            float rInner = radius + 2.0f;
            float rOuter = radius + ((i % 6 == 0) ? 6.5f : 4.0f);

            float cosA = std::cos (tickAngle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (tickAngle - juce::MathConstants<float>::halfPi);

            juce::Colour tickColor = isActive ? juce::Colour (0xff00e5ff) : juce::Colour (0xff242d3d);
            g.setColour (tickColor);
            g.drawLine (centreX + rInner * cosA, centreY + rInner * sinA,
                        centreX + rOuter * cosA, centreY + rOuter * sinA, (i % 6 == 0) ? 1.5f : 1.0f);
        }
    }

    // --- 2. TRACK: Recessed Groove & Glowing LED Value Arc ---
    float trackThickness = isMaster ? 9.0f : 4.5f;

    // Background Track Groove
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff10131a));
    g.strokePath (backgroundArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff1e2636).withAlpha (0.7f));
    g.strokePath (backgroundArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active Glowing Value Arc
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                0.0f, rotaryStartAngle, angle, true);

        // Soft LED Bloom Glow pass behind the arc
        if (isMaster)
        {
            g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.22f));
            g.strokePath (valueArc, juce::PathStrokeType (trackThickness + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::ColourGradient grad (juce::Colour (0xff00f0ff), centreX - radius, centreY,
                                       juce::Colour (0xffd500f9), centreX + radius, centreY, false);
            g.setGradientFill (grad);
        }
        else
        {
            g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.20f));
            g.strokePath (valueArc, juce::PathStrokeType (trackThickness + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (slider.isEnabled() ? juce::Colour (0xff00e5ff) : juce::Colours::grey);
        }
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- 3. THE MATERIALS: Anodized Gunmetal Aluminum Body with Directional Light ---
    float dialInset = isMaster ? 15.0f : 7.0f;
    float dialRadius = radius - dialInset;
    float dialX = centreX - dialRadius;
    float dialY = centreY - dialRadius;
    float dialD = dialRadius * 2.0f;

    // Directional Ambient Light (Top-down 90° light angle highlight)
    juce::ColourGradient dialBodyGrad (juce::Colour (0xff2a3344), centreX - dialRadius * 0.2f, centreY - dialRadius * 0.4f,
                                       juce::Colour (0xff0c0f16), centreX, centreY + dialRadius, false);
    g.setGradientFill (dialBodyGrad);
    g.fillEllipse (dialX, dialY, dialD, dialD);

    // CNC Chamfer Bevel Rim (Metallic top-edge reflection, dark underside drop shadow)
    juce::ColourGradient bevelGrad (juce::Colour (0xff546687), centreX, dialY,
                                    juce::Colour (0xff080a0f), centreX, dialY + dialD, false);
    g.setGradientFill (bevelGrad);
    g.drawEllipse (dialX, dialY, dialD, dialD, isMaster ? 1.8f : 1.2f);

    // Inner Cavity for Master Knob
    if (isMaster)
    {
        float innerCavityRadius = dialRadius * 0.65f;
        juce::ColourGradient cavityGrad (juce::Colour (0xff080a0f), centreX, centreY,
                                         juce::Colour (0xff141922), centreX, centreY + innerCavityRadius, true);
        g.setGradientFill (cavityGrad);
        g.fillEllipse (centreX - innerCavityRadius, centreY - innerCavityRadius, innerCavityRadius * 2.0f, innerCavityRadius * 2.0f);

        g.setColour (juce::Colour (0xff1e2636));
        g.drawEllipse (centreX - innerCavityRadius, centreY - innerCavityRadius, innerCavityRadius * 2.0f, innerCavityRadius * 2.0f, 1.0f);
    }

    // --- 4. TACTILE INDICATOR: Glowing LED Pointer Needle ---
    juce::Path pointer;
    float pointerLength = isMaster ? 18.0f : 8.5f;
    float pointerThickness = isMaster ? 3.5f : 2.2f;
    float pointerOffset = dialRadius - 2.0f;

    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerOffset, pointerThickness, pointerLength, 1.2f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

    // Pointer Bloom Glow
    if (slider.isEnabled())
    {
        g.setColour (juce::Colour (0xff00f0ff).withAlpha (0.45f));
        g.strokePath (pointer, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (slider.isEnabled() ? juce::Colours::white : juce::Colours::grey);
    g.fillPath (pointer);
}

//==============================================================================
// Correlation Meter (Voger Design: Calibration Ticks & Phase Awareness)
//==============================================================================

void CorrelationMeterComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float h = b.getHeight();
    float w = b.getWidth();

    // Smoked-glass background track
    g.setColour (juce::Colour (0xff080b12));
    g.fillRoundedRectangle (b, 4.0f);

    // Outer Bevel (Directional light)
    juce::ColourGradient bevelGrad (juce::Colour (0xff1f2838), 0.0f, 0.0f,
                                    juce::Colour (0xff080b10), 0.0f, h, false);
    g.setGradientFill (bevelGrad);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    // Center divider (0.0 correlation)
    float zeroX = w * 0.5f;

    // Phase Zone Backdrops
    // Danger Red (-1 to 0)
    g.setColour (juce::Colour (0xffe53935).withAlpha (0.07f));
    g.fillRect (1.0f, 1.0f, zeroX - 1.0f, h - 2.0f);

    // Safe Cyan/Green (0 to +1)
    g.setColour (juce::Colour (0xff00e676).withAlpha (0.07f));
    g.fillRect (zeroX, 1.0f, zeroX - 1.0f, h - 2.0f);

    // Calibration Tick Marks (-1.0, -0.5, 0.0, +0.5, +1.0)
    g.setColour (juce::Colour (0xff253043));
    g.drawVerticalLine (static_cast<int> (w * 0.25f), 2.0f, h - 2.0f);
    g.drawVerticalLine (static_cast<int> (w * 0.75f), 2.0f, h - 2.0f);

    // Center marker tick (high-contrast 0 line)
    g.setColour (juce::Colour (0xff41516e));
    g.drawVerticalLine (static_cast<int> (zeroX), 0.0f, h);

    // Needle Position
    float markerX = ((currentValue + 1.0f) * 0.5f) * w;
    markerX = juce::jlimit (4.0f, w - 4.0f, markerX);

    // Dynamic Color Determination
    juce::Colour markerColor;
    if (currentValue >= 0.60f)
        markerColor = juce::Colour (0xff00e676); // Safe Billboard Green
    else if (currentValue >= 0.25f)
        markerColor = juce::Colour (0xff00e5ff); // Wide 3D Cyan
    else if (currentValue >= 0.0f)
        markerColor = juce::Colour (0xffffb300); // Amber Caution
    else
        markerColor = juce::Colour (0xffff1744); // Phase Danger Red

    // Active Bar Fill from Center 0.0 to Marker
    if (markerX >= zeroX)
    {
        juce::ColourGradient fillGrad (markerColor.withAlpha (0.10f), zeroX, 0.0f,
                                       markerColor.withAlpha (0.45f), markerX, 0.0f, false);
        g.setGradientFill (fillGrad);
        g.fillRect (zeroX, 2.0f, markerX - zeroX, h - 4.0f);
    }
    else
    {
        juce::ColourGradient fillGrad (markerColor.withAlpha (0.45f), markerX, 0.0f,
                                       markerColor.withAlpha (0.10f), zeroX, 0.0f, false);
        g.setGradientFill (fillGrad);
        g.fillRect (markerX, 2.0f, zeroX - markerX, h - 4.0f);
    }

    // Glowing Needle with Top-Down Bloom
    g.setColour (markerColor.withAlpha (0.35f));
    g.fillRoundedRectangle (markerX - 4.0f, 0.0f, 8.0f, h, 2.0f);

    g.setColour (markerColor);
    g.fillRoundedRectangle (markerX - 1.8f, 1.0f, 3.6f, h - 2.0f, 1.8f);
}

//==============================================================================
// SpectrumVisualizerComponent (Holographic Smoked-Glass Analyzer)
//==============================================================================

void SpectrumVisualizerComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth();
    float h = bounds.getHeight();

    // 1. Smoked Acrylic Faceplate
    g.setColour (juce::Colour (0xff070a10));
    g.fillRoundedRectangle (bounds, 8.0f);

    // Acrylic Reflection Sheen (Voger Principle 6: Materials)
    juce::ColourGradient glassGlare (juce::Colours::white.withAlpha (0.04f), 0.0f, 0.0f,
                                     juce::Colours::transparentWhite, w * 0.45f, h * 0.85f, false);
    g.setGradientFill (glassGlare);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Bevel frame with ambient top light
    juce::ColourGradient frameGrad (juce::Colour (0xff253044), 0.0f, 0.0f,
                                    juce::Colour (0xff0a0e16), 0.0f, h, false);
    g.setGradientFill (frameGrad);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);

    // 2. Frequency Grid Guidelines & Labels (100Hz, 1kHz, 5kHz, 10kHz)
    g.setFont (juce::FontOptions (9.0f, juce::Font::plain));
    g.setColour (juce::Colour (0xff202b3d));

    const struct { float prop; const char* label; } freqs[] = {
        { 0.18f, "100" },
        { 0.48f, "1k" },
        { 0.74f, "5k" },
        { 0.90f, "12k" }
    };

    for (const auto& f : freqs)
    {
        float lineX = w * f.prop;
        g.setColour (juce::Colour (0xff151c28));
        g.drawVerticalLine (static_cast<int> (lineX), 4.0f, h - 14.0f);

        g.setColour (juce::Colour (0xff3d4f6d));
        g.drawText (f.label, lineX - 15.0f, h - 14.0f, 30.0f, 12.0f, juce::Justification::centred, false);
    }

    // Horizontal dB lines (-12dB, -24dB)
    g.setColour (juce::Colour (0xff121824));
    g.drawHorizontalLine (static_cast<int> (h * 0.35f), 4.0f, w - 4.0f);
    g.drawHorizontalLine (static_cast<int> (h * 0.70f), 4.0f, w - 4.0f);

    // 3. Dynamic Color Tuning (Cyan Safe, Magenta Wide, Amber Caution)
    juce::Colour primaryColor, secondaryColor;
    if (currentCorrelation >= 0.65f)
    {
        primaryColor   = juce::Colour (0xff00f0ff); // Electric Cyan
        secondaryColor = juce::Colour (0xff00e676); // Billboard Green
    }
    else if (currentCorrelation >= 0.28f)
    {
        primaryColor   = juce::Colour (0xffd500f9); // Vivid Magenta
        secondaryColor = juce::Colour (0xff00e5ff); // Cyan
    }
    else
    {
        primaryColor   = juce::Colour (0xffff9100); // Amber Caution
        secondaryColor = juce::Colour (0xffff1744); // Red Attention
    }

    // 4. Build Smooth Frequency Spectrum Spline
    juce::Path spectrumPath;
    float dx = w / 63.0f;
    for (int i = 0; i < 64; ++i)
    {
        float x = i * dx;
        float y = (h - 16.0f) - (levels[i] * (h - 24.0f));
        y = juce::jlimit (4.0f, h - 16.0f, y);

        if (i == 0)
            spectrumPath.startNewSubPath (x, y);
        else
            spectrumPath.lineTo (x, y);
    }

    // Filled Gradient under spectrum curve
    juce::Path filledPath = spectrumPath;
    filledPath.lineTo (w, h - 16.0f);
    filledPath.lineTo (0.0f, h - 16.0f);
    filledPath.closeSubPath();

    juce::ColourGradient fillGrad (primaryColor.withAlpha (0.32f), 0.0f, 0.0f,
                                  secondaryColor.withAlpha (0.02f), 0.0f, h - 16.0f, false);
    g.setGradientFill (fillGrad);
    g.fillPath (filledPath);

    // Glowing Neon Stroke (Halo pass + Core stroke)
    g.setColour (primaryColor.withAlpha (0.25f));
    g.strokePath (spectrumPath, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (primaryColor.withAlpha (0.95f));
    g.strokePath (spectrumPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Editor
//==============================================================================

FxSoundAudioProcessorEditor::FxSoundAudioProcessorEditor (FxSoundAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Header Console
    titleLabel.setText ("FXSOUND", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00f0ff));
    addAndMakeVisible (titleLabel);

    badgeLabel.setText ("BILLBOARD SPATIAL V0.0.7 (DYNAMIC PHASE 3D)", juce::dontSendNotification);
    badgeLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    badgeLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc084fc));
    addAndMakeVisible (badgeLabel);

    // Power Button
    addAndMakeVisible (powerButton);
    powerAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getAPVTS(), "power", powerButton);

    // Glass Analyzer Chamber Components
    addAndMakeVisible (spectrumVisualizer);
    addAndMakeVisible (correlationMeter);

    correlationTitleLabel.setText ("PHASE CORRELATION", juce::dontSendNotification);
    correlationTitleLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    correlationTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff73849e));
    addAndMakeVisible (correlationTitleLabel);

    correlationReadoutLabel.setText ("+1.00 (MONO SAFE)", juce::dontSendNotification);
    correlationReadoutLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    correlationReadoutLabel.setJustificationType (juce::Justification::right);
    correlationReadoutLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e676));
    addAndMakeVisible (correlationReadoutLabel);

    // Master Spatial Slider
    masterSpatialSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSpatialSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterSpatialSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (masterSpatialSlider);

    masterSpatialLabel.setText ("SPATIAL", juce::dontSendNotification);
    masterSpatialLabel.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    masterSpatialLabel.setJustificationType (juce::Justification::centred);
    masterSpatialLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (masterSpatialLabel);

    masterValueLabel.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    masterValueLabel.setJustificationType (juce::Justification::centred);
    masterValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00f0ff));
    addAndMakeVisible (masterValueLabel);

    masterSpatialSlider.onValueChange = [this]() {
        float val = static_cast<float> (masterSpatialSlider.getValue());
        masterValueLabel.setText (juce::String (val * 10.0f, 0) + "%", juce::dontSendNotification);
    };

    spatialAttachment = std::make_unique<SliderAttachment> (audioProcessor.getAPVTS(), "spatial", masterSpatialSlider);
    masterSpatialSlider.onValueChange();

    // Bottom Hardware Console
    setupMiniDial (vocalPunchCtrl, "vocal_punch", "TRANSIENT PUNCH", "");
    setupMiniDial (monoFreqCtrl,   "mono_freq",   "MONO BASS",       " Hz");
    setupMiniDial (clarityAirCtrl, "clarity_air", "CLARITY AIR",     "");
    setupMiniDial (outputGainCtrl, "output_gain", "OUTPUT",          " dB");

    // Grid Dimensions: 620 x 590
    setSize (620, 590);
    startTimerHz (30);
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
    ctrl.title.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    ctrl.title.setJustificationType (juce::Justification::centred);
    ctrl.title.setColour (juce::Label::textColourId, juce::Colour (0xff7a8ba3));
    addAndMakeVisible (ctrl.title);

    ctrl.valLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    ctrl.valLabel.setJustificationType (juce::Justification::centred);
    ctrl.valLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6edf8));
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

    // Update Holographic FFT Analyzer
    std::array<float, 64> scopeLevels;
    audioProcessor.copyScopeData (scopeLevels);
    spectrumVisualizer.updateData (scopeLevels, corr);

    juce::String statusText;
    juce::Colour statusColor;
    if (corr >= 0.65f)
    {
        statusText = (corr >= 0.0f ? "+" : "") + juce::String (corr, 2) + " (OPTIMAL MONO SAFE)";
        statusColor = juce::Colour (0xff00e676);
    }
    else if (corr >= 0.28f)
    {
        statusText = (corr >= 0.0f ? "+" : "") + juce::String (corr, 2) + " (WIDE BILLBOARD 3D)";
        statusColor = juce::Colour (0xffc084fc);
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
    // --- 1. OBSIDIAN CHASSIS BACKGROUND ---
    g.fillAll (juce::Colour (0xff0b0e14));

    // Top Header Plate
    juce::ColourGradient headerGrad (juce::Colour (0xff141924), 0.0f, 0.0f,
                                     juce::Colour (0xff0e121b), 0.0f, 50.0f, false);
    g.setGradientFill (headerGrad);
    g.fillRect (0, 0, getWidth(), 48);

    g.setColour (juce::Colour (0xff1f2838));
    g.drawHorizontalLine (48, 0.0f, static_cast<float> (getWidth()));

    // --- 2. HERO SECTION ENCLOSURE (SPATIAL DIAL WELL) ---
    auto heroRect = juce::Rectangle<int> (20, 206, getWidth() - 40, 222);
    juce::ColourGradient heroGrad (juce::Colour (0xff10141d), 0.0f, static_cast<float> (heroRect.getY()),
                                   juce::Colour (0xff0a0d13), 0.0f, static_cast<float> (heroRect.getBottom()), false);
    g.setGradientFill (heroGrad);
    g.fillRoundedRectangle (heroRect.toFloat(), 10.0f);

    juce::ColourGradient heroBorder (juce::Colour (0xff253043), 0.0f, static_cast<float> (heroRect.getY()),
                                     juce::Colour (0xff0e131b), 0.0f, static_cast<float> (heroRect.getBottom()), false);
    g.setGradientFill (heroBorder);
    g.drawRoundedRectangle (heroRect.toFloat(), 10.0f, 1.2f);

    // --- 3. BOTTOM HARDWARE CONSOLE BAY (4 MICRO DIALS) ---
    auto rackRect = juce::Rectangle<int> (20, 438, getWidth() - 40, 134);
    juce::ColourGradient rackGrad (juce::Colour (0xff0f131c), 0.0f, static_cast<float> (rackRect.getY()),
                                   juce::Colour (0xff090c12), 0.0f, static_cast<float> (rackRect.getBottom()), false);
    g.setGradientFill (rackGrad);
    g.fillRoundedRectangle (rackRect.toFloat(), 10.0f);

    juce::ColourGradient rackBorder (juce::Colour (0xff242f42), 0.0f, static_cast<float> (rackRect.getY()),
                                     juce::Colour (0xff0d1118), 0.0f, static_cast<float> (rackRect.getBottom()), false);
    g.setGradientFill (rackBorder);
    g.drawRoundedRectangle (rackRect.toFloat(), 10.0f, 1.2f);

    // Voger Hardware Details: 4 Precision Chassis Screws at Rack Corners
    auto drawScrew = [&g](float cx, float cy) {
        g.setColour (juce::Colour (0xff121620));
        g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff2d3a4e));
        g.drawEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, 1.0f);
        g.setColour (juce::Colour (0xff080a0f));
        g.drawLine (cx - 2.2f, cy - 2.2f, cx + 2.2f, cy + 2.2f, 1.2f);
    };

    drawScrew (rackRect.getX() + 10.0f, rackRect.getY() + 10.0f);
    drawScrew (rackRect.getRight() - 10.0f, rackRect.getY() + 10.0f);
    drawScrew (rackRect.getX() + 10.0f, rackRect.getBottom() - 10.0f);
    drawScrew (rackRect.getRight() - 10.0f, rackRect.getBottom() - 10.0f);
}

void FxSoundAudioProcessorEditor::resized()
{
    // --- 1. HEADER ROW ---
    titleLabel.setBounds (22, 10, 130, 24);
    badgeLabel.setBounds (154, 15, 330, 18);
    powerButton.setBounds (getWidth() - 108, 12, 88, 24);

    // --- 2. GLASS ANALYZER CHAMBER (FFT Spectrum + Phase Correlation) ---
    // Total Bay: x = 20, y = 56, w = getWidth() - 40, h = 142
    spectrumVisualizer.setBounds (20, 56, getWidth() - 40, 96);

    int meterY = 158;
    correlationTitleLabel.setBounds (24, meterY, 150, 14);
    correlationReadoutLabel.setBounds (getWidth() - 250, meterY, 226, 14);
    correlationMeter.setBounds (20, meterY + 16, getWidth() - 40, 16);

    // --- 3. HERO SPATIAL CORE ---
    int knobSize = 160;
    int centerX = getWidth() / 2;
    masterSpatialSlider.setBounds (centerX - knobSize / 2, 218, knobSize, knobSize);
    masterValueLabel.setBounds (centerX - 50, 284, 100, 28);
    masterSpatialLabel.setBounds (centerX - 60, 396, 120, 20);

    // --- 4. BOTTOM 4-CHANNEL HARDWARE BAY ---
    int dockY = 448;
    int colWidth = (getWidth() - 40) / 4;

    std::vector<MiniControl*> dials = { &vocalPunchCtrl, &monoFreqCtrl, &clarityAirCtrl, &outputGainCtrl };
    for (size_t i = 0; i < dials.size(); ++i)
    {
        int x = 20 + static_cast<int> (i) * colWidth;
        dials[i]->title.setBounds (x, dockY, colWidth, 14);
        dials[i]->slider.setBounds (x + (colWidth - 62) / 2, dockY + 18, 62, 62);
        dials[i]->valLabel.setBounds (x, dockY + 86, colWidth, 16);
    }
}
