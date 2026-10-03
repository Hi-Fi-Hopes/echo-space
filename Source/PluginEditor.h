#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Boutique-pedal look: coloured enclosures on a dark board, cream silkscreen,
// knurled knobs, slot faders, LEDs and a chrome footswitch.
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 10; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
// Label on top, control in the middle, value readout underneath.
class Control : public juce::Component
{
public:
    enum class Style { knob, fader };

    Control (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name, Style = Style::knob);
    void setName (const juce::String& n) { label.setText (n.toUpperCase(), juce::dontSendNotification); }
    void resized() override;

    juce::Slider slider;
    juce::Label  label;

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// Round chrome footswitch, toggles a bool parameter.
class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("ON") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

//==============================================================================
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
// Engine menu grouped into DELAYS / REVERBS. Item IDs are parameter index + 1.
class EngineSelector : public juce::ComboBox
{
public:
    explicit EngineSelector (juce::RangedAudioParameter&);

private:
    juce::ParameterAttachment attachment;
};

//==============================================================================
class StripPanel : public juce::Component
{
public:
    StripPanel (EchoSpaceProcessor&, const juce::String& prefix, const juce::String& letter, juce::Colour enclosure);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix, letter;
    juce::Colour enclosure;
    int lastEngine = -1, lastSync = -1, lastOn = -1;

    Footswitch onSwitch;
    juce::TextButton syncButton { "SYNC" };
    EngineSelector engineBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, syncAttach;

    // Top row: encoders. Bottom row: faders. Column i pairs knobs[i] with faders[i].
    Control time, division, feedback, tone, control1, control2, speed;
    Control mix, drive, lowCut, highCut, width, duck;

    es::EngineType engine() const;
    std::vector<Control*> knobs();
    std::vector<Control*> faders();
    void setControlText (Control&, int which);

    juce::Rectangle<int> ledArea, knobRow, faderRow;
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
    PedalLookAndFeel lnf;

    StripPanel stripA, stripB;
    Segmented routing;
    juce::TextButton freezeButton { "FREEZE" }, trailsButton { "TRAILS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach, trailsAttach;
    Control inputFader, outputFader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoSpaceEditor)
};
