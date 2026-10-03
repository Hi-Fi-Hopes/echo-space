#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    Attr percent() { return Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; }); }
    Attr ms()      { return Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }); }
    Attr db()      { return Attr().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; }); }
    Attr hz()
    {
        return Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
        {
            if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
            return v >= 10.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v, 2) + " Hz";
        });
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::NormalisableRange<float> pct() { return { 0.0f, 100.0f, 0.1f }; }

    struct StripDefaults
    {
        int engine; float time; bool sync; int division; float feedback, mix, tone, c1, c2;
    };

    void addStrip (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& prefix,
                   const juce::String& name, const StripDefaults& d)
    {
        using namespace juce;
        auto id = [&] (const char* p) { return ParameterID { prefix + p, 1 }; };
        auto nm = [&] (const char* n) { return name + " " + n; };

        layout.add (std::make_unique<AudioParameterBool>   (id (ParamIDs::on),       nm ("On"), true));
        layout.add (std::make_unique<AudioParameterChoice> (id (ParamIDs::engine),   nm ("Engine"), EchoSpaceProcessor::engineNames(), d.engine));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::time),     nm ("Time"), skewed (1.0f, 2000.0f, 400.0f), d.time, ms()));
        layout.add (std::make_unique<AudioParameterBool>   (id (ParamIDs::sync),     nm ("Sync"), d.sync));
        layout.add (std::make_unique<AudioParameterChoice> (id (ParamIDs::division), nm ("Division"), EchoSpaceProcessor::divisionNames(), d.division));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::feedback), nm ("Feedback"), pct(), d.feedback, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::mix),      nm ("Mix"), pct(), d.mix, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::tone),     nm ("Tone"), pct(), d.tone, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::control1), nm ("Control 1"), pct(), d.c1, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::control2), nm ("Control 2"), pct(), d.c2, percent()));

        // Deep edit
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::lowCut),   nm ("Low Cut"),  skewed (20.0f, 2000.0f, 200.0f), 20.0f, hz()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::highCut),  nm ("High Cut"), skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, hz()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::width),    nm ("Width"), NormalisableRange<float> (0.0f, 150.0f, 0.1f), 100.0f, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::modRate),  nm ("Mod Rate"), skewed (0.05f, 8.0f, 1.0f), 0.8f, hz()));
        layout.add (std::make_unique<AudioParameterFloat>  (id (ParamIDs::duck),     nm ("Duck"), pct(), 0.0f, percent()));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { prefix + ParamIDs::drive, 2 }, nm ("Drive"), pct(), 0.0f, percent()));
    }
}

//==============================================================================
// Order must match es::EngineType. New engines are appended so saved projects keep their engine.
juce::StringArray EchoSpaceProcessor::engineNames()
{
    return { "Digital", "Tape", "Room", "Hall", "Plate",
             "Analog", "Reverse", "Spring", "Shimmer", "Dual", "Pattern" };
}
juce::StringArray EchoSpaceProcessor::routingNames()  { return { "Series", "Parallel", "Split" }; }
juce::StringArray EchoSpaceProcessor::divisionNames()
{
    return { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
}

float EchoSpaceProcessor::divisionInBeats (int index)
{
    static constexpr float beats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                       2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

juce::String EchoSpaceProcessor::control1Name (es::EngineType e)
{
    using E = es::EngineType;
    switch (e)
    {
        case E::digital: return "Spread";
        case E::tape:    return "Age";
        case E::room:    return "Size";
        case E::hall:    return "Size";
        case E::plate:   return "Diffusion";
        case E::analog:  return "Mod Depth";
        case E::reverse: return "Smear";
        case E::spring:  return "Drip";
        case E::shimmer: return "Shimmer";
        case E::dual:    return "Ratio";
        case E::pattern: return "Pattern";
    }
    return "Control 1";
}

juce::String EchoSpaceProcessor::control2Name (es::EngineType e)
{
    using E = es::EngineType;
    switch (e)
    {
        case E::digital: return "Crush";
        case E::tape:    return "Wow";
        case E::room:    return "Early";
        case E::hall:    return "Mod";
        case E::plate:   return "Mod";
        case E::analog:  return "Grit";
        case E::reverse: return "Octave";
        case E::spring:  return "Tension";
        case E::shimmer: return "Interval";
        case E::dual:    return "Spread";
        case E::pattern: return "Spread";
    }
    return "Control 2";
}

// Readout text for controls that pick from a set of values instead of a percentage.
// Returns an empty string when the plain "nn%" readout is right.
juce::String EchoSpaceProcessor::controlValueText (es::EngineType e, int which, double percent)
{
    using E = es::EngineType;
    const float t = (float) percent * 0.01f;

    if (e == E::dual && which == 1)
        return "x" + juce::String (es::dualRatioNames[es::dualRatioIndex (t)]);

    if (e == E::pattern && which == 1)
        return juce::String (es::patternNames[es::patternIndex (t)]);

    if (e == E::shimmer && which == 2)
        return juce::String (es::shimmerIntervalNames[es::shimmerIntervalIndex (t)]);

    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoSpaceProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    //                     engine    time  sync  div  fb    mix   tone  c1    c2
    addStrip (layout, "a_", "A", { 1 /*Tape*/, 375.0f, true,  6 /*1/8D*/, 35.0f, 30.0f, 55.0f, 30.0f, 25.0f });
    addStrip (layout, "b_", "B", { 3 /*Hall*/, 160.0f, false, 8 /*1/4*/,  50.0f, 25.0f, 55.0f, 60.0f, 30.0f });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::routing, 1 }, "Routing", routingNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::input, 2 },   "Input",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db()));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::freeze, 1 },  "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::trails, 1 },  "Trails", true));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::output, 1 },  "Output",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db()));
    return layout;
}

