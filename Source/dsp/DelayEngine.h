#pragma once

#include "DspCommon.h"
#include "Settings.h"

namespace es
{
    /*
        Stereo feedback delay with two voicings.

        DIGITAL   Control 1 = Spread (0 = straight stereo, 1 = full ping-pong)
                  Control 2 = Crush  (bit depth + sample-rate reduction on the repeats)

        TAPE      Control 1 = Age    (darker, more saturated repeats)
                  Control 2 = Wow & flutter (pitch wobble; speed follows Mod Rate)

        ANALOG    Bucket-brigade style: repeats get darker as the time gets longer.
                  Control 1 = Mod depth (chorus-like wobble; speed follows Mod Rate)
                  Control 2 = Grit (saturation)

        Loop:  in --> [ + ] --> saturate --> delay --> low cut --> tone (low-pass) --> wet out
                       ^                                                     |
                       '------------------ feedback gain <-------------------'
    */
    class DelayEngine
    {
    public:
        static constexpr float maxDelayMs = 4000.0f;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            const int maxSamples = (int) std::ceil ((maxDelayMs + 30.0f) * 0.001f * sr);
            bufL.allocate (maxSamples);
            bufR.allocate (maxSamples);
            delaySamples.reset (newSampleRate, 0.15);
            feedback.reset (newSampleRate, 0.03);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            reset();
        }

        void reset()
        {
            bufL.clear(); bufR.clear();
            hpL.clear(); hpR.clear(); lpL.clear(); lpR.clear(); lp2L.clear(); lp2R.clear();
            fbL = fbR = 0.0f;
            holdL = holdR = 0.0f; holdCounter = 0.0f;
            wowPhase = flutterPhase = 0.0f; drift = 0.0f;
            first = true;
        }

        // in: dry input (already gated by the strip). out: wet only.
        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            const bool tape    = s.engine == EngineType::tape;
            const bool analog  = s.engine == EngineType::analog;
            const bool digital = ! tape && ! analog;
            const float targetDelay = std::min (maxDelayMs, std::max (1.0f, s.timeMs)) * 0.001f * sr;

