#pragma once

// Phase 2 engines: Reverse, Dual / Pattern (multi-tap), Spring and Shimmer.

#include "DspCommon.h"
#include "Settings.h"
#include "EngineTables.h"
#include "ReverbEngine.h"
#include <array>

namespace es
{
    // Clean below 0.8, rounds off smoothly to a ceiling of 1.0.
    inline float softLimit (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f);
        return x < 0.0f ? -y : y;
    }

    //==========================================================================
    /*
        REVERSE: plays each slice of the last "Time" worth of audio backwards.
        Two overlapping read heads with crossfaded windows keep it smooth.

        Control 1 = Smear  (diffuses the reversed slices into a wash)
        Control 2 = Octave (blends in an octave-up reversed voice)
    */
    class ReverseEngine
    {
    public:
        static constexpr float maxSliceMs = 2000.0f;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            const int maxSamples = (int) std::ceil (3.0f * maxSliceMs * 0.001f * sr) + 64;
            bufL.allocate (maxSamples);
            bufR.allocate (maxSamples);
            static constexpr float smearMs[3] = { 7.3f, 11.9f, 17.3f };
            for (int i = 0; i < 3; ++i)
            {
                apL[(size_t) i].allocate ((int) (smearMs[i] * 0.001f * sr));
                apR[(size_t) i].allocate ((int) (smearMs[i] * 1.09f * 0.001f * sr));
            }
            feedback.reset (newSampleRate, 0.03);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            reset();
        }

        void reset()
        {
            bufL.clear(); bufR.clear();
            for (auto& a : apL) a.clear();
            for (auto& a : apR) a.clear();
            lpL.clear(); lpR.clear(); hpL.clear(); hpR.clear();
            head[0] = { 0, 0 };
            head[1] = { -1, 0 };          // second head starts half a slice later
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            if (first)
            {
                feedback.setCurrentAndTarget (s.feedback);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            feedback.setTarget (s.feedback);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const int slice = 2 * (int) (std::clamp (s.timeMs, 50.0f, maxSliceMs) * 0.0005f * sr);   // even length
            const float toneHz = expMap (s.tone, 700.0f, 20000.0f);
            lpL.setCutoff (toneHz, sr); lpR.setCutoff (toneHz, sr);
            hpL.setCutoff (40.0f, sr);  hpR.setCutoff (40.0f, sr);
            const float smear = 0.7f * s.control1;
            const float octave = s.control2;

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float fb  = lerp (feedback.next(), 1.0f, fz);
                const float ing = inputGain.next() * (1.0f - fz);

                float revL = 0.0f, revR = 0.0f;
                for (auto& h : head)
                {
                    if (h.length <= 0) { h.length = slice; if (h.pos < 0) h.pos = slice / 2; }   // first window
                    // Whole-sample read positions: no interpolation, so frozen loops don't dull.
                    const int k = h.pos;
                    const float w = std::sin (pi * (float) h.pos / (float) h.length);   // equal-power crossfade
                    const float nL = bufL.at (2 * k + 1), nR = bufR.at (2 * k + 1);
                    float l = nL, r = nR;
                    if (octave > 0.001f)
                    {
                        const float oL = bufL.at (3 * k + 1), oR = bufR.at (3 * k + 1);
                        l = lerp (nL, oL, octave);
                        r = lerp (nR, oR, octave);
                    }
                    revL += l * w;
                    revR += r * w;

                    if (++h.pos >= h.length) { h.pos = 0; h.length = slice; }
                }

                if (smear > 0.001f)
                    for (size_t k = 0; k < 3; ++k) { revL = apL[k].process (revL, smear); revR = apR[k].process (revR, smear); }

                const float shapedL = lpL.lowpass (hpL.highpass (revL));
                const float shapedR = lpR.lowpass (hpR.highpass (revR));
                const float echoL = lerp (shapedL, revL, fz);
                const float echoR = lerp (shapedR, revR, fz);

                // Fully frozen: stop writing, so the heads keep cycling the held slice at a constant level.
                if (fz < 0.999f)
                {
                    bufL.push (softLimit (inL[i] * ing + echoL * fb));
                    bufR.push (softLimit (inR[i] * ing + echoR * fb));
                }

                outL[i] = echoL;
                outR[i] = echoR;
            }
        }

    private:
        struct Head { int pos, length; };
        float sr = 44100.0f;
        DelayBuffer bufL, bufR;
        std::array<Allpass, 3> apL, apR;
        OnePole lpL, lpR, hpL, hpR;
        std::array<Head, 2> head {};
        Ramp feedback, freezeAmt, inputGain;
        bool first = true;
    };

    //==========================================================================
    /*
        DUAL:    two delays side by side. The second runs at "Ratio" x the first.
                 Control 1 = Ratio, Control 2 = Spread (delay 1 left, delay 2 right)

        PATTERN: rhythmic multi-tap. Taps fall inside the Time setting following one
                 of eight patterns; the last tap feeds back and repeats the pattern.
                 Control 1 = Pattern, Control 2 = Spread (taps alternate sides)
    */
    class MultiTapEngine
    {
    public:
        static constexpr float maxDelayMs = 4000.0f;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            const int maxSamples = (int) std::ceil ((maxDelayMs + 10.0f) * 0.001f * sr);
            buf1.allocate (maxSamples);
            buf2.allocate (maxSamples);
            time1.reset (newSampleRate, 0.15);
            time2.reset (newSampleRate, 0.15);
            feedback.reset (newSampleRate, 0.03);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            reset();
        }

        void reset()
        {
            buf1.clear(); buf2.clear();
            lp1.clear(); lp2.clear(); hp1.clear(); hp2.clear();
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            const bool dual = s.engine == EngineType::dual;
            const float t1 = std::clamp (s.timeMs, 1.0f, maxDelayMs) * 0.001f * sr;
            const float t2 = dual ? std::clamp (s.timeMs * dualRatios[dualRatioIndex (s.control1)], 1.0f, maxDelayMs) * 0.001f * sr : t1;

            if (first)
            {
                time1.setCurrentAndTarget (t1);
                time2.setCurrentAndTarget (t2);
                feedback.setCurrentAndTarget (s.feedback);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            time1.setTarget (t1);
            time2.setTarget (t2);
            feedback.setTarget (s.feedback);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const float toneHz = expMap (s.tone, 700.0f, 20000.0f);
            lp1.setCutoff (toneHz, sr); lp2.setCutoff (toneHz, sr);
            hp1.setCutoff (40.0f, sr);  hp2.setCutoff (40.0f, sr);
            const float spread = s.control2;

            const Pattern& pat = patterns[(size_t) patternIndex (s.control1)];

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float fb  = lerp (feedback.next(), 1.0f, fz);
                const float ing = inputGain.next() * (1.0f - fz);
                const float d1 = time1.next(), d2 = time2.next();
                const float in = 0.5f * (inL[i] + inR[i]) * ing;

                if (dual)
                {
                    const float e1 = buf1.readHermite (d1);
                    const float e2 = buf2.readHermite (d2);
                    buf1.push (softLimit (lerp (lp1.lowpass (hp1.highpass (in + e1 * fb)), in + e1 * fb, fz)));
                    buf2.push (softLimit (lerp (lp2.lowpass (hp2.highpass (in + e2 * fb)), in + e2 * fb, fz)));

                    // Delay 1 pans left and delay 2 pans right as Spread goes up.
                    outL[i] = e1 + e2 * (1.0f - spread);
                    outR[i] = e2 + e1 * (1.0f - spread);
                    outL[i] *= 0.7f; outR[i] *= 0.7f;          // two voices summed: keep the level in line
                }
                else
                {
                    float l = 0.0f, r = 0.0f;
                    for (int k = 0; k < pat.count; ++k)
                    {
                        const float tap = buf1.readHermite (std::max (2.0f, d1 * pat.at[k])) * pat.gain[k];
                        const float pan = (k & 1) == 0 ? -spread : spread;
                        l += tap * std::min (1.0f, 1.0f - pan);
                        r += tap * std::min (1.0f, 1.0f + pan);
                    }
                    const float last = buf1.readHermite (d1);          // the full-length tap feeds back
                    const float x = in + last * fb;
                    buf1.push (softLimit (lerp (lp1.lowpass (hp1.highpass (x)), x, fz)));
                    outL[i] = l * 0.75f;
                    outR[i] = r * 0.75f;
                }
            }
        }

    private:
        struct Pattern { int count; float at[4]; float gain[4]; };
        static constexpr std::array<Pattern, numPatterns> patterns {{
            { 4, { 0.25f, 0.5f, 0.75f, 1.0f },          { 1.0f, 0.85f, 0.75f, 0.7f } },   // Quarters
            { 3, { 1.0f / 3, 2.0f / 3, 1.0f, 0 },       { 1.0f, 0.85f, 0.75f, 0 } },      // Triplets
            { 3, { 0.375f, 0.75f, 1.0f, 0 },            { 1.0f, 0.85f, 0.7f, 0 } },       // Dotted
            { 4, { 0.25f, 0.375f, 0.75f, 1.0f },        { 1.0f, 0.7f, 0.85f, 0.7f } },    // Gallop
            { 3, { 0.5f, 0.625f, 1.0f, 0 },             { 1.0f, 0.8f, 0.75f, 0 } },       // Push
            { 4, { 0.125f, 0.25f, 0.5f, 1.0f },         { 0.6f, 0.75f, 0.9f, 0.8f } },    // Rush
            { 4, { 1.0f / 6, 0.5f, 5.0f / 6, 1.0f },    { 0.8f, 1.0f, 0.8f, 0.75f } },    // Swing
            { 3, { 0.75f, 0.875f, 1.0f, 0 },            { 1.0f, 0.6f, 0.8f, 0 } },        // Late
        }};

        float sr = 44100.0f;
        DelayBuffer buf1, buf2;
        OnePole lp1, lp2, hp1, hp2;
        Ramp time1, time2, feedback, freezeAmt, inputGain;
        bool first = true;
    };

    //==========================================================================
    /*
        SPRING: three "springs", each a feedback delay with a long chain of
        allpass filters inside the loop. The allpasses smear high frequencies
        later than lows, which gives the springy chirp and drip.

        Control 1 = Drip    (how strongly each splash chirps)
        Control 2 = Tension (shorter, tighter springs as it goes up)
        Speed     = slow wobble of the springs
    */
    class SpringEngine
    {
    public:
        static constexpr int numSprings = 3;
        static constexpr int numStages = 40;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            pre.allocate ((int) (sr * 0.27f));
            for (auto& l : lines) l.allocate ((int) (sr * 0.09f));
            preDelay.reset (newSampleRate, 0.2);
            tensionAmt.reset (newSampleRate, 0.25);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            reset();
        }

        void reset()
        {
            pre.clear();
            for (auto& l : lines) l.clear();
            for (auto& sp : stages) for (auto& st : sp) st = 0.0f;
            for (auto& d : damp) d.clear();
            inHp.clear(); inLp.clear();
            for (int i = 0; i < numSprings; ++i) lfo[(size_t) i] = (float) i * 2.1f;
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            const float preSamples = std::clamp (s.timeMs / preDelayDivisor, 0.0f, 250.0f) * 0.001f * sr;
            if (first)
            {
                preDelay.setCurrentAndTarget (preSamples);
                tensionAmt.setCurrentAndTarget (s.control2);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            preDelay.setTarget (preSamples);
            tensionAmt.setTarget (s.control2);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const float rt60 = 0.2f * std::pow (100.0f, clamp01 (s.feedback));
            const float apCoef = 0.45f + 0.33f * s.control1;               // drip
            const float brightHz = expMap (s.tone, 2000.0f, 8000.0f);
            inHp.setCutoff (120.0f, sr);
            inLp.setCutoff (brightHz, sr);
            for (auto& d : damp) d.setCutoff (brightHz * 0.8f, sr);

            static constexpr float baseMs[numSprings] = { 37.1f, 43.3f, 51.7f };
            const float lfoInc = twoPi * std::max (0.05f, s.modRateHz) * 0.5f / sr;
            const float lfoDepth = 0.0004f * sr;

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float ing = inputGain.next() * (1.0f - fz);
                const float tension = tensionAmt.next();
                const float lengthScale = 1.3f - 0.6f * tension;

                pre.push (0.5f * (inL[i] + inR[i]) * ing);
                const float pd = preDelay.next();
                const float p = pd < 1.0f ? pre.at (1) : pre.readLinear (pd);
                const float x = inLp.lowpass (inHp.highpass (p));

                std::array<float, numSprings> y {};
                for (int k = 0; k < numSprings; ++k)
                {
                    lfo[(size_t) k] += lfoInc * (1.0f + 0.23f * (float) k);
                    if (lfo[(size_t) k] > twoPi) lfo[(size_t) k] -= twoPi;

                    const float lenSec = baseMs[k] * 0.001f * lengthScale;
                    const float len = lenSec * sr + lfoDepth * (1.0f + std::sin (lfo[(size_t) k]));
                    const float g = lerp (std::pow (10.0f, -3.0f * lenSec / rt60), 1.0f, fz);

                    float v = lines[(size_t) k].readLinear (len);
                    v = lerp (damp[(size_t) k].lowpass (v), v, fz);
                    y[(size_t) k] = v;

                    // Chirp: the loop signal runs through the allpass chain each trip
                    float c = x + v * g;
                    for (auto& st : stages[(size_t) k])
                    {
                        const float out = apCoef * c + st;
                        st = c - apCoef * out;
                        c = out;
                    }
                    lines[(size_t) k].push (softLimit (c));
                }

                outL[i] = (y[0] + 0.6f * y[1] - 0.3f * y[2]) * outScale;
                outR[i] = (y[2] + 0.6f * y[1] - 0.3f * y[0]) * outScale;
            }
        }

    private:
        static constexpr float outScale = 2.4f;
        float sr = 44100.0f;
        DelayBuffer pre;
        std::array<DelayBuffer, numSprings> lines;
        std::array<std::array<float, numStages>, numSprings> stages {};
        std::array<OnePole, numSprings> damp;
        std::array<float, numSprings> lfo {};
        OnePole inHp, inLp;
        Ramp preDelay, tensionAmt, freezeAmt, inputGain;
        bool first = true;
    };

    //==========================================================================
    // Two-tap delay-line pitch shifter (upwards), one per channel.
    class PitchShifter
    {
    public:
        void prepare (float sampleRate)
        {
            window = 0.05f * sampleRate;
            buf.allocate ((int) window + 8);
            phase = 0.0f;
        }
        void clear() { buf.clear(); phase = 0.0f; }

        float process (float x, float ratio) noexcept
        {
            buf.push (x);
            // Read point slides towards "now" by (ratio - 1) samples per sample.
            phase -= (ratio - 1.0f) / window;
            while (phase < 0.0f) phase += 1.0f;
            float out = 0.0f;
            for (float off : { 0.0f, 0.5f })
            {
                float ph = phase + off;
                if (ph >= 1.0f) ph -= 1.0f;
                out += buf.readLinear (1.0f + ph * window) * std::sin (pi * ph);
            }
            return out;
        }

    private:
        DelayBuffer buf;
        float window = 2400.0f, phase = 0.0f;
    };

    /*
        SHIMMER: a hall reverb whose tail is pitched up and fed back into itself,
        so each pass climbs higher.

        Control 1 = Shimmer  (how much pitched signal is fed back)
        Control 2 = Interval (+5th, +octave, +octave and a 5th)
    */
    class ShimmerEngine
    {
    public:
        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            hall.prepare (newSampleRate);
            shiftL.prepare (sr); shiftR.prepare (sr);
            amount.reset (newSampleRate, 0.05);
            envAttack  = 1.0f - std::exp (-1.0f / (0.01f * sr));
            envRelease = 1.0f - std::exp (-1.0f / (0.4f * sr));
            reset();
        }

        void reset()
        {
            hall.reset();
            shiftL.clear(); shiftR.clear();
            hpL.clear(); hpR.clear(); lpL.clear(); lpR.clear();
            fbL.fill (0.0f); fbR.fill (0.0f);
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            if (first) { amount.setCurrentAndTarget (s.control1); env = 0.0f; first = false; }
            amount.setTarget (s.control1);

            StripSettings h = s;                       // hall voicing underneath
            h.engine   = EngineType::hall;
            h.control1 = 0.75f;                        // size
            h.control2 = 0.45f;                        // modulation

            hpL.setCutoff (350.0f, sr); hpR.setCutoff (350.0f, sr);
            lpL.setCutoff (9000.0f, sr); lpR.setCutoff (9000.0f, sr);
            const float ratio = shimmerRatios[shimmerIntervalIndex (s.control2)];

            // Small chunks: the pitched tail from the previous chunk is mixed into this one.
            for (int pos = 0; pos < n; pos += chunk)
            {
                const int m = std::min (chunk, n - pos);
                for (int i = 0; i < m; ++i)
                {
                    // Regeneration backs off as the tail gets loud, so it can't run away.
                    const float a = amount.next() * 0.55f / (1.0f + 6.0f * env);
                    tmpL[(size_t) i] = inL[pos + i] + fbL[(size_t) i] * a;
                    tmpR[(size_t) i] = inR[pos + i] + fbR[(size_t) i] * a;
                }
                hall.process (tmpL.data(), tmpR.data(), outL + pos, outR + pos, m, h, freeze, inputLevel);

                for (int i = 0; i < m; ++i)
                {
                    const float lvl = std::max (std::abs (outL[pos + i]), std::abs (outR[pos + i]));
                    env += (lvl - env) * (lvl > env ? envAttack : envRelease);
                    fbL[(size_t) i] = softLimit (lpL.lowpass (hpL.highpass (shiftL.process (outL[pos + i], ratio))));
                    fbR[(size_t) i] = softLimit (lpR.lowpass (hpR.highpass (shiftR.process (outR[pos + i], ratio))));
                }
                for (int i = m; i < chunk; ++i) { fbL[(size_t) i] = fbR[(size_t) i] = 0.0f; }
            }
        }

    private:
        static constexpr int chunk = 32;
        float sr = 44100.0f;
        ReverbEngine hall;
        PitchShifter shiftL, shiftR;
        OnePole hpL, hpR, lpL, lpR;
        std::array<float, chunk> fbL {}, fbR {}, tmpL {}, tmpR {};
        Ramp amount;
        float env = 0.0f, envAttack = 0.01f, envRelease = 0.0001f;
        bool first = true;
    };
}
