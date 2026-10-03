#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class EchoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EchoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
// Label on top, rotary below, value readout underneath.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name);
    void setName (const juce::String& n) { label.setText (n.toUpperCase(), juce::dontSendNotification); }
    void resized() override;

    juce::Slider slider;
    juce::Label  label;

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// Row of buttons acting as one choice parameter (routing).
class Segmented : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& names);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class StripPanel : public juce::Component
{
public:
    StripPanel (EchoSpaceProcessor&, const juce::String& prefix, const juce::String& letter);

    void setEditPage (bool shouldShowEdit);
    void refresh();                         // follows engine / sync / on-off changes
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix, letter;
    bool editPage = false;
    int lastEngine = -1, lastSync = -1, lastOn = -1;

    juce::TextButton onButton { "ON" }, syncButton { "SYNC" };
    juce::ComboBox engineBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAttach;

    Knob time, division, feedback, mix, tone, control1, control2;
    Knob lowCut, highCut, width, modRate, duck;

    bool isReverb() const;
    std::vector<Knob*> mainKnobs();
    std::vector<Knob*> editKnobs();
    void layoutGrid (juce::Rectangle<int> area, const std::vector<Knob*>& knobs);

    juce::Rectangle<int> knobArea;
};

//==============================================================================
class EchoSpaceEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit EchoSpaceEditor (EchoSpaceProcessor&);
    ~EchoSpaceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    EchoSpaceProcessor& processor;
    EchoLookAndFeel lnf;

    StripPanel stripA, stripB;
    Segmented routing;
    juce::TextButton freezeButton { "FREEZE" }, trailsButton { "TRAILS" }, editButton { "EDIT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach, trailsAttach;
    juce::Slider outputKnob;
    juce::Label outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoSpaceEditor)
};
