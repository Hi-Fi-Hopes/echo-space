#pragma once

// Phase 3 engines: Oil Can, Decay and Swell. (Dome is a voicing of ReverbEngine.)

#include "DspCommon.h"
#include "Settings.h"
#include "EngineTables.h"
#include "ReverbEngine.h"
#include "ExtraEngines.h"     // softLimit
#include <array>

namespace es
{
    //==========================================================================
    /*
        OIL CAN: modelled loosely on the old electrostatic "oil can" delays. Echoes were
        stored on a spinning disc in oil and read by several brushes, so the repeats are
        dark, smeared, gritty and wobble a lot.

        Three read heads around the "disc", each wobbling with its own phase, then a short
        allpass smear, a dark band-pass and saturation in the loop.

        Control 1 = Wobble (depth)   Control 2 = Grit   Speed = wobble rate
    */
    class OilCanEngine
    {
    public:
        static constexpr float maxDelayMs = 4000.0f;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            const int maxSamples = (int) std::ceil ((maxDelayMs * 1.03f + 40.0f) * 0.001f * sr);
            bufL.allocate (maxSamples);
            bufR.allocate (maxSamples);
            static constexpr float apMs[2] = { 3.7f, 5.3f };
            smearSamples = 0.0f;
            for (size_t i = 0; i < 2; ++i)
            {
                apL[i].allocate ((int) (apMs[i] * 0.001f * sr));
                apR[i].allocate ((int) (apMs[i] * 0.001f * sr));
                smearSamples += (float) apL[i].buf.size();
            }
            delaySamples.reset (newSampleRate, 0.15);
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
            phase = 0.0f;
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            // The smear allpasses add their own delay; take it off the read point so echoes stay on time.
            const float target = std::clamp (s.timeMs, 1.0f, maxDelayMs) * 0.001f * sr;
            if (first)
            {
                delaySamples.setCurrentAndTarget (target);
                feedback.setCurrentAndTarget (s.feedback);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            delaySamples.setTarget (target);
            feedback.setTarget (s.feedback);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const float toneHz = expMap (s.tone, 600.0f, 7000.0f);        // oil cans are dark
            lpL.setCutoff (toneHz, sr); lpR.setCutoff (toneHz, sr);
            hpL.setCutoff (170.0f, sr); hpR.setCutoff (170.0f, sr);

            const float grit  = s.control2;
            const float drive = 1.0f + 5.0f * grit;
            const float bias  = 0.2f * grit;
            const float off   = std::tanh (bias);
            const float depth = (0.0003f + 0.0045f * s.control1) * sr;      // up to ~5 ms of wobble
            const float inc   = twoPi * std::max (0.05f, s.modRateHz) * 1.6f / sr;

            static constexpr float headPos[3]  = { 1.0f, 0.985f, 1.017f };
            static constexpr float headGain[3] = { 0.5f, 0.25f, 0.25f };
            static constexpr float headPh[3]   = { 0.0f, 2.09f, 4.19f };

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float fb  = lerp (feedback.next(), 1.0f, fz);
                const float ing = inputGain.next() * (1.0f - fz);
                const float d   = std::max (2.0f, delaySamples.next() - smearSamples);

                phase += inc;
                if (phase > twoPi) phase -= twoPi;

                float l = 0.0f, r = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    // While frozen, fold the three heads into one so the held loop doesn't comb-filter away.
                    const float gk = lerp (headGain[k], k == 0 ? 1.0f : 0.0f, fz);
                    if (gk <= 0.0f) continue;
                    const float base = d * headPos[k];
                    l += gk * bufL.readHermite (base + depth * (1.0f + std::sin (phase + headPh[k])));
                    r += gk * bufR.readHermite (base + depth * (1.0f + std::sin (phase + headPh[k] + 0.9f)));
                }

                // Oil smear (a few ms of diffusion, faded out while frozen)
                const float g = 0.55f * (1.0f - fz);
                for (size_t k = 0; k < 2; ++k) { l = apL[k].process (l, g); r = apR[k].process (r, g); }

                const float echoL = lerp (lpL.lowpass (hpL.highpass (l)), l, fz);
                const float echoR = lerp (lpR.lowpass (hpR.highpass (r)), r, fz);

                const float xl = inL[i] * ing + echoL * fb;
                const float xr = inR[i] * ing + echoR * fb;
                bufL.push (lerp ((std::tanh (xl * drive + bias) - off) / drive, softLimit (xl), fz));
                bufR.push (lerp ((std::tanh (xr * drive + bias) - off) / drive, softLimit (xr), fz));

