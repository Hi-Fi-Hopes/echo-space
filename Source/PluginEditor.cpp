#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour board     { 0xff1c1b19 };   // dark pedalboard
    const juce::Colour boardHi   { 0xff2a2825 };
    const juce::Colour cream     { 0xfff2e9d8 };   // silkscreen
    const juce::Colour creamDim  { 0xffb8ae9c };
    const juce::Colour ink       { 0xff151413 };
    const juce::Colour plate     { 0xff121110 };   // recessed label plate / buttons
    const juce::Colour amber     { 0xffffb238 };
    const juce::Colour ledRed    { 0xffff3a2e };
    const juce::Colour ledOff    { 0xff4a1916 };

    const juce::Colour enclosureA { 0xffa3322b };  // oxblood red
    const juce::Colour enclosureB { 0xff1e6b69 };  // deep teal
}

namespace
{
    juce::Font sans (float size, bool bold = false, float kerning = 0.08f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (kerning));
    }

    juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    // Reverb decay knob position (0..100) <-> RT60 seconds, matching the DSP.
    float decaySeconds (double pct)        { return 0.2f * std::pow (100.0f, (float) pct * 0.01f); }
    double decayPercent (float seconds)    { return 100.0 * std::log (juce::jmax (0.2f, seconds) / 0.2f) / std::log (100.0); }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        juce::ColourGradient grad (juce::Colour (0xffe6e6e2), c.x - r * 0.5f, c.y - r * 0.5f,
                                   juce::Colour (0xff6f6f6a), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
        const float dx = std::cos (angle) * r * 0.7f, dy = std::sin (angle) * r * 0.7f;
        g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.4f);
    }
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::Label::textColourId,               Palette::cream);
    setColour (juce::TextEditor::textColourId,          Palette::cream);
    setColour (juce::TextEditor::backgroundColourId,    Palette::plate);
    setColour (juce::TextEditor::highlightColourId,     Palette::amber.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId,     Palette::amber);
    setColour (juce::ComboBox::textColourId,            Palette::cream);
    setColour (juce::ComboBox::arrowColourId,           Palette::cream);
    setColour (juce::PopupMenu::backgroundColourId,     Palette::plate);
    setColour (juce::PopupMenu::textColourId,           Palette::cream);
    setColour (juce::PopupMenu::headerTextColourId,     Palette::amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink);
    setColour (juce::TextButton::textColourOffId,       Palette::cream);
    setColour (juce::TextButton::textColourOnId,        Palette::ink);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto area   = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float r     = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto  c     = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    // Silkscreen scale: thin track with value arc, plus 11 dots
    const float ringR = r - 2.0f;
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
    g.setColour (Palette::cream.withAlpha (0.25f * alpha));
    g.strokePath (track, juce::PathStrokeType (2.0f));
    g.setColour (Palette::cream.withAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knurled skirt
    const float skirtR = r * 0.80f;
    g.setColour (juce::Colour (0xff2b2a28).withMultipliedAlpha (alpha));
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2, skirtR * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f * alpha));
    for (int i = 0; i < 40; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 40.0f;
        g.drawLine (c.x + std::sin (a) * skirtR * 0.86f, c.y - std::cos (a) * skirtR * 0.86f,
                    c.x + std::sin (a) * skirtR,         c.y - std::cos (a) * skirtR, 1.2f);
    }

    // Cap with a soft top-left highlight
    const float capR = r * 0.60f;
    juce::ColourGradient capGrad (juce::Colour (0xff4a4844).withMultipliedAlpha (alpha), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                  juce::Colour (0xff121110).withMultipliedAlpha (alpha), c.x + capR * 0.7f, c.y + capR, true);
    g.setGradientFill (capGrad);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    g.setColour (juce::Colours::black.withAlpha (0.8f * alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);

    // Pointer: cream line from the cap centre out across the skirt
    const juce::Point<float> p0 (c.x + std::sin (angle) * capR * 0.15f, c.y - std::cos (angle) * capR * 0.15f);
    const juce::Point<float> p1 (c.x + std::sin (angle) * skirtR * 0.97f, c.y - std::cos (angle) * skirtR * 0.97f);
    g.setColour (Palette::cream.withAlpha (alpha));
    g.drawLine ({ p0, p1 }, 2.6f);
}

void PedalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float sliderPos, float minPos, float maxPos,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }
    juce::ignoreUnused (minPos, maxPos);

    const float alpha  = s.isEnabled() ? 1.0f : 0.35f;
    const float cx     = (float) x + (float) w * 0.5f;
    const float top    = (float) y, bottom = (float) (y + h);
    const auto  stripe = s.findColour (juce::Slider::thumbColourId);

    // Silkscreen ticks
    g.setColour (Palette::cream.withAlpha (0.55f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = bottom + (top - bottom) * (float) i / 10.0f;
        const float len = (i % 5 == 0) ? 8.0f : 4.0f;
        g.drawLine (cx - 12.0f - len, ty, cx - 12.0f, ty, i % 5 == 0 ? 1.4f : 1.0f);
    }

    // Slot with a glow below the cap
    const auto slot = juce::Rectangle<float> (cx - 3.5f, top - 4.0f, 7.0f, bottom - top + 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.85f * alpha));
    g.fillRoundedRectangle (slot, 3.5f);
    g.setColour (Palette::amber.withAlpha (0.75f * alpha));
    g.fillRoundedRectangle (cx - 1.5f, sliderPos, 3.0f, bottom - sliderPos, 1.5f);

    // Cap: cream block with a coloured stripe
    const auto cap = juce::Rectangle<float> (30.0f, 18.0f).withCentre ({ cx, sliderPos });
    g.setColour (juce::Colours::black.withAlpha (0.35f * alpha));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    g.setColour (Palette::cream.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (stripe.withMultipliedAlpha (alpha));
    g.fillRect (cap.getX() + 3.0f, sliderPos - 1.5f, cap.getWidth() - 6.0f, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f * alpha));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
}

void PedalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = b.isEnabled() ? 1.0f : 0.35f;

    if (b.getToggleState())
    {
        g.setColour (Palette::amber.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour ((over ? Palette::plate.brighter (0.25f) : Palette::plate).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f * alpha));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void PedalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (sans (11.0f, true, 0.12f));
    const auto col = b.getToggleState() ? Palette::ink : Palette::cream;
    g.setColour (col.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void PedalLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    // Recessed black label plate
    auto r = juce::Rectangle<int> (w, h).toFloat();
    g.setColour (Palette::plate);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (h - 2, 6.0f, (float) w - 6.0f);

    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (Palette::amber);
    g.fillPath (arrow);
}

juce::Font PedalLookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (15.0f, true, 0.1f); }
juce::Font PedalLookAndFeel::getPopupMenuFont()                 { return sans (14.0f); }

juce::Font PedalLookAndFeel::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return mono (12.0f);
    return LookAndFeel_V4::getLabelFont (l);
}

void PedalLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
void Footswitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();

    // Hex nut ring
    juce::Path nut;
    nut.addPolygon (c, 6, d * 0.5f, juce::MathConstants<float>::pi / 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcfca), c.x - d * 0.4f, c.y - d * 0.4f,
                                             juce::Colour (0xff5b5b57), c.x + d * 0.4f, c.y + d * 0.4f, false));
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (nut, juce::PathStrokeType (1.0f));

    // Domed chrome button
    const float br = d * 0.30f * (down ? 0.94f : 1.0f);
    juce::ColourGradient dome (juce::Colours::white, c.x - br * 0.4f, c.y - br * 0.5f,
                               juce::Colour (0xff77776f), c.x + br, c.y + br, true);
    g.setGradientFill (dome);
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (over ? 0.8f : 0.6f));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);
}

