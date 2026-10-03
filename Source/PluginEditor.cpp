#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour bg     { 0xffe8e6e1 };   // warm light grey
    const juce::Colour panel  { 0xfff3f2ee };
    const juce::Colour knob   { 0xfffbfaf8 };
    const juce::Colour ink    { 0xff161616 };
    const juce::Colour dim    { 0xff86847e };
    const juce::Colour line   { 0xffcac7bf };
    const juce::Colour accent { 0xffff5a1f };   // signal orange
}

namespace
{
    juce::Font sans (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (0.06f));
    }

    juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    // Reverb decay knob position (0..100) <-> RT60 seconds, matching the DSP.
    float decaySeconds (double pct)        { return 0.2f * std::pow (100.0f, (float) pct * 0.01f); }
    double decayPercent (float seconds)    { return 100.0 * std::log (juce::jmax (0.2f, seconds) / 0.2f) / std::log (100.0); }
}

//==============================================================================
EchoLookAndFeel::EchoLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Palette::ink);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  Palette::accent.withAlpha (0.35f));
    setColour (juce::Label::textColourId,               Palette::dim);
    setColour (juce::TextEditor::textColourId,          Palette::ink);
    setColour (juce::TextEditor::highlightColourId,     Palette::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId,     Palette::accent);
    setColour (juce::ComboBox::textColourId,            Palette::ink);
    setColour (juce::ComboBox::arrowColourId,           Palette::ink);
    setColour (juce::PopupMenu::backgroundColourId,     Palette::panel);
    setColour (juce::PopupMenu::textColourId,           Palette::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink);
    setColour (juce::TextButton::textColourOffId,       Palette::ink);
    setColour (juce::TextButton::textColourOnId,        Palette::ink);
}

void EchoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto area   = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r     = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto  c     = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    // Outer track + value arc
    const float ringR = r - 2.0f;
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
    g.setColour (Palette::line.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    g.setColour (Palette::accent.withMultipliedAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Body
    const float bodyR = r * 0.72f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (Palette::knob.withMultipliedAlpha (alpha));
    g.fillEllipse (body);
    g.setColour (Palette::ink.withMultipliedAlpha (alpha));
    g.drawEllipse (body, 1.3f);

    // Indicator
    const juce::Point<float> inner (c.x + bodyR * 0.25f * std::sin (angle), c.y - bodyR * 0.25f * std::cos (angle));
    const juce::Point<float> outer (c.x + bodyR * 0.88f * std::sin (angle), c.y - bodyR * 0.88f * std::cos (angle));
    g.drawLine ({ inner, outer }, 2.2f);
}

void EchoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.35f;

    if (on)
    {
        g.setColour (Palette::accent.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour ((over || down ? Palette::knob : Palette::panel).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
    }
    g.setColour (Palette::ink.withMultipliedAlpha (on ? alpha : alpha * 0.55f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void EchoLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (sans (11.0f, true));
    g.setColour (Palette::ink.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void EchoLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (Palette::knob);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Palette::ink.withMultipliedAlpha (box.isEnabled() ? 0.55f : 0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // Down arrow
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::ink);
    g.fillPath (arrow);
}

juce::Font EchoLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (13.0f, true); }
juce::Font EchoLookAndFeel::getPopupMenuFont()                 { return sans (13.0f); }

juce::Font EchoLookAndFeel::getLabelFont (juce::Label& l)
{
    // Slider value readouts use a monospaced face; everything else the default.
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return mono (12.0f);
    return LookAndFeel_V4::getLabelFont (l);
}

void EchoLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

namespace
{
    // Value readout styling set on the slider itself, so it wins over JUCE's defaults.
    void styleReadout (juce::Slider& s)
    {
        s.setColour (juce::Slider::textBoxTextColourId,       Palette::ink);
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxHighlightColourId,  Palette::accent.withAlpha (0.35f));
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name)
    : attachment (apvts, paramID, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    styleReadout (slider);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    if (auto* p = apvts.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // double-click = default
    addAndMakeVisible (slider);

    label.setJustificationType (juce::Justification::centred);
    label.setFont (sans (11.0f, true));
    label.setColour (juce::Label::textColourId, Palette::ink);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    setName (name);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
Segmented::Segmented (juce::RangedAudioParameter& param, const juce::StringArray& names)
    : attachment (param, [this] (float v)
                  {
                      for (int i = 0; i < buttons.size(); ++i)
                          buttons[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
                  })
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i].toUpperCase()));
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < names.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i == buttons.size() - 1 ? r : r.removeFromLeft (w));
}

//==============================================================================
StripPanel::StripPanel (EchoSpaceProcessor& p, const juce::String& pre, const juce::String& l)
    : apvts (p.apvts), prefix (pre), letter (l),
      time     (apvts, pre + ParamIDs::time,     "Time"),
      division (apvts, pre + ParamIDs::division, "Time"),
      feedback (apvts, pre + ParamIDs::feedback, "Repeats"),
      mix      (apvts, pre + ParamIDs::mix,      "Mix"),
      tone     (apvts, pre + ParamIDs::tone,     "Tone"),
      control1 (apvts, pre + ParamIDs::control1, "Control 1"),
      control2 (apvts, pre + ParamIDs::control2, "Control 2"),
      lowCut   (apvts, pre + ParamIDs::lowCut,   "Low Cut"),
      highCut  (apvts, pre + ParamIDs::highCut,  "High Cut"),
      width    (apvts, pre + ParamIDs::width,    "Width"),
      modRate  (apvts, pre + ParamIDs::modRate,  "Mod Rate"),
      duck     (apvts, pre + ParamIDs::duck,     "Duck")
{
    onButton.setClickingTogglesState (true);
    syncButton.setClickingTogglesState (true);
    engineBox.addItemList (EchoSpaceProcessor::engineNames(), 1);

    for (auto* c : std::initializer_list<juce::Component*> { &onButton, &syncButton, &engineBox })
        addAndMakeVisible (c);

    onAttach     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>   (apvts, pre + ParamIDs::on, onButton);
    syncAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>   (apvts, pre + ParamIDs::sync, syncButton);
    engineAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, pre + ParamIDs::engine, engineBox);

    for (auto* k : mainKnobs()) addChildComponent (k);
    addChildComponent (division);
    for (auto* k : editKnobs()) addChildComponent (k);

    // Time knob: shows pre-delay (time / 8) when a reverb engine is loaded.
    time.slider.textFromValueFunction = [this] (double v)
    {
        if (isReverb()) return juce::String (juce::roundToInt (v / es::preDelayDivisor)) + " ms";
        return juce::String (juce::roundToInt (v)) + " ms";
    };
    time.slider.valueFromTextFunction = [this] (const juce::String& t)
    {
        const double v = t.getDoubleValue();
        return isReverb() ? v * es::preDelayDivisor : v;
    };

    // Feedback knob: shows decay time in seconds for reverbs.
    feedback.slider.textFromValueFunction = [this] (double v)
    {
        if (isReverb())
        {
            const float s = decaySeconds (v);
            return juce::String (s, s < 10.0f ? 2 : 1) + " s";
        }
        return juce::String (juce::roundToInt (v)) + "%";
    };
    feedback.slider.valueFromTextFunction = [this] (const juce::String& t)
    {
        const double v = t.getDoubleValue();
        return isReverb() ? decayPercent ((float) v) : v;
    };

    refresh();
    setEditPage (false);
}

bool StripPanel::isReverb() const
{
    const int e = (int) apvts.getRawParameterValue (prefix + ParamIDs::engine)->load();
    return es::isReverb ((es::EngineType) e);
}

std::vector<Knob*> StripPanel::mainKnobs() { return { &time, &feedback, &mix, &tone, &control1, &control2 }; }
std::vector<Knob*> StripPanel::editKnobs() { return { &lowCut, &highCut, &width, &modRate, &duck }; }

void StripPanel::setEditPage (bool shouldShowEdit)
{
    editPage = shouldShowEdit;
    lastEngine = lastSync = -1;   // force a refresh of visibility
    refresh();
    repaint();
}

void StripPanel::refresh()
{
    const int engine = (int) apvts.getRawParameterValue (prefix + ParamIDs::engine)->load();
    const int sync   = apvts.getRawParameterValue (prefix + ParamIDs::sync)->load() > 0.5f ? 1 : 0;
    const int on     = apvts.getRawParameterValue (prefix + ParamIDs::on)->load() > 0.5f ? 1 : 0;

    if (engine == lastEngine && sync == lastSync && on == lastOn)
        return;

    lastEngine = engine; lastSync = sync; lastOn = on;
    const auto type   = (es::EngineType) engine;
    const bool reverb = es::isReverb (type);
    const bool synced = sync == 1 && ! reverb;

    time.setName (reverb ? "Pre-delay" : "Time");
    division.setName ("Time");
    feedback.setName (reverb ? "Decay" : "Repeats");
    control1.setName (EchoSpaceProcessor::control1Name (type));
    control2.setName (EchoSpaceProcessor::control2Name (type));
    time.slider.updateText();
    feedback.slider.updateText();

    syncButton.setEnabled (! reverb);

    for (auto* k : mainKnobs()) k->setVisible (! editPage);
    for (auto* k : editKnobs()) k->setVisible (editPage);
    time.setVisible (! editPage && ! synced);
    division.setVisible (! editPage && synced);

    const float alpha = on ? 1.0f : 0.4f;
    for (auto* k : mainKnobs()) k->setAlpha (alpha);
    for (auto* k : editKnobs()) k->setAlpha (alpha);
    division.setAlpha (alpha);
    repaint();
}

void StripPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Palette::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    // Strip letter in a circle
    const auto badge = juce::Rectangle<float> (16.0f, 14.0f, 30.0f, 30.0f);
    g.setColour (Palette::ink);
    g.fillEllipse (badge);
    g.setColour (Palette::panel);
    g.setFont (sans (15.0f, true));
    g.drawText (letter, badge, juce::Justification::centred);

    // Divider under the header row
    g.setColour (Palette::line);
    g.drawHorizontalLine (58, 16.0f, (float) getWidth() - 16.0f);

    g.setColour (Palette::dim);
    g.setFont (sans (10.0f, true));
    g.drawText (editPage ? "EDIT" : "MAIN", getWidth() - 70, 64, 54, 14, juce::Justification::centredRight);
}

