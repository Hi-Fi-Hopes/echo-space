#pragma once

// Discrete choices behind some Control 1 / Control 2 knobs. Shared by the DSP and the UI
// so the readout always matches what you hear.

#include <algorithm>
#include <cmath>

namespace es
{
    inline int pickIndex (float t, int count) { return std::clamp ((int) std::lround (t * (float) (count - 1)), 0, count - 1); }

    // DUAL: second delay time as a ratio of the first
    constexpr int numDualRatios = 9;
    constexpr float dualRatios[numDualRatios]          = { 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f };
    constexpr const char* dualRatioNames[numDualRatios] = { "1/4", "1/3", "1/2", "2/3", "3/4", "1", "4/3", "3/2", "2" };
    inline int dualRatioIndex (float t) { return pickIndex (t, numDualRatios); }

    // PATTERN: tap positions as fractions of the Time setting (last tap = 1.0 feeds back)
    constexpr int numPatterns = 8;
    constexpr const char* patternNames[numPatterns] = { "Quarters", "Triplets", "Dotted", "Gallop", "Push", "Rush", "Swing", "Late" };
    inline int patternIndex (float t) { return pickIndex (t, numPatterns); }

    // SHIMMER: pitch of the shimmer voice
    constexpr int numShimmerIntervals = 3;
    constexpr float shimmerRatios[numShimmerIntervals]             = { 1.4983f, 2.0f, 2.9966f };
    constexpr const char* shimmerIntervalNames[numShimmerIntervals] = { "+5th", "+Oct", "+Oct+5th" };
    inline int shimmerIntervalIndex (float t) { return pickIndex (t, numShimmerIntervals); }

    // SWELL: attack time of the volume swell, 10 ms .. 2 s
    inline float swellAttackSeconds (float t) { return 0.01f * std::pow (200.0f, std::clamp (t, 0.0f, 1.0f)); }
}