//==============================================================================
EchoSpaceProcessor::EchoSpaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EchoSpaceState", createLayout())
{
    stripA   = bindStrip ("a_");
    stripB   = bindStrip ("b_");
    pRouting = apvts.getRawParameterValue (ParamIDs::routing);
    pInput   = apvts.getRawParameterValue (ParamIDs::input);
    pFreeze  = apvts.getRawParameterValue (ParamIDs::freeze);
    pTrails  = apvts.getRawParameterValue (ParamIDs::trails);
    pOutput  = apvts.getRawParameterValue (ParamIDs::output);
}

EchoSpaceProcessor::StripParams EchoSpaceProcessor::bindStrip (const juce::String& prefix)
{
    auto get = [&] (const char* id) { auto* p = apvts.getRawParameterValue (prefix + id); jassert (p != nullptr); return p; };

    return { get (ParamIDs::on), get (ParamIDs::engine), get (ParamIDs::time), get (ParamIDs::sync),
             get (ParamIDs::division), get (ParamIDs::feedback), get (ParamIDs::mix), get (ParamIDs::tone),
             get (ParamIDs::control1), get (ParamIDs::control2), get (ParamIDs::lowCut), get (ParamIDs::highCut),
             get (ParamIDs::width), get (ParamIDs::modRate), get (ParamIDs::duck), get (ParamIDs::drive) };
}

es::StripSettings EchoSpaceProcessor::readStrip (const StripParams& p) const
{
    es::StripSettings s;
    s.enabled = p.on->load() > 0.5f;
    s.engine  = (es::EngineType) juce::jlimit (0, es::numEngines - 1, (int) p.engine->load());

    // Synced delay time comes from the note division; reverbs always use the Time knob (pre-delay).
    if (! es::isReverb (s.engine) && p.sync->load() > 0.5f)
        s.timeMs = (float) (60000.0 / bpm) * divisionInBeats ((int) p.division->load());
    else
        s.timeMs = p.time->load();

    s.feedback  = p.feedback->load() * 0.01f;
    s.mix       = p.mix->load() * 0.01f;
    s.tone      = p.tone->load() * 0.01f;
    s.control1  = p.control1->load() * 0.01f;
    s.control2  = p.control2->load() * 0.01f;
    s.lowCutHz  = p.lowCut->load();
    s.highCutHz = p.highCut->load();
    s.width     = p.width->load() * 0.01f;
    s.modRateHz = p.modRate->load();
    s.duck      = p.duck->load() * 0.01f;
    s.drive     = p.drive->load() * 0.01f;
    return s;
}

bool EchoSpaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void EchoSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate, samplesPerBlock);
}

void EchoSpaceProcessor::reset()
{
    dsp.reset();
}

void EchoSpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() < 2)
        return;

    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 1.0)
                    bpm = *hostBpm;

    const auto a = readStrip (stripA);
    const auto b = readStrip (stripB);

    es::GlobalSettings g;
    g.routing  = (es::Routing) juce::jlimit (0, 2, (int) pRouting->load());
    g.inputDb  = pInput->load();
    g.freeze   = pFreeze->load() > 0.5f;
    g.trails   = pTrails->load() > 0.5f;
    g.outputDb = pOutput->load();

    // The DSP was sized for samplesPerBlock; split anything bigger.
    const int maxBlock = juce::jmax (1, getBlockSize());
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    for (int pos = 0; pos < buffer.getNumSamples(); pos += maxBlock)
    {
        const int n = juce::jmin (maxBlock, buffer.getNumSamples() - pos);
        dsp.process (left + pos, right + pos, n, a, b, g);
    }
}

//==============================================================================
juce::AudioProcessorEditor* EchoSpaceProcessor::createEditor()
{
    return new EchoSpaceEditor (*this);
}

void EchoSpaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EchoSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoSpaceProcessor();
}
