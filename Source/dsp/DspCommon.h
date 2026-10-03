#pragma once

// Small, dependency-free DSP building blocks shared by every engine.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace es
{
    constexpr float pi    = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline float clamp01 (float x) noexcept           { return std::min (1.0f, std::max (0.0f, x)); }
    inline float lerp (float a, float b, float t)      { return a + (b - a) * t; }
    inline float dbToGain (float db) noexcept          { return std::pow (10.0f, db * 0.05f); }

    // Exponential map of a 0..1 control onto [lo, hi].
    inline float expMap (float t, float lo, float hi)  { return lo * std::pow (hi / lo, clamp01 (t)); }

    //==========================================================================
    // Linear ramp toward a target, reached after a set time.
    struct Ramp
    {
        void reset (double sampleRate, double seconds)
        {
            steps = std::max (1, (int) (sampleRate * seconds));
            setCurrentAndTarget (target);
        }
        void setCurrentAndTarget (float v) { current = target = v; remaining = 0; }
        void setTarget (float v)
        {
            if (v == target) return;
            target = v;
            remaining = steps;
            step = (target - current) / (float) steps;
        }
        float next() noexcept
        {
            if (remaining > 0)
            {
                current += step;
                if (--remaining == 0) current = target;
            }
            return current;
        }
        float getCurrent() const noexcept { return current; }
        float getTarget()  const noexcept { return target; }
        bool  isRamping()  const noexcept { return remaining > 0; }

        float current = 0.0f, target = 0.0f, step = 0.0f;
        int steps = 1, remaining = 0;
    };

    //==========================================================================
    // Circular buffer read k samples back (k = 1 is the most recent write).
    struct DelayBuffer
    {
        void allocate (int maxDelaySamples)
        {
            int size = 1;
            while (size < maxDelaySamples + 8) size <<= 1;
            data.assign ((size_t) size, 0.0f);
            mask = size - 1;
            w = 0;
        }
        void clear()                    { std::fill (data.begin(), data.end(), 0.0f); }
        void push (float x) noexcept    { data[(size_t) w] = x; w = (w + 1) & mask; }
        float at (int k) const noexcept { return data[(size_t) ((w - k) & mask)]; }
        int maxDelay() const noexcept   { return mask - 4; }

        float readLinear (float d) const noexcept
        {
            d = std::min ((float) maxDelay(), std::max (1.0f, d));
            const int i = (int) d;
            const float f = d - (float) i;
            return lerp (at (i), at (i + 1), f);
        }

        float readHermite (float d) const noexcept
        {
            d = std::min ((float) maxDelay(), std::max (2.0f, d));
            const int i = (int) d;
            const float f = d - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        std::vector<float> data;
        int mask = 0, w = 0;
    };

    //==========================================================================
    // Schroeder allpass with a fixed integer length (diffusion).
    struct Allpass
    {
        void allocate (int length)  { buf.assign ((size_t) std::max (1, length), 0.0f); idx = 0; }
        void clear()                { std::fill (buf.begin(), buf.end(), 0.0f); }
        float process (float x, float g) noexcept
        {
            const float d = buf[(size_t) idx];
            const float v = x + g * d;
            buf[(size_t) idx] = v;
            if (++idx == (int) buf.size()) idx = 0;
            return d - g * v;
        }
        std::vector<float> buf;
        int idx = 0;
    };

    //==========================================================================
    // One-pole TPT filter: lowpass output, highpass = input - lowpass.
    struct OnePole
    {
        void setCutoff (float hz, float sampleRate) noexcept
        {
            hz = std::min (hz, sampleRate * 0.45f);
            const float g = std::tan (pi * hz / sampleRate);
            G = g / (1.0f + g);
        }
        float lowpass (float x) noexcept
        {
            const float v = (x - s) * G;
            const float y = v + s;
            s = y + v;
            return y;
        }
        float highpass (float x) noexcept { return x - lowpass (x); }
        void clear() noexcept { s = 0.0f; }
        float G = 1.0f, s = 0.0f;
    };

    //==========================================================================
    // 12 dB/oct state-variable filter (Cytomic/Zavalishin TPT form).
    struct SVF
    {
        void setCutoff (float hz, float sampleRate, float q = 0.7071f) noexcept
        {
            hz = std::min (hz, sampleRate * 0.45f);
            const float g = std::tan (pi * hz / sampleRate);
            k  = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void tick (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            low = v2; band = v1; high = x - k * v1 - v2;
        }
        float lowpass (float x) noexcept  { tick (x); return low; }
        float highpass (float x) noexcept { tick (x); return high; }
        void clear() noexcept { ic1 = ic2 = 0.0f; }

        float k = 1.414f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0, low = 0, band = 0, high = 0;
    };

    //==========================================================================
    // Small fast random generator for drift / noise.
    struct Rng
    {
        uint32_t state = 0x12345678u;
        float bipolar() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) (int32_t) state * (1.0f / 2147483648.0f);
        }
    };
}
