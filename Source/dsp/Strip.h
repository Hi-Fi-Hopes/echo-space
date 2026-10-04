#pragma once

#include "DelayEngine.h"
#include "ReverbEngine.h"
#include "ExtraEngines.h"
#include "Phase3Engines.h"

namespace es
{
    /*
        One effect strip: picks an engine, then handles everything common to all
        engines -- on/off with trails, click-free engine switching, wet filters,
        stereo width, ducking and the dry/wet balance.

        process() returns the wet signal and a per-sample dry gain separately, so
        the routing stage can combine two strips without doubling the dry signal.
    */
    class Strip
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = (float) sampleRate;
            delay.prepare (sampleRate);
            reverb.prepare (sampleRate);
            reverse.prepare (sampleRate);
            multiTap.prepare (sampleRate);
            spring.prepare (sampleRate);
            shimmer.prepare (sampleRate);
            oilCan.prepare (sampleRate);
            decayEngine.prepare (sampleRate);
            swell.prepare (sampleRate);

            for (auto* v : { &inL, &inR, &wetL, &wetR })
                v->assign ((size_t) maxBlock, 0.0f);

            onAmt.reset (sampleRate, 0.02);
            switchGain.reset (sampleRate, 0.015);
            mixAmt.reset (sampleRate, 0.03);
            widthAmt.reset (sampleRate, 0.03);

