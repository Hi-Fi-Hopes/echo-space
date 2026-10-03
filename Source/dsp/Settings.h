#pragma once

namespace es
{
    // Order matters: it's the order of the engine selector. New engines get appended.
    enum class EngineType { digital = 0, tape, room, hall, plate,           // phase 1
                            analog, reverse, spring, shimmer, dual, pattern }; // phase 2
    constexpr int numEngines = 11;

    inline bool isReverb (EngineType e)
    {
        return e == EngineType::room || e == EngineType::hall || e == EngineType::plate
            || e == EngineType::spring || e == EngineType::shimmer;
    }

    // Engines that respond to the Speed (mod rate) knob.
    inline bool usesSpeed (EngineType e)
    {
        return e == EngineType::tape || e == EngineType::analog || e == EngineType::room || e == EngineType::hall
            || e == EngineType::plate || e == EngineType::spring || e == EngineType::shimmer;
    }

    enum class Routing { series = 0, parallel, split };

    // Everything one strip needs for one block. Units are already resolved
    // (sync converted to ms, percentages converted to 0..1).
    struct StripSettings
    {
        bool       enabled   = true;
        EngineType engine    = EngineType::digital;

        float timeMs    = 375.0f;  // delay time, or 8x the reverb pre-delay
        float feedback  = 0.35f;   // repeats / decay, 0..1
        float mix       = 0.3f;    // 0..1
        float tone      = 0.6f;    // 0 dark .. 1 bright
        float control1  = 0.3f;    // engine specific, 0..1
        float control2  = 0.3f;    // engine specific, 0..1

        // Deep-edit page
        float lowCutHz  = 20.0f;   // high-pass on the wet signal
        float highCutHz = 20000.0f;// low-pass on the wet signal
        float width     = 1.0f;    // 0 mono .. 1.5 extra wide
        float modRateHz = 0.8f;
        float duck      = 0.0f;    // 0..1
        float drive     = 0.0f;    // 0..1, saturation before the engine
    };

    struct GlobalSettings
    {
        Routing routing   = Routing::series;
        float   inputDb   = 0.0f;
        bool    freeze    = false;
        bool    trails    = true;
        float   outputDb  = 0.0f;
    };

    // Reverb pre-delay is the Time knob divided by this (2000 ms knob = 250 ms pre-delay).
    constexpr float preDelayDivisor = 8.0f;
}