//==============================================================================
Control::Control (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name, Style style)
    : attachment (apvts, paramID, slider)
{
    if (style == Style::knob)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
    }
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    slider.setColour (juce::Slider::textBoxTextColourId,       Palette::cream);
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.28f));
    slider.setColour (juce::Slider::textBoxHighlightColourId,  Palette::amber.withAlpha (0.4f));
    slider.setColour (juce::Slider::thumbColourId,             Palette::amber);

    if (auto* p = apvts.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // double-click = default
    addAndMakeVisible (slider);

    label.setJustificationType (juce::Justification::centred);
    label.setFont (sans (11.0f, true, 0.1f));
    label.setColour (juce::Label::textColourId, Palette::cream);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    setName (name);
}

void Control::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r.withTrimmedTop (2));
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
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
    {
        buttons[i]->setBounds (i == buttons.size() - 1 ? r : r.removeFromLeft (w).withTrimmedRight (4));
    }
}

//==============================================================================
EngineSelector::EngineSelector (juce::RangedAudioParameter& param)
    : attachment (param, [this] (float v) { setSelectedId (juce::roundToInt (v) + 1, juce::dontSendNotification); })
{
    using E = es::EngineType;
    const auto names = EchoSpaceProcessor::engineNames();
    auto add = [&] (E e) { addItem (names[(int) e].toUpperCase(), (int) e + 1); };

    addSectionHeading ("DELAYS");
    for (auto e : { E::digital, E::tape, E::analog, E::oilCan, E::reverse, E::decay, E::dual, E::pattern }) add (e);
    addSectionHeading ("REVERBS");
    for (auto e : { E::room, E::hall, E::plate, E::spring, E::shimmer, E::swell, E::dome }) add (e);

    onChange = [this] { if (getSelectedId() > 0) attachment.setValueAsCompleteGesture ((float) (getSelectedId() - 1)); };
    attachment.sendInitialUpdate();
}

//==============================================================================
StripPanel::StripPanel (EchoSpaceProcessor& p, const juce::String& pre, const juce::String& l, juce::Colour colour)
    : apvts (p.apvts), prefix (pre), letter (l), enclosure (colour),
      engineBox (*p.apvts.getParameter (pre + ParamIDs::engine)),
      time     (apvts, pre + ParamIDs::time,     "Time"),
      division (apvts, pre + ParamIDs::division, "Time"),
      feedback (apvts, pre + ParamIDs::feedback, "Repeats"),
      tone     (apvts, pre + ParamIDs::tone,     "Tone"),
      control1 (apvts, pre + ParamIDs::control1, "Control 1"),
      control2 (apvts, pre + ParamIDs::control2, "Control 2"),
      speed    (apvts, pre + ParamIDs::modRate,  "Speed"),
      mix      (apvts, pre + ParamIDs::mix,      "Mix",      Control::Style::fader),
      drive    (apvts, pre + ParamIDs::drive,    "Drive",    Control::Style::fader),
      lowCut   (apvts, pre + ParamIDs::lowCut,   "Low Cut",  Control::Style::fader),
      highCut  (apvts, pre + ParamIDs::highCut,  "High Cut", Control::Style::fader),
      width    (apvts, pre + ParamIDs::width,    "Width",    Control::Style::fader),
      duck     (apvts, pre + ParamIDs::duck,     "Duck",     Control::Style::fader)
{
    syncButton.setClickingTogglesState (true);

    for (auto* c : std::initializer_list<juce::Component*> { &onSwitch, &syncButton, &tapButton, &engineBox })
        addAndMakeVisible (c);
    tapButton.onClick = [this] { tap(); };
    for (auto* k : knobs())  addAndMakeVisible (k);
    for (auto* f : faders()) addAndMakeVisible (f);
    addChildComponent (division);

    // Fader caps carry the enclosure colour as their stripe
    for (auto* f : faders()) f->slider.setColour (juce::Slider::thumbColourId, enclosure.brighter (0.2f));

    onAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, pre + ParamIDs::on, onSwitch);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, pre + ParamIDs::sync, syncButton);

    // Time knob shows pre-delay (time / 8) for reverb engines.
    time.slider.textFromValueFunction = [this] (double v)
    {
        if (es::isReverb (engine())) return juce::String (juce::roundToInt (v / es::preDelayDivisor)) + " ms";
        return juce::String (juce::roundToInt (v)) + " ms";
    };
    time.slider.valueFromTextFunction = [this] (const juce::String& t)
    {
        const double v = t.getDoubleValue();
        return es::isReverb (engine()) ? v * es::preDelayDivisor : v;
    };

    // Feedback knob shows decay time in seconds for reverbs.
    feedback.slider.textFromValueFunction = [this] (double v)
    {
        if (es::isReverb (engine()))
        {
            const float s = decaySeconds (v);
            return juce::String (s, s < 10.0f ? 2 : 1) + " s";
        }
        return juce::String (juce::roundToInt (v)) + "%";
    };
    feedback.slider.valueFromTextFunction = [this] (const juce::String& t)
    {
        const double v = t.getDoubleValue();
        return es::isReverb (engine()) ? decayPercent ((float) v) : v;
    };

    setControlText (control1, 1);
    setControlText (control2, 2);
    refresh();
}