            duckAttack  = 1.0f - std::exp (-1.0f / (0.003f * sr));
            duckRelease = 1.0f - std::exp (-1.0f / (0.200f * sr));
            reset();
        }

        void reset()
        {
            resetEngines();
            for (auto& f : wetFilters) f.clear();
            env = 0.0f;
            first = true;
            pendingSwitch = false;
            tailCleared = false;
        }

        // dryGainOut receives the per-sample gain the caller should apply to the dry signal.
        void process (const float* inLeft, const float* inRight, float* outWetL, float* outWetR, float* dryGainOut,
                      int n, const StripSettings& s, bool freeze, bool trails)
        {
            const float mixTarget = s.mix;
            if (first)
            {
                current = s.engine;
                onAmt.setCurrentAndTarget (s.enabled ? 1.0f : 0.0f);
                switchGain.setCurrentAndTarget (1.0f);
                mixAmt.setCurrentAndTarget (mixTarget);
                widthAmt.setCurrentAndTarget (s.width);
                first = false;
            }
            onAmt.setTarget (s.enabled ? 1.0f : 0.0f);
            mixAmt.setTarget (mixTarget);
            widthAmt.setTarget (s.width);

            // Engine change: fade the wet out, swap and clear, fade back in.
            if (s.engine != current && ! pendingSwitch)
            {
                pendingSwitch = true;
                switchGain.setTarget (0.0f);
            }

            // Strip off with trails off: once faded out, clear the tail and stop processing.
            const bool idle = ! s.enabled && ! trails && onAmt.getCurrent() == 0.0f && ! onAmt.isRamping();
            if (s.enabled) tailCleared = false;

            StripSettings engineSettings = s;
            engineSettings.engine = current;

            // Wet filters on the deep-edit page
            const float lo = std::max (20.0f, s.lowCutHz), hi = std::min (20000.0f, s.highCutHz);
            wetFilters[0].setCutoff (lo, sr); wetFilters[1].setCutoff (lo, sr);
            wetFilters[2].setCutoff (hi, sr); wetFilters[3].setCutoff (hi, sr);
            const bool useLowCut = lo > 21.0f, useHighCut = hi < 19900.0f;

            if (idle)
            {
                if (! tailCleared) { resetEngines(); tailCleared = true; }
                for (int i = 0; i < n; ++i) { outWetL[i] = outWetR[i] = 0.0f; dryGainOut[i] = 1.0f; }
                return;
            }

            // Engine input: a strip that is off (with trails) stops taking new input.
            // The engine ramps its own input gain, so pass the target level.
            const float inputLevel = s.enabled ? 1.0f : 0.0f;
            std::copy (inLeft,  inLeft  + n, inL.begin());
            std::copy (inRight, inRight + n, inR.begin());

            // Drive: saturation on the way into the engine (the dry signal stays clean).
            // Normalised so a -6 dBFS signal comes out at the same level, quieter parts come up.
            if (s.drive > 0.001f)
            {
                const float g = 1.0f + 7.0f * s.drive;
                const float norm = 0.5f / std::tanh (0.5f * g);
                for (int i = 0; i < n; ++i)
                {
                    inL[(size_t) i] = lerp (inL[(size_t) i], std::tanh (g * inL[(size_t) i]) * norm, s.drive);
                    inR[(size_t) i] = lerp (inR[(size_t) i], std::tanh (g * inR[(size_t) i]) * norm, s.drive);
                }
            }

            auto* il = inL.data(); auto* ir = inR.data(); auto* wl = wetL.data(); auto* wr = wetR.data();
            switch (current)
            {
                case EngineType::digital:
                case EngineType::tape:
                case EngineType::analog:  delay.process    (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::room:
                case EngineType::hall:
                case EngineType::plate:
                case EngineType::dome:    reverb.process   (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::oilCan:  oilCan.process   (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::decay:   decayEngine.process (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::swell:   swell.process    (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::reverse: reverse.process  (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::dual:
                case EngineType::pattern: multiTap.process (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::spring:  spring.process   (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
                case EngineType::shimmer: shimmer.process  (il, ir, wl, wr, n, engineSettings, freeze, inputLevel); break;
            }

            for (int i = 0; i < n; ++i)
            {
                float l = wetL[(size_t) i], r = wetR[(size_t) i];

                if (useLowCut)  { l = wetFilters[0].highpass (l); r = wetFilters[1].highpass (r); }
                if (useHighCut) { l = wetFilters[2].lowpass (l);  r = wetFilters[3].lowpass (r); }

                // Width (mid/side on the wet only)
                const float w = widthAmt.next();
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
                l = mid + side; r = mid - side;

                // Ducking: the wet dips while you play, swells back when you stop.
                const float level = std::max (std::abs (inLeft[i]), std::abs (inRight[i]));
                env += (level - env) * (level > env ? duckAttack : duckRelease);
                const float duckGain = 1.0f - s.duck * 0.94f * clamp01 (env * 4.0f);

                const float on  = onAmt.next();
                const float mix = mixAmt.next();
                const float sw  = switchGain.next();

                const float wetGain = std::sin (mix * 0.5f * pi) * duckGain * sw * (trails ? 1.0f : on);
                const float dryGain = lerp (1.0f, std::cos (mix * 0.5f * pi), on);

                outWetL[i]    = l * wetGain;
                outWetR[i]    = r * wetGain;
                dryGainOut[i] = dryGain;
            }

            if (pendingSwitch && switchGain.getCurrent() == 0.0f && ! switchGain.isRamping())
            {
                current = s.engine;
                resetEngines();
                for (auto& f : wetFilters) f.clear();
                pendingSwitch = false;
                switchGain.setTarget (1.0f);
            }
        }

        EngineType currentEngine() const { return current; }

    private:
        float sr = 44100.0f;
        void resetEngines()
        {
            delay.reset(); reverb.reset(); reverse.reset(); multiTap.reset(); spring.reset(); shimmer.reset();
            oilCan.reset(); decayEngine.reset(); swell.reset();
        }

        DelayEngine delay;
        ReverbEngine reverb;
        ReverseEngine reverse;
        MultiTapEngine multiTap;
        SpringEngine spring;
        ShimmerEngine shimmer;
        OilCanEngine oilCan;
        DecayEngine decayEngine;
        SwellEngine swell;
        EngineType current = EngineType::digital;

        std::vector<float> inL, inR, wetL, wetR;
        SVF wetFilters[4];   // HP L, HP R, LP L, LP R
        Ramp onAmt, switchGain, mixAmt, widthAmt;
        float env = 0.0f, duckAttack = 0.1f, duckRelease = 0.001f;
        bool first = true, pendingSwitch = false, tailCleared = false;
    };

    //==========================================================================
    // Two strips + routing + output gain.
    class EchoSpaceDSP
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            a.prepare (sampleRate, maxBlock);
            b.prepare (sampleRate, maxBlock);
            for (auto* v : { &midL, &midR, &wAL, &wAR, &wBL, &wBR, &dA, &dB, &monoL, &monoR })
                v->assign ((size_t) maxBlock, 0.0f);
            outGain.reset (sampleRate, 0.03);
            inGain.reset (sampleRate, 0.03);
            first = true;
        }

        void reset() { a.reset(); b.reset(); first = true; }

        void process (float* left, float* right, int n,
                      const StripSettings& sa, const StripSettings& sb, const GlobalSettings& g)
        {
            const float og = dbToGain (g.outputDb);
            const float ig = dbToGain (g.inputDb);
            if (first) { outGain.setCurrentAndTarget (og); inGain.setCurrentAndTarget (ig); first = false; }
            outGain.setTarget (og);
            inGain.setTarget (ig);

            for (int i = 0; i < n; ++i)
            {
                const float gi = inGain.next();
                left[i]  *= gi;
                right[i] *= gi;
            }

            switch (g.routing)
            {
                case Routing::series:
                {
                    a.process (left, right, wAL.data(), wAR.data(), dA.data(), n, sa, g.freeze, g.trails);
                    for (int i = 0; i < n; ++i)
                    {
                        midL[(size_t) i] = left[i]  * dA[(size_t) i] + wAL[(size_t) i];
                        midR[(size_t) i] = right[i] * dA[(size_t) i] + wAR[(size_t) i];
                    }
                    b.process (midL.data(), midR.data(), wBL.data(), wBR.data(), dB.data(), n, sb, g.freeze, g.trails);
                    for (int i = 0; i < n; ++i)
                    {
                        left[i]  = midL[(size_t) i] * dB[(size_t) i] + wBL[(size_t) i];
                        right[i] = midR[(size_t) i] * dB[(size_t) i] + wBR[(size_t) i];
                    }
                    break;
                }
                case Routing::parallel:
                {
                    a.process (left, right, wAL.data(), wAR.data(), dA.data(), n, sa, g.freeze, g.trails);
                    b.process (left, right, wBL.data(), wBR.data(), dB.data(), n, sb, g.freeze, g.trails);
                    for (int i = 0; i < n; ++i)
                    {
                        // Dry level follows whichever strip is wetter, so it never doubles up.
                        const float d = std::min (dA[(size_t) i], dB[(size_t) i]);
                        left[i]  = left[i]  * d + wAL[(size_t) i] + wBL[(size_t) i];
                        right[i] = right[i] * d + wAR[(size_t) i] + wBR[(size_t) i];
                    }
                    break;
                }
                case Routing::split:
                {
                    // Left input -> strip A -> left output. Right input -> strip B -> right output.
                    std::copy (left,  left  + n, monoL.begin());
                    std::copy (right, right + n, monoR.begin());
                    a.process (monoL.data(), monoL.data(), wAL.data(), wAR.data(), dA.data(), n, sa, g.freeze, g.trails);
                    b.process (monoR.data(), monoR.data(), wBL.data(), wBR.data(), dB.data(), n, sb, g.freeze, g.trails);
                    for (int i = 0; i < n; ++i)
                    {
                        left[i]  = monoL[(size_t) i] * dA[(size_t) i] + 0.5f * (wAL[(size_t) i] + wAR[(size_t) i]);
                        right[i] = monoR[(size_t) i] * dB[(size_t) i] + 0.5f * (wBL[(size_t) i] + wBR[(size_t) i]);
                    }
                    break;
                }
            }

            for (int i = 0; i < n; ++i)
            {
                const float gg = outGain.next();
                left[i]  *= gg;
                right[i] *= gg;
            }
        }

        const Strip& stripA() const { return a; }
        const Strip& stripB() const { return b; }

    private:
        Strip a, b;
        std::vector<float> midL, midR, wAL, wAR, wBL, wBR, dA, dB, monoL, monoR;
        Ramp outGain, inGain;
        bool first = true;
    };
}