                outL[i] = echoL;
                outR[i] = echoR;
            }
        }

    private:
        float sr = 44100.0f, smearSamples = 0.0f, phase = 0.0f;
        DelayBuffer bufL, bufR;
        std::array<Allpass, 2> apL, apR;
        OnePole lpL, lpR, hpL, hpR;
        Ramp delaySamples, feedback, freezeAmt, inputGain;
        bool first = true;
    };

    //==========================================================================
    /*
        DECAY: a delay whose repeats fall apart. Everything happens inside the feedback
        loop, so each pass is a little more worn than the one before:
        the band narrows, bits drop away, the pitch sags, holes open up and dust crackles.

        Control 1 = Erode    (how fast each repeat wears: bandwidth, bit depth, pitch sag)
        Control 2 = Dropouts (random gaps and crackle)
        Speed     = how fast the pitch sags and wanders
    */
    class DecayEngine
    {
    public:
        static constexpr float maxDelayMs = 4000.0f;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            const int maxSamples = (int) std::ceil ((maxDelayMs + 20.0f) * 0.001f * sr);
            bufL.allocate (maxSamples);
            bufR.allocate (maxSamples);
            delaySamples.reset (newSampleRate, 0.15);
            feedback.reset (newSampleRate, 0.03);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            gateCoef   = 1.0f - std::exp (-1.0f / (0.004f * sr));
            envAttack  = 1.0f - std::exp (-1.0f / (0.002f * sr));
            envRelease = 1.0f - std::exp (-1.0f / (0.150f * sr));
            clickHp.setCutoff (1800.0f, sr);
            reset();
        }

        void reset()
        {
            bufL.clear(); bufR.clear();
            lpL.clear(); lpR.clear(); hpL.clear(); hpR.clear(); clickHp.clear();
            gate = gateTarget = 1.0f;
            gateCounter = 0;
            sagPhase = 0.0f; wander = 0.0f; env = 0.0f;
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            const float target = std::clamp (s.timeMs, 1.0f, maxDelayMs) * 0.001f * sr;
            if (first)
            {
                delaySamples.setCurrentAndTarget (target);
                feedback.setCurrentAndTarget (s.feedback);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            delaySamples.setTarget (target);
            feedback.setTarget (s.feedback);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const float erode = s.control1, drops = s.control2;

            // Per-pass wear. Because it sits in the loop, it compounds with every repeat.
            const float lpHz = std::min (expMap (s.tone, 700.0f, 20000.0f), expMap (1.0f - erode, 1500.0f, 18000.0f));
            lpL.setCutoff (lpHz, sr); lpR.setCutoff (lpHz, sr);
            const float hpHz = expMap (erode, 30.0f, 450.0f);
            hpL.setCutoff (hpHz, sr); hpR.setCutoff (hpHz, sr);
            const float quantum = std::pow (2.0f, 15.0f - 9.0f * erode);    // 16 bits down to 7
            const bool  crush   = erode > 0.02f;

            const float sagDepth = erode * 0.0018f * sr;
            const float sagInc   = twoPi * std::max (0.05f, s.modRateHz) * 0.35f / sr;

            const float dropChance  = 0.45f * drops;                         // chance each segment is a hole
            const float crackleRate = 30.0f * drops / sr;                    // clicks per sample

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float fb  = lerp (feedback.next(), 1.0f, fz);
                const float ing = inputGain.next() * (1.0f - fz);

                sagPhase += sagInc;
                if (sagPhase > twoPi) sagPhase -= twoPi;
                wander += (rng.bipolar() - wander) * 0.0003f;
                const float d = delaySamples.next() + sagDepth * (1.0f + 0.7f * std::sin (sagPhase) + 0.3f * wander);

                const float rawL = bufL.readHermite (d);
                const float rawR = bufR.readHermite (d);

                float eL = lpL.lowpass (hpL.highpass (rawL));
                float eR = lpR.lowpass (hpR.highpass (rawR));
                if (crush)
                {
                    eL = std::round (eL * quantum) / quantum;
                    eR = std::round (eR * quantum) / quantum;
                }

                // Dropouts: the loop is cut into random 20-150 ms segments; some become holes.
                if (--gateCounter <= 0)
                {
                    gateCounter = (int) ((0.02f + 0.13f * (0.5f + 0.5f * rng.bipolar())) * sr);
                    gateTarget = (0.5f + 0.5f * rng.bipolar()) < dropChance ? 0.03f + 0.2f * (0.5f + 0.5f * rng.bipolar()) : 1.0f;
                }
                gate += (gateTarget - gate) * gateCoef;
                const float gg = lerp (gate, 1.0f, fz);

                // Dust: clicks that only appear while something is ringing.
                const float lvl = std::max (std::abs (rawL), std::abs (rawR));
                env += (lvl - env) * (lvl > env ? envAttack : envRelease);
                float click = 0.0f;
                if ((0.5f + 0.5f * rng.bipolar()) < crackleRate)
                    click = (rng.bipolar() > 0.0f ? 1.0f : -1.0f) * (0.05f + 0.15f * (0.5f + 0.5f * rng.bipolar()));
                const float dust = clickHp.highpass (click) * std::min (1.0f, env * 6.0f) * (1.0f - fz);

                const float echoL = lerp (eL, rawL, fz) * gg + dust;
                const float echoR = lerp (eR, rawR, fz) * gg + dust;

                bufL.push (softLimit (inL[i] * ing + echoL * fb));
                bufR.push (softLimit (inR[i] * ing + echoR * fb));

                outL[i] = echoL;
                outR[i] = echoR;
            }
        }

    private:
        float sr = 44100.0f;
        DelayBuffer bufL, bufR;
        OnePole lpL, lpR, hpL, hpR, clickHp;
        Ramp delaySamples, feedback, freezeAmt, inputGain;
        Rng rng;
        float gate = 1.0f, gateTarget = 1.0f, gateCoef = 0.005f;
        int gateCounter = 0;
        float sagPhase = 0.0f, wander = 0.0f, env = 0.0f, envAttack = 0.01f, envRelease = 0.0001f;
        bool first = true;
    };

    //==========================================================================
    /*
        SWELL: every new note fades in like a volume pedal, then rings into a big hall.
        A fast and a slow level follower spot the start of each note; the swell
        restarts from silence (with a 4 ms dip, so it never clicks) and rises over the
        Attack time.

        Control 1 = Attack (10 ms .. 2 s)   Control 2 = Mod   Speed = mod rate
    */
    class SwellEngine
    {
    public:
        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            hall.prepare (newSampleRate);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            fastAtt = coef (0.001f); fastRel = coef (0.040f);
            slowAtt = coef (0.060f); slowRel = coef (0.400f);
            fallStep = 1.0f / (0.004f * sr);
            reset();
        }

        void reset()
        {
            hall.reset();
            fastEnv = slowEnv = 0.0f;
            gain = 0.0f; falling = false; armed = true;
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            if (first)
            {
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                first = false;
            }
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            const float riseStep = 1.0f / (swellAttackSeconds (s.control1) * sr);

            StripSettings h = s;           // big lush hall underneath
            h.engine   = EngineType::hall;
            h.control1 = 0.85f;
            h.control2 = s.control2;

            for (int pos = 0; pos < n; pos += chunk)
            {
                const int m = std::min (chunk, n - pos);
                for (int i = 0; i < m; ++i)
                {
                    const float xl = inL[pos + i], xr = inR[pos + i];
                    const float lvl = std::max (std::abs (xl), std::abs (xr));
                    fastEnv += (lvl - fastEnv) * (lvl > fastEnv ? fastAtt : fastRel);
                    slowEnv += (lvl - slowEnv) * (lvl > slowEnv ? slowAtt : slowRel);

                    if (armed && fastEnv > 2.0f * slowEnv + 0.002f) { falling = true; armed = false; }
                    if (! armed && fastEnv < 1.3f * slowEnv)        { armed = true; }

                    if (falling) { gain -= fallStep; if (gain <= 0.0f) { gain = 0.0f; falling = false; } }
                    else         { gain = std::min (1.0f, gain + riseStep); }

                    const float shaped = gain * gain * (3.0f - 2.0f * gain);   // smooth S-curve
                    sL[(size_t) i] = xl * shaped;
                    sR[(size_t) i] = xr * shaped;

                    const float direct = inputGain.next() * (1.0f - freezeAmt.next());
                    dL[(size_t) i] = sL[(size_t) i] * direct;
                    dR[(size_t) i] = sR[(size_t) i] * direct;
                }

                hall.process (sL.data(), sR.data(), outL + pos, outR + pos, m, h, freeze, inputLevel);

                for (int i = 0; i < m; ++i)
                {
                    outL[pos + i] = outL[pos + i] * 0.7f + dL[(size_t) i] * 0.7f;
                    outR[pos + i] = outR[pos + i] * 0.7f + dR[(size_t) i] * 0.7f;
                }
            }
        }

    private:
        float coef (float seconds) const { return 1.0f - std::exp (-1.0f / (seconds * sr)); }

        static constexpr int chunk = 64;
        float sr = 44100.0f;
        ReverbEngine hall;
        Ramp freezeAmt, inputGain;
        std::array<float, chunk> sL {}, sR {}, dL {}, dR {};
        float fastEnv = 0, slowEnv = 0, fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0;
        float gain = 0.0f, fallStep = 0.01f;
        bool falling = false, armed = true, first = true;
    };
}