void StripPanel::tap()
{
    // Tap tempo: average the gaps between the last few taps and set the delay Time.
    // A pause of more than 2 seconds starts a fresh count.
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();
    taps.push_back (now);
    if (taps.size() > 5)
        taps.erase (taps.begin());

    // Flash the button for each tap
    tapButton.setToggleState (true, juce::dontSendNotification);
    juce::Component::SafePointer<juce::TextButton> safe (&tapButton);
    juce::Timer::callAfterDelay (90, [safe] { if (safe != nullptr) safe->setToggleState (false, juce::dontSendNotification); });

    if (taps.size() < 2)
        return;

    const double ms = (taps.back() - taps.front()) / (double) (taps.size() - 1);

    auto setParam = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (prefix + id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    setParam (ParamIDs::sync, 0.0f);                               // tapped time replaces tempo sync
    setParam (ParamIDs::time, (float) juce::jlimit (1.0, 2000.0, ms));
}

void StripPanel::setControlText (Control& c, int which)
{
    c.slider.textFromValueFunction = [this, which] (double v)
    {
        const auto t = EchoSpaceProcessor::controlValueText (engine(), which, v);
        return t.isNotEmpty() ? t : juce::String (juce::roundToInt (v)) + "%";
    };
}

es::EngineType StripPanel::engine() const
{
    const int e = (int) apvts.getRawParameterValue (prefix + ParamIDs::engine)->load();
    return (es::EngineType) juce::jlimit (0, es::numEngines - 1, e);
}

std::vector<Control*> StripPanel::knobs()  { return { &time, &feedback, &tone, &control1, &control2, &speed }; }
std::vector<Control*> StripPanel::faders() { return { &mix, &drive, &lowCut, &highCut, &width, &duck }; }

void StripPanel::refresh()
{
    const int eng  = (int) engine();
    const int sync = apvts.getRawParameterValue (prefix + ParamIDs::sync)->load() > 0.5f ? 1 : 0;
    const int on   = apvts.getRawParameterValue (prefix + ParamIDs::on)->load() > 0.5f ? 1 : 0;

    if (eng == lastEngine && sync == lastSync && on == lastOn)
        return;

    lastEngine = eng; lastSync = sync; lastOn = on;
    const auto type   = (es::EngineType) eng;
    const bool reverb = es::isReverb (type);
    const bool synced = sync == 1 && ! reverb;

    time.setName (reverb ? "Pre-delay" : "Time");
    division.setName ("Time");
    feedback.setName (reverb ? "Decay" : "Repeats");
    control1.setName (EchoSpaceProcessor::control1Name (type));
    control2.setName (EchoSpaceProcessor::control2Name (type));
    for (auto* c : { &time, &feedback, &control1, &control2 })
        c->slider.updateText();

    syncButton.setEnabled (! reverb);
    tapButton.setEnabled (! reverb);
    time.setVisible (! synced);
    division.setVisible (synced);

    const bool speedActive = es::usesSpeed (type);
    speed.slider.setEnabled (speedActive);
    speed.label.setColour (juce::Label::textColourId, Palette::cream.withAlpha (speedActive ? 1.0f : 0.4f));

    const float alpha = on ? 1.0f : 0.55f;
    for (auto* k : knobs())  k->setAlpha (alpha);
    for (auto* f : faders()) f->setAlpha (alpha);
    division.setAlpha (alpha);
    repaint();
}

void StripPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    // Enclosure: powder-coat gradient, bevel highlight, edge shadow
    juce::ColourGradient body (enclosure.brighter (0.18f), 0.0f, 0.0f, enclosure.darker (0.35f), 0.0f, r.getHeight(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (1.5f), 15.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.0f);

    // Corner screws
    const float inset = 12.0f;
    drawScrew (g, { inset, inset }, 5.0f, 0.6f);
    drawScrew (g, { r.getWidth() - inset, inset }, 5.0f, 2.1f);
    drawScrew (g, { inset, r.getHeight() - inset }, 5.0f, 1.2f);
    drawScrew (g, { r.getWidth() - inset, r.getHeight() - inset }, 5.0f, 0.2f);

    // LED with glow when the strip is on
    const bool on = lastOn == 1;
    const auto led = ledArea.toFloat();
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (Palette::ledRed.withAlpha (0.55f), led.getCentreX(), led.getCentreY(),
                                                 Palette::ledRed.withAlpha (0.0f), led.getCentreX() + 18.0f, led.getCentreY(), true));
        g.fillEllipse (led.expanded (12.0f));
    }
    g.setColour (on ? Palette::ledRed : Palette::ledOff);
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.15f));
    g.fillEllipse (led.getX() + 3.0f, led.getY() + 2.5f, 4.0f, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (led, 1.0f);

    // Channel letter
    const auto badge = juce::Rectangle<float> (led.getRight() + 12.0f, led.getCentreY() - 17.0f, 34.0f, 34.0f);
    g.setColour (Palette::cream);
    g.drawEllipse (badge.reduced (1.0f), 2.0f);
    g.setFont (sans (19.0f, true, 0.0f));
    g.drawText (letter, badge, juce::Justification::centred);

    // Silkscreen rules between header / knobs / faders
    g.setColour (Palette::cream.withAlpha (0.35f));
    g.drawHorizontalLine (knobRow.getY() - 8, 24.0f, r.getWidth() - 24.0f);
    g.drawHorizontalLine (faderRow.getY() - 6, 24.0f, r.getWidth() - 24.0f);

    // Footer silkscreen
    g.setColour (Palette::cream.withAlpha (0.85f));
    g.setFont (sans (10.5f, true, 0.35f));
    g.drawText ("ECHO SPACE  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  CHANNEL " + letter, getLocalBounds().removeFromBottom (30), juce::Justification::centred);
}

