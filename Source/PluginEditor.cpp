#include "PluginEditor.h"

using namespace pink_ids;

const juce::Colour TronLookAndFeel::pink { 0xffff2d95 };
const juce::Colour TronLookAndFeel::pinkDeep { 0xff6b0c3d };
const juce::Colour TronLookAndFeel::cyan { 0xff35e0ff };
const juce::Colour TronLookAndFeel::bg { 0xff07060b };
const juce::Colour TronLookAndFeel::panel { 0xff100e19 };
const juce::Colour TronLookAndFeel::text { 0xffb6b0c6 };

// ---------------------------------------------------------------------------
TronLookAndFeel::TronLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ResizableWindow::backgroundColourId, bg);
}

juce::Label* TronLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (11.0f)));
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::textColourId, text.withAlpha (0.85f));
    return label;
}

void TronLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float ringWidth = juce::jmax (2.0f, radius * 0.12f);

    juce::Path track;
    track.addCentredArc (centreX, centreY, radius - ringWidth * 0.5f, radius - ringWidth * 0.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2a2438));
    g.strokePath (track, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centreX, centreY, radius - ringWidth * 0.5f, radius - ringWidth * 0.5f, 0.0f,
                             rotaryStartAngle, angle, true);

        for (int i = 3; i >= 1; --i)
        {
            g.setColour (pink.withAlpha (0.07f * (float) i));
            g.strokePath (value, juce::PathStrokeType (ringWidth * (1.0f + 0.45f * (float) i),
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        g.setColour (pink);
        g.strokePath (value, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const float bodyRadius = radius - ringWidth * 1.9f;
    g.setColour (juce::Colour (0xff141020));
    g.fillEllipse (centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (pinkDeep.withAlpha (0.8f));
    g.drawEllipse (centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (centreX, centreY);
    pointer.lineTo (centreX + std::sin (angle) * bodyRadius * 0.85f,
                    centreY - std::cos (angle) * bodyRadius * 0.85f);
    g.setColour (cyan.withAlpha (0.9f));
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void TronLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? pink.withAlpha (0.22f) : juce::Colour (0xff191424));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? pink : juce::Colour (0xff332b44));
    g.drawRoundedRectangle (bounds, 4.0f, on ? 1.4f : 1.0f);

    g.setColour (on ? juce::Colours::white : text.withAlpha (shouldDrawButtonAsHighlighted ? 1.0f : 0.7f));
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);

    if (shouldDrawButtonAsDown)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }
}

// ---------------------------------------------------------------------------
TractView::TractView (PinkTronAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

void TractView::timerCallback()
{
    snapshot = processor.readSnapshot();
    repaint();
}

float TractView::xForIndex (float index, int length) const
{
    const float left = 26.0f;
    const float right = (float) getWidth() - 26.0f;
    const float t = (length > 1) ? juce::jlimit (0.0f, 1.0f, index / (float) (length - 1)) : 0.0f;
    return left + t * (right - left);
}

float TractView::indexForX (float x, int length) const
{
    const float left = 26.0f;
    const float right = (float) getWidth() - 26.0f;
    const float t = juce::jlimit (0.0f, 1.0f, (x - left) / juce::jmax (1.0f, right - left));
    return t * (float) juce::jmax (1, length - 1);
}

float TractView::yForDiameter (float diameter) const
{
    const float centre = (float) getHeight() * 0.5f;
    const float scale = (float) getHeight() * 0.30f / 3.2f;
    return centre - juce::jlimit (0.0f, 4.0f, diameter) * scale;
}

float TractView::diameterForY (float y) const
{
    const float centre = (float) getHeight() * 0.5f;
    const float scale = (float) getHeight() * 0.30f / 3.2f;
    return juce::jlimit (0.0f, 3.0f, (centre - y) / scale);
}

void TractView::mouseDown (const juce::MouseEvent& e)
{
    const int length = snapshot.length;
    if (length < 2)
        return;

    const float index = indexForX ((float) e.x, length);
    const float diameter = diameterForY ((float) e.y);

    activeSlot = processor.firstFreeConstrictionSlot();
    processor.setConstriction (activeSlot, index, diameter);
}

void TractView::mouseDrag (const juce::MouseEvent& e)
{
    const int length = snapshot.length;
    if (length < 2)
        return;

    if (activeSlot < 0)
        activeSlot = processor.firstFreeConstrictionSlot();

    processor.setConstriction (activeSlot, indexForX ((float) e.x, length), diameterForY ((float) e.y));
}

void TractView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (TronLookAndFeel::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colour (0xff221a30));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const int length = snapshot.length;
    const float centreY = (float) getHeight() * 0.5f;

    if (length < 2)
    {
        g.setColour (TronLookAndFeel::text.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("waiting for audio...", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // centre line
    g.setColour (juce::Colour (0xff1d1830));
    g.drawHorizontalLine ((int) centreY, 20.0f, (float) getWidth() - 20.0f);

    // tube
    juce::Path upper, lower, body;
    upper.startNewSubPath (xForIndex (0.0f, length), yForDiameter (snapshot.diameter[0]));
    lower.startNewSubPath (xForIndex (0.0f, length), 2.0f * centreY - yForDiameter (snapshot.diameter[0]));

    for (int i = 1; i < length; ++i)
    {
        const float x = xForIndex ((float) i, length);
        const float d = snapshot.diameter[(size_t) i];
        upper.lineTo (x, yForDiameter (d));
        lower.lineTo (x, 2.0f * centreY - yForDiameter (d));
    }

    body = upper;
    for (int i = length - 1; i >= 0; --i)
        body.lineTo (xForIndex ((float) i, length),
                     2.0f * centreY - yForDiameter (snapshot.diameter[(size_t) i]));
    body.closeSubPath();

    g.setGradientFill (juce::ColourGradient (TronLookAndFeel::pink.withAlpha (0.16f), 0.0f, centreY - 60.0f,
                                             TronLookAndFeel::pinkDeep.withAlpha (0.05f), 0.0f, centreY + 90.0f,
                                             false));
    g.fillPath (body);

    for (int pass = 3; pass >= 1; --pass)
    {
        g.setColour (TronLookAndFeel::pink.withAlpha (0.06f * (float) pass));
        g.strokePath (upper, juce::PathStrokeType (1.0f + 1.6f * (float) pass));
        g.strokePath (lower, juce::PathStrokeType (1.0f + 1.6f * (float) pass));
    }

    g.setColour (TronLookAndFeel::pink);
    g.strokePath (upper, juce::PathStrokeType (1.6f));
    g.strokePath (lower, juce::PathStrokeType (1.6f));

    // nose branch
    if (snapshot.noseStart > 0 && snapshot.velum > 0.05f)
    {
        const float x = xForIndex ((float) snapshot.noseStart, length);
        const float h = juce::jlimit (10.0f, 80.0f, snapshot.velum * 90.0f);

        juce::Path nosePath;
        nosePath.startNewSubPath (x, yForDiameter (snapshot.diameter[(size_t) snapshot.noseStart]));
        nosePath.quadraticTo (x + 40.0f, yForDiameter (snapshot.diameter[(size_t) snapshot.noseStart]) - h,
                              x + 120.0f, yForDiameter (snapshot.diameter[(size_t) snapshot.noseStart]) - h * 0.6f);
        g.setColour (TronLookAndFeel::cyan.withAlpha (0.55f));
        g.strokePath (nosePath, juce::PathStrokeType (2.0f));
    }

    // tongue
    {
        const float tx = xForIndex (snapshot.tongueIndex, length);
        const float r = juce::jmax (7.0f, snapshot.tongueDiameter * (float) getHeight() * 0.055f);
        g.setColour (TronLookAndFeel::pink.withAlpha (0.13f));
        g.fillEllipse (tx - r * 1.4f, centreY - r, r * 2.8f, r * 2.0f);
        g.setColour (TronLookAndFeel::pink.withAlpha (0.35f));
        g.drawEllipse (tx - r * 1.4f, centreY - r, r * 2.8f, r * 2.0f, 1.2f);
    }

    // constriction handles
    for (int slot = 0; slot < PinkTronAudioProcessor::kConstrictionSlots; ++slot)
    {
        if (! processor.isConstrictionEnabled (slot))
            continue;

        const float index = processor.getParamValue (constrictionIndex (slot));
        const float diameter = processor.getParamValue (constrictionDiameter (slot));
        const float x = xForIndex (index, length);
        const float y = yForDiameter (diameter);

        g.setColour (TronLookAndFeel::cyan.withAlpha (0.25f));
        g.drawLine (x, y, x, centreY, 1.0f);
        g.setColour (TronLookAndFeel::cyan);
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (x - 1.6f, y - 1.6f, 3.2f, 3.2f);
    }

    // labels
    g.setColour (TronLookAndFeel::text.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("GLOTTIS / IN", 24, (int) centreY + 10, 120, 14, juce::Justification::left);
    g.drawText ("LIPS / OUT", getWidth() - 140, (int) centreY + 10, 116, 14, juce::Justification::right);
    g.drawText ("drag inside to add a constriction", 24, 8, 300, 14, juce::Justification::left);
}

// ---------------------------------------------------------------------------
PinkTronAudioProcessorEditor::PinkTronAudioProcessorEditor (PinkTronAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tractView (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (tractView);

    // Row 1 - source / breath
    addKnob (frequency, "FREQUENCY", { 20, 276, 120, 92 });
    addKnob (tenseness, "TENSENESS", { 140, 276, 120, 92 });
    addKnob (intensity, "INTENSITY", { 260, 276, 120, 92 });
    addKnob (loudness, "LOUDNESS", { 380, 276, 120, 92 });
    addKnob (aspiration, "ASPIRATION", { 500, 276, 120, 92 });
    addKnob (noise, "NOISE", { 620, 276, 120, 92 });

    // Row 2 - tract
    addKnob (tongueIndex, "TONGUE INDEX", { 20, 392, 120, 92 });
    addKnob (tongueDiameter, "TONGUE SIZE", { 140, 392, 120, 92 });
    addKnob (tractLength, "TRACT LENGTH", { 260, 392, 120, 92 });
    addKnob (nasal, "NASAL", { 380, 392, 120, 92 });
    addKnob (mix, "MIX", { 500, 392, 120, 92 });
    addKnob (output, "OUTPUT", { 620, 392, 120, 92 });

    // Row 3 - vibrato + constrictions
    addKnob (vibratoFrequency, "VIB FREQ", { 20, 508, 100, 92 }, false, true);
    addKnob (vibratoGain, "VIB GAIN", { 116, 508, 100, 92 }, false, true);
    addKnob (vibratoWobble, "WOBBLE", { 212, 508, 100, 92 }, false, true);

    for (int slot = 0; slot < PinkTronAudioProcessor::kConstrictionSlots; ++slot)
    {
        const int x = 322 + slot * 106;

        addToggle (constrictionEnable (slot), "CON " + juce::String (slot + 1), { x, 512, 100, 20 });
        addKnob (constrictionIndex (slot), "IDX", { x, 540, 50, 62 }, false, true);
        addKnob (constrictionDiameter (slot), "DIA", { x + 50, 540, 50, 62 }, false, true);
    }

    setSize (760, 656);
    setResizable (false, false);
}

PinkTronAudioProcessorEditor::~PinkTronAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PinkTronAudioProcessorEditor::addKnob (const juce::String& parameterId, const juce::String& name,
                                            juce::Rectangle<int> cell, bool centredDetent, bool small)
{
    juce::ignoreUnused (centredDetent);

    auto knob = std::make_unique<Knob>();

    knob->label = std::make_unique<juce::Label> (name, name);
    knob->label->setJustificationType (juce::Justification::centred);
    knob->label->setFont (juce::Font (juce::FontOptions (small ? 8.5f : 9.5f, juce::Font::bold)));
    knob->label->setColour (juce::Label::textColourId, TronLookAndFeel::text.withAlpha (0.6f));
    knob->label->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*knob->label);

    knob->slider = std::make_unique<juce::Slider>();
    knob->slider->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, small ? 48 : 84, 13);
    knob->slider->setColour (juce::Slider::textBoxTextColourId, TronLookAndFeel::text.withAlpha (0.85f));
    knob->slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.15f,
                                       juce::MathConstants<float>::pi * 2.85f, true);
    knob->slider->setMouseDragSensitivity (140);
    addAndMakeVisible (*knob->slider);

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, parameterId, *knob->slider);

    knob->label->setBounds (cell.getX(), cell.getY(), cell.getWidth(), 13);
    knob->slider->setBounds (cell.getX() + (cell.getWidth() - (small ? 48 : 84)) / 2, cell.getY() + 14,
                             small ? 48 : 84, cell.getHeight() - 16);

    knobs.push_back (std::move (knob));
}

void PinkTronAudioProcessorEditor::addToggle (const juce::String& parameterId, const juce::String& name,
                                              juce::Rectangle<int> bounds)
{
    auto button = std::make_unique<juce::ToggleButton> (name);
    button->setBounds (bounds);
    addAndMakeVisible (*button);

    toggleAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, parameterId, *button));

    toggles.push_back (std::move (button));
}

