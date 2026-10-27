#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Neon "tron" look
// ---------------------------------------------------------------------------
class TronLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static const juce::Colour pink;
    static const juce::Colour pinkDeep;
    static const juce::Colour cyan;
    static const juce::Colour bg;
    static const juce::Colour panel;
    static const juce::Colour text;

    TronLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// ---------------------------------------------------------------------------
// Live vocal tract display (also the drag surface for constrictions)
// ---------------------------------------------------------------------------
class TractView : public juce::Component,
                  private juce::Timer
{
public:
    explicit TractView (PinkTronAudioProcessor&);
    ~TractView() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float xForIndex (float index, int length) const;
    float indexForX (float x, int length) const;
    float yForDiameter (float diameter) const;
    float diameterForY (float y) const;

    PinkTronAudioProcessor& processor;
    PinkTronAudioProcessor::TractSnapshot snapshot;
    int activeSlot = -1;
};

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
class PinkTronAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PinkTronAudioProcessorEditor (PinkTronAudioProcessor&);
    ~PinkTronAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (const juce::String& parameterId, const juce::String& name, juce::Rectangle<int> cell,
                  bool centredDetent = false, bool small = false);
    void addToggle (const juce::String& parameterId, const juce::String& name, juce::Rectangle<int> bounds);
    void paintGroupTitle (juce::Graphics&, const juce::String&, int y);

    PinkTronAudioProcessor& processor;
    TronLookAndFeel lookAndFeel;
    TractView tractView;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinkTronAudioProcessorEditor)
};