void StripPanel::resized()
{
    auto header = getLocalBounds().reduced (26, 16).removeFromTop (44);
    ledArea = header.removeFromLeft (14).withSizeKeepingCentre (14, 14);
    header.removeFromLeft (12 + 34 + 14);                 // letter badge
    onSwitch.setBounds (header.removeFromRight (44));
    header.removeFromRight (12);
    syncButton.setBounds (header.removeFromRight (62).withSizeKeepingCentre (62, 28));
    header.removeFromRight (8);
    tapButton.setBounds (header.removeFromRight (52).withSizeKeepingCentre (52, 28));
    header.removeFromRight (12);
    engineBox.setBounds (header.withSizeKeepingCentre (header.getWidth(), 34));

    auto body = getLocalBounds().reduced (14, 0).withTrimmedTop (78).withTrimmedBottom (30);
    knobRow  = body.removeFromTop (128);
    body.removeFromTop (12);
    faderRow = body;

    const auto ks = knobs();
    const auto fs = faders();
    const int colW = knobRow.getWidth() / (int) ks.size();
    for (size_t i = 0; i < ks.size(); ++i)
    {
        ks[i]->setBounds (knobRow.getX() + (int) i * colW, knobRow.getY(), colW, knobRow.getHeight());
        fs[i]->setBounds (faderRow.getX() + (int) i * colW, faderRow.getY(), colW, faderRow.getHeight());
    }
    division.setBounds (time.getBounds());
}