void PinkTronAudioProcessorEditor::paintGroupTitle (juce::Graphics& g, const juce::String& title, int y)
{
    g.setColour (TronLookAndFeel::text.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (title, 22, y, 300, 12, juce::Justification::left);

    g.setColour (juce::Colour (0xff1d1830));
    g.drawHorizontalLine (y + 13, 22.0f, (float) getWidth() - 22.0f);
}

void PinkTronAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TronLookAndFeel::bg);

    // header
    g.setColour (TronLookAndFeel::pink);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("PINK TRON", 22, 12, 400, 32, juce::Justification::left);

    g.setColour (TronLookAndFeel::text.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("vocal tract resonator  -  your audio is the glottal source",
                24, 38, 500, 13, juce::Justification::left);

    // accent line under the title
    g.setColour (TronLookAndFeel::pink.withAlpha (0.55f));
    g.fillRect (322.0f, 46.0f, 120.0f, 2.0f);

    paintGroupTitle (g, "SOURCE  /  BREATH", 260);
    paintGroupTitle (g, "TRACT", 376);
    paintGroupTitle (g, "MODULATION  /  CONSTRICTIONS", 492);

    g.setColour (TronLookAndFeel::text.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("constrictions: click CON to enable / disable, drag the knob or drag inside the tract",
                22, 620, 700, 14, juce::Justification::left);
}

void PinkTronAudioProcessorEditor::resized()
{
    tractView.setBounds (20, 58, getWidth() - 40, 190);
}