void StripPanel::resized()
{
    auto header = getLocalBounds().reduced (16, 14).removeFromTop (30);
    header.removeFromLeft (30 + 12);                      // badge
    onButton.setBounds (header.removeFromLeft (48));
    header.removeFromLeft (10);
    syncButton.setBounds (header.removeFromRight (64));
    header.removeFromRight (10);
    engineBox.setBounds (header);

    knobArea = getLocalBounds().withTrimmedTop (76).reduced (12, 6);
    layoutGrid (knobArea, mainKnobs());
    division.setBounds (time.getBounds());
    layoutGrid (knobArea, editKnobs());
}

void StripPanel::layoutGrid (juce::Rectangle<int> area, const std::vector<Knob*>& knobs)
{
    const int cols = 3;
    const int rows = ((int) knobs.size() + cols - 1) / cols;
    const int cw = area.getWidth() / cols, rh = area.getHeight() / juce::jmax (1, rows);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int c = (int) i % cols, rI = (int) i / cols;
        knobs[i]->setBounds (area.getX() + c * cw, area.getY() + rI * rh, cw, rh);
    }
}

//==============================================================================
EchoSpaceEditor::EchoSpaceEditor (EchoSpaceProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      stripA (p, "a_", "A"), stripB (p, "b_", "B"),
      routing (*p.apvts.getParameter (ParamIDs::routing), EchoSpaceProcessor::routingNames())
{
    setLookAndFeel (&lnf);

    for (auto* b : { &freezeButton, &trailsButton, &editButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::freeze, freezeButton);
    trailsAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::trails, trailsButton);
    editButton.onClick = [this]
    {
        stripA.setEditPage (editButton.getToggleState());
        stripB.setEditPage (editButton.getToggleState());
    };

    outputKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleReadout (outputKnob);
    outputKnob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    outputKnob.setDoubleClickReturnValue (true, 0.0);
    outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamIDs::output, outputKnob);
    outputLabel.setText ("OUT", juce::dontSendNotification);
    outputLabel.setFont (sans (11.0f, true));
    outputLabel.setColour (juce::Label::textColourId, Palette::ink);
    outputLabel.setJustificationType (juce::Justification::centredRight);

    for (auto* c : std::initializer_list<juce::Component*> { &stripA, &stripB, &routing, &outputKnob, &outputLabel })
        addAndMakeVisible (c);

    setSize (1040, 480);
    startTimerHz (20);
}

EchoSpaceEditor::~EchoSpaceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EchoSpaceEditor::timerCallback()
{
    stripA.refresh();
    stripB.refresh();
}

void EchoSpaceEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    g.setColour (Palette::ink);
    g.setFont (sans (16.0f, true));
    g.drawText ("ECHO SPACE", 20, 18, 200, 20, juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (sans (10.5f, true));
    g.drawText ("DUAL DELAY / REVERB", 20, 38, 200, 14, juce::Justification::centredLeft);

    // Routing caption
    g.drawText ("ROUTING", routing.getX(), routing.getY() - 15, 120, 12, juce::Justification::centredLeft);
}

void EchoSpaceEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto header = r.removeFromTop (48);
    r.removeFromTop (16);

    header.removeFromLeft (220);
    routing.setBounds (header.removeFromLeft (270).withSizeKeepingCentre (270, 28).translated (0, 7));

    auto right = header;
    auto outArea = right.removeFromRight (150);
    outputLabel.setBounds (outArea.removeFromLeft (34));
    outputKnob.setBounds (outArea);
    right.removeFromRight (16);
    editButton.setBounds   (right.removeFromRight (64).withSizeKeepingCentre (64, 28).translated (0, 7));
    right.removeFromRight (8);
    trailsButton.setBounds (right.removeFromRight (78).withSizeKeepingCentre (78, 28).translated (0, 7));
    right.removeFromRight (8);
    freezeButton.setBounds (right.removeFromRight (78).withSizeKeepingCentre (78, 28).translated (0, 7));

    const int gap = 16;
    const int w = (r.getWidth() - gap) / 2;
    stripA.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (gap);
    stripB.setBounds (r);
}