            if (first)
            {
                delaySamples.setCurrentAndTarget (targetDelay);
                feedback.setCurrentAndTarget (s.feedback);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            delaySamples.setTarget (targetDelay);
            feedback.setTarget (s.feedback);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            // ---- voicing ---------------------------------------------------
            const float age = tape ? s.control1 : 0.0f;
            float toneHz = expMap (s.tone, 700.0f, 20000.0f);
            if (tape) toneHz = std::min (toneHz, expMap (1.0f - age, 1800.0f, 14000.0f));
            if (analog)
            {
                // Bucket-brigade chips run a slower clock for longer delays, so they get darker.
                const float bbdHz = 9000.0f * std::sqrt (300.0f / std::max (300.0f, s.timeMs));
                toneHz = std::min (toneHz, std::max (2200.0f, bbdHz));
            }
            lpL.setCutoff (toneHz, sr);  lpR.setCutoff (toneHz, sr);
            lp2L.setCutoff (toneHz, sr); lp2R.setCutoff (toneHz, sr);

            const float lowCutHz = tape ? 60.0f + 90.0f * age : analog ? 80.0f : 40.0f;
            hpL.setCutoff (lowCutHz, sr); hpR.setCutoff (lowCutHz, sr);

            const float grit    = analog ? s.control2 : 0.0f;
            const float drive   = tape ? 1.0f + 3.0f * age : analog ? 1.0f + 4.0f * grit : 1.0f;
            const float bias    = 0.15f * grit;                      // asymmetry = even harmonics
            const float spread  = digital ? s.control1 : 0.0f;
            const float crush   = digital ? s.control2 : 0.0f;
            const float wowAmt  = tape ? s.control2 : 0.0f;
            const float chorus  = analog ? s.control1 * 0.0025f * sr : 0.0f;
            const float chorusInc = twoPi * std::max (0.05f, s.modRateHz) / sr;

            // Wow: slow, a few ms. Flutter: fast, a fraction of a ms. Plus random drift.
            const float wowDepth     = wowAmt * 0.0030f * sr;
            const float flutterDepth = wowAmt * 0.00025f * sr;
            const float driftDepth   = wowAmt * 0.0012f * sr;
            const float wowInc       = twoPi * std::max (0.05f, s.modRateHz * 0.6f) / sr;
            const float flutterInc   = twoPi * 6.3f / sr;

            // Crush: 16 -> 4 bits, hold up to 16 samples
            const float bits     = 16.0f - 12.0f * crush;
            const float quantum  = std::pow (2.0f, bits - 1.0f);
            const float holdLen  = 1.0f + 15.0f * crush * crush;

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float fb  = lerp (feedback.next(), 1.0f, fz);
                const float ing = inputGain.next() * (1.0f - fz);

                float d = delaySamples.next();
                if (wowAmt > 0.0f)
                {
                    wowPhase     += wowInc;     if (wowPhase > twoPi) wowPhase -= twoPi;
                    flutterPhase += flutterInc; if (flutterPhase > twoPi) flutterPhase -= twoPi;
                    drift += (rng.bipolar() - drift) * 0.0005f;          // slowly wandering
                    d += wowDepth * (1.0f + std::sin (wowPhase))
                       + flutterDepth * (1.0f + std::sin (flutterPhase))
                       + driftDepth * (1.0f + drift);
                }
                if (chorus > 0.0f)
                {
                    wowPhase += chorusInc; if (wowPhase > twoPi) wowPhase -= twoPi;
                    d += chorus * (1.0f + std::sin (wowPhase));
                }

                // Read the echoes and shape them (shaping fades out while frozen
                // so a frozen loop doesn't decay).
                const float rawL = bufL.readHermite (d);
                const float rawR = bufR.readHermite (d);
                float shapedL = lpL.lowpass (hpL.highpass (rawL));
                float shapedR = lpR.lowpass (hpR.highpass (rawR));
                if (analog) { shapedL = lp2L.lowpass (shapedL); shapedR = lp2R.lowpass (shapedR); }   // 12 dB/oct
                const float echoL = lerp (shapedL, rawL, fz);
                const float echoR = lerp (shapedR, rawR, fz);

                // Ping-pong amount: input moves to the left, feedback crosses sides.
                const float inMono = 0.5f * (inL[i] + inR[i]);
                const float srcL = lerp (inL[i], inMono, spread) * ing;
                const float srcR = lerp (inR[i], 0.0f,   spread) * ing;
                const float loopL = lerp (echoL, echoR, spread) * fb;
                const float loopR = lerp (echoR, echoL, spread) * fb;

                if (analog)
                {
                    const float xl = srcL + loopL, xr = srcR + loopR;
                    const float off = std::tanh (bias);
                    bufL.push (lerp ((std::tanh (xl * drive + bias) - off) / drive, softLimit (xl), fz));
                    bufR.push (lerp ((std::tanh (xr * drive + bias) - off) / drive, softLimit (xr), fz));
                }
                else if (tape)
                {
                    // Tape drive saturates; while frozen it hands over to the clean
                    // limiter so the held loop doesn't sag over time.
                    const float xl = srcL + loopL, xr = srcR + loopR;
                    bufL.push (lerp (std::tanh (xl * drive) / drive, softLimit (xl), fz));
                    bufR.push (lerp (std::tanh (xr * drive) / drive, softLimit (xr), fz));
                }
                else
                {
                    bufL.push (softLimit (srcL + loopL));
                    bufR.push (softLimit (srcR + loopR));
                }

                float wl = echoL, wr = echoR;
                if (crush > 0.001f)
                {
                    holdCounter += 1.0f;
                    if (holdCounter >= holdLen)
                    {
                        holdCounter -= holdLen;
                        holdL = std::round (wl * quantum) / quantum;
                        holdR = std::round (wr * quantum) / quantum;
                    }
                    wl = holdL; wr = holdR;
                }
                outL[i] = wl;
                outR[i] = wr;
            }
        }

    private:
        // Clean below 0.8, then rounds off smoothly to a ceiling of 1.0 (keeps runaway
        // feedback from exploding without colouring normal levels).
        static float softLimit (float x) noexcept
        {
            const float a = std::abs (x);
            if (a <= 0.8f) return x;
            const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f);
            return x < 0.0f ? -y : y;
        }

        float sr = 44100.0f;
        DelayBuffer bufL, bufR;
        OnePole hpL, hpR, lpL, lpR, lp2L, lp2R;
        Ramp delaySamples, feedback, freezeAmt, inputGain;
        Rng rng;
        float fbL = 0, fbR = 0, holdL = 0, holdR = 0, holdCounter = 0;
        float wowPhase = 0, flutterPhase = 0, drift = 0;
        bool first = true;
    };
}
