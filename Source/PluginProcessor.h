#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Strip.h"

// Parameter IDs. Strip parameters are prefixed "a_" or "b_" (e.g. "a_time").
namespace ParamIDs
{
    // per strip
    inline constexpr auto on       = "on";
    inline constexpr auto engine   = "engine";
    inline constexpr auto time     = "time";
    inline constexpr auto sync     = "sync";
    inline constexpr auto division = "division";
    inline constexpr auto feedback = "feedback";
    inline constexpr auto mix      = "mix";
    inline constexpr auto tone     = "tone";
    inline constexpr auto control1 = "control1";
    inline constexpr auto control2 = "control2";
    inline constexpr auto lowCut   = "lowCut";
    inline constexpr auto highCut  = "highCut";
    inline constexpr auto width    = "width";
    inline constexpr auto modRate  = "modRate";
    inline constexpr auto duck     = "duck";

    // global
    inline constexpr auto routing  = "routing";
    inline constexpr auto freeze   = "freeze";
    inline constexpr auto trails   = "trails";
    inline constexpr auto output   = "output";

    inline juce::String strip (const juce::String& prefix, const char* id) { return prefix + id; }
}

class EchoSpaceProcessor : public juce::AudioProcessor
{
public:
    EchoSpaceProcessor();
    ~EchoSpaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Shared lists and lookups (also used by the editor)
    static juce::StringArray engineNames();
    static juce::StringArray divisionNames();
    static juce::StringArray routingNames();
    static float divisionInBeats (int index);
    static juce::String control1Name (es::EngineType);
    static juce::String control2Name (es::EngineType);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct StripParams
    {
        std::atomic<float> *on, *engine, *time, *sync, *division, *feedback, *mix, *tone,
                           *control1, *control2, *lowCut, *highCut, *width, *modRate, *duck;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    StripParams bindStrip (const juce::String& prefix);
    es::StripSettings readStrip (const StripParams&) const;

    es::EchoSpaceDSP dsp;
    StripParams stripA {}, stripB {};
    std::atomic<float> *pRouting = nullptr, *pFreeze = nullptr, *pTrails = nullptr, *pOutput = nullptr;
    double bpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoSpaceProcessor)
};