//==============================================================================
EchoSpaceEditor::EchoSpaceEditor (EchoSpaceProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      stripA (p, "a_", "A", Palette::enclosureA), stripB (p, "b_", "B", Palette::enclosureB),
      routing (*p.apvts.getParameter (ParamIDs::routing), EchoSpaceProcessor::routingNames()),
      inputFader  (p.apvts, ParamIDs::input,  "In",  Control::Style::fader),
      outputFader (p.apvts, ParamIDs::output, "Out", Control::Style::fader)
{
    setLookAndFeel (&lnf);

    for (auto* b : { &freezeButton, &trailsButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::freeze, freezeButton);
    trailsAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::trails, trailsButton);

    for (auto* c : std::initializer_list<juce::Component*> { &stripA, &stripB, &routing, &inputFader, &outputFader })
        addAndMakeVisible (c);

    setSize (1320, 590);
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
    // Pedalboard: dark with a soft light falloff from the top
    g.setGradientFill (juce::ColourGradient (Palette::boardHi, (float) getWidth() * 0.5f, 0.0f,
                                             Palette::board, (float) getWidth() * 0.5f, (float) getHeight(), false));
    g.fillAll();

    // Drop shadows under the pedals
    for (auto* s : { &stripA, &stripB })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (s->getBounds().toFloat().translated (0.0f, 6.0f).expanded (2.0f), 18.0f);
    }

    g.setColour (Palette::cream);
    g.setFont (sans (24.0f, true, 0.18f));
    g.drawText ("ECHO SPACE", 24, 16, 300, 30, juce::Justification::centredLeft);
    g.setColour (Palette::amber);
    g.setFont (sans (11.0f, true, 0.3f));
    g.drawText ("DUAL DELAY  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  REVERB", 26, 46, 300, 14, juce::Justification::centredLeft);

    g.setColour (Palette::creamDim);
    g.setFont (sans (10.0f, true, 0.3f));
    g.drawText ("ROUTING", routing.getX(), routing.getY() - 16, 120, 12, juce::Justification::centredLeft);
}

void EchoSpaceEditor::resized()
{
    auto r = getLocalBounds().reduced (18);
    auto header = r.removeFromTop (52);
    r.removeFromTop (18);

    header.removeFromLeft (300);
    routing.setBounds (header.removeFromLeft (330).withSizeKeepingCentre (330, 30).translated (0, 8));
    trailsButton.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 30).translated (0, 8));
    header.removeFromRight (10);
    freezeButton.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 30).translated (0, 8));

    // IN fader | pedal A | pedal B | OUT fader. Side faders line up with the pedals' fader rows.
    const int sideW = 70, gap = 18;
    auto inCol  = r.removeFromLeft (sideW);  r.removeFromLeft (gap);
    auto outCol = r.removeFromRight (sideW); r.removeFromRight (gap);

    const int w = (r.getWidth() - 22) / 2;
    stripA.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (22);
    stripB.setBounds (r);

    // Match the strip's fader row: top inset 78 + 128 + 12, bottom inset 30
    const int faderTop = stripA.getY() + 78 + 128 + 12;
    const int faderBottom = stripA.getBottom() - 30;
    inputFader.setBounds  (inCol.getX(),  faderTop, sideW, faderBottom - faderTop);
    outputFader.setBounds (outCol.getX(), faderTop, sideW, faderBottom - faderTop);
}
