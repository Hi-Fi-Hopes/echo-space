#pragma once

#include "DspCommon.h"
#include "Settings.h"
#include <array>

namespace es
{
    /*
        8-line feedback delay network (FDN) reverb with three voicings.

          in --> pre-delay --> 4 input allpasses (diffusion) --> FDN (8 delay lines,
                     |                                            Householder mixing,
                     '--> early reflections (Room only)           in-loop allpass,
                                                                  damping, modulation)

        Feedback knob = decay time (0.2 s .. 20 s, RT60)
        Tone          = damping (dark .. bright)
        Time knob     = pre-delay (time / 8, so up to 250 ms)

        ROOM   Control 1 = Size        Control 2 = Early reflections level
        DOME   Control 1 = Size        Control 2 = Modulation depth
        HALL   Control 1 = Size        Control 2 = Modulation depth
        PLATE  Control 1 = Diffusion   Control 2 = Modulation depth
    */
    class ReverbEngine
    {
    public:
        static constexpr int N = 8;

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;

            preL.allocate ((int) (sr * 0.32f));
            preR.allocate ((int) (sr * 0.32f));

            static constexpr float diffMsL[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
            static constexpr float diffMsR[4] = { 4.53f, 3.81f, 12.27f, 9.79f };
            for (int i = 0; i < 4; ++i)
            {
                diffL[i].allocate ((int) (diffMsL[i] * 0.001f * sr));
                diffR[i].allocate ((int) (diffMsR[i] * 0.001f * sr));
            }

            static constexpr float loopApMs[N] = { 2.31f, 3.07f, 2.83f, 3.59f, 2.53f, 3.31f, 2.69f, 3.83f };
            for (int i = 0; i < N; ++i)
            {
                lines[i].allocate ((int) (0.230f * sr));   // longest line (dome, max size) + mod headroom
                loopAp[i].allocate ((int) (loopApMs[i] * 0.001f * sr));
            }

            preDelay.reset (newSampleRate, 0.2);
            sizeFactor.reset (newSampleRate, 0.25);
            freezeAmt.reset (newSampleRate, 0.05);
            inputGain.reset (newSampleRate, 0.01);
            reset();
        }

        void reset()
        {
            preL.clear(); preR.clear();
            for (auto& a : diffL) a.clear();
            for (auto& a : diffR) a.clear();
            for (int i = 0; i < N; ++i) { lines[i].clear(); loopAp[i].clear(); damp[i].clear(); state[i] = 0.0f; }
            for (int i = 0; i < N; ++i) lfoPhase[i] = (float) i / (float) N * twoPi;
            first = true;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const StripSettings& s, bool freeze, float inputLevel)
        {
            const EngineType type = s.engine;

            // ---- voicing ---------------------------------------------------
            static constexpr float roomMs[N]  = { 10.1f, 12.3f, 14.9f, 16.7f, 19.3f, 21.1f, 23.9f, 26.7f };
            static constexpr float hallMs[N]  = { 33.1f, 39.7f, 45.1f, 51.3f, 57.7f, 63.1f, 70.9f, 77.3f };
            static constexpr float plateMs[N] = { 8.9f, 11.3f, 13.7f, 15.1f, 17.9f, 20.3f, 23.3f, 25.9f };
            static constexpr float domeMs[N]  = { 47.3f, 55.1f, 63.7f, 71.9f, 81.1f, 89.3f, 97.7f, 107.9f };

            const float* baseMs = type == EngineType::room ? roomMs
                                : type == EngineType::hall ? hallMs
                                : type == EngineType::dome ? domeMs : plateMs;

            float size = 1.0f, inDiff = 0.6f, loopDiff = 0.35f, modDepthSec = 0.00008f, erLevel = 0.0f;
            switch (type)
            {
                case EngineType::room:
                    size = 0.5f + 1.0f * s.control1;
                    inDiff = 0.6f; loopDiff = 0.3f;
                    erLevel = s.control2;
                    break;
                case EngineType::hall:
                    size = 0.6f + 0.9f * s.control1;
                    inDiff = 0.65f; loopDiff = 0.4f;
                    modDepthSec += 0.0009f * s.control2;
                    break;
                case EngineType::dome:
                    // Huge, dense and slowly moving: long lines, heavy diffusion, deep modulation.
                    size = 0.8f + 0.8f * s.control1;
                    inDiff = 0.72f; loopDiff = 0.5f;
                    modDepthSec += 0.0016f * s.control2;
                    break;
                default: // plate
                    size = 1.0f;
                    inDiff = 0.45f + 0.3f * s.control1;
                    loopDiff = 0.25f + 0.4f * s.control1;
                    modDepthSec += 0.0006f * s.control2;
                    break;
            }

            const float preSamples = std::min (250.0f, std::max (0.0f, s.timeMs / preDelayDivisor)) * 0.001f * sr;
            if (first)
            {
                preDelay.setCurrentAndTarget (preSamples);
                sizeFactor.setCurrentAndTarget (size);
                freezeAmt.setCurrentAndTarget (freeze ? 1.0f : 0.0f);
                inputGain.setCurrentAndTarget (inputLevel);
                lastType = type;
                first = false;
            }
            preDelay.setTarget (preSamples);
            sizeFactor.setTarget (size);
            freezeAmt.setTarget (freeze ? 1.0f : 0.0f);
            inputGain.setTarget (inputLevel);

            // Decay: feedback 0..1 maps to RT60 0.2 .. 20 s.
            const float rt60 = 0.2f * std::pow (100.0f, clamp01 (s.feedback));

            // Per-line loop gain for the current (block-start) size.
            const float sizeNow = sizeFactor.getCurrent();
            std::array<float, N> gain {};
            for (int i = 0; i < N; ++i)
            {
                const float lenSec = baseMs[i] * 0.001f * sizeNow + loopApSeconds (i);
                gain[i] = std::pow (10.0f, -3.0f * lenSec / rt60);
            }

            const float dampHz = expMap (s.tone, 900.0f, 20000.0f);
            for (auto& d : damp) d.setCutoff (dampHz, sr);

            const float modDepth = modDepthSec * sr;
            const float lfoInc   = twoPi * std::max (0.05f, s.modRateHz) / sr;
            static constexpr float lfoSpread[N] = { 0.71f, 0.83f, 0.97f, 1.07f, 0.89f, 1.19f, 1.31f, 0.77f };

            static constexpr float inSign[N]  = { 1, 1, -1, -1, 1, -1, 1, -1 };
            static constexpr float outLsg[N]  = { 1, -1, 1, -1, 1, 1, -1, -1 };
            static constexpr float outRsg[N]  = { 1, 1, -1, -1, -1, 1, 1, -1 };

            static constexpr float erMs[6]    = { 5.3f, 8.9f, 13.1f, 17.7f, 23.3f, 29.9f };
            static constexpr float erGain[6]  = { 0.80f, 0.66f, 0.55f, 0.45f, 0.36f, 0.28f };

            // Output level: 1/sqrt(N), a trim so 100% wet sits near the dry level, and a
            // small boost for longer line sets (fewer reflections per second = less energy).
            float meanMs = 0.0f;
            for (int k = 0; k < N; ++k) meanMs += baseMs[k] / (float) N;
            const float outScale = 1.07f / 2.828427f * std::pow (meanMs * size / 18.0f, 0.25f);

            for (int i = 0; i < n; ++i)
            {
                const float fz  = freezeAmt.next();
                const float ing = inputGain.next() * (1.0f - fz);
                const float pre = preDelay.next();
                const float sz  = sizeFactor.next();

                // Pre-delay
                preL.push (inL[i] * ing);
                preR.push (inR[i] * ing);
                const float pl = pre < 1.0f ? preL.at (1) : preL.readLinear (pre);
                const float pr = pre < 1.0f ? preR.at (1) : preR.readLinear (pre);

                // Input diffusion
                float xl = pl, xr = pr;
                for (int k = 0; k < 4; ++k)
                {
                    xl = diffL[k].process (xl, inDiff);
                    xr = diffR[k].process (xr, inDiff);
                }

                // Read lines (modulated), damp, in-loop diffusion
                std::array<float, N> v {};
                float sum = 0.0f;
                for (int k = 0; k < N; ++k)
                {
                    lfoPhase[k] += lfoInc * lfoSpread[k];
                    if (lfoPhase[k] > twoPi) lfoPhase[k] -= twoPi;

                    const float len = baseMs[k] * 0.001f * sr * sz + modDepth * (1.0f + std::sin (lfoPhase[k]));
                    float y = lines[k].readLinear (len);
                    y = lerp (damp[k].lowpass (y), y, fz);
                    y = loopAp[k].process (y, loopDiff);
                    v[k] = y;
                    sum += y;
                }

                // Outputs tap the line outputs
                float wl = 0.0f, wr = 0.0f;
                for (int k = 0; k < N; ++k)
                {
                    wl += v[k] * outLsg[k];
                    wr += v[k] * outRsg[k];
                }
                wl *= outScale;
                wr *= outScale;

                // Householder feedback matrix (energy preserving), then decay gain
                const float h = sum * (2.0f / (float) N);
                for (int k = 0; k < N; ++k)
                {
                    const float g = lerp (gain[k], 1.0f, fz);
                    const float inj = ((k & 1) == 0 ? xl : xr) * inSign[k];
                    lines[k].push ((v[k] - h) * g + inj);
                }

                // Early reflections (Room)
                if (erLevel > 0.001f)
                {
                    float el = 0.0f, er = 0.0f;
                    for (int k = 0; k < 6; ++k)
                    {
                        const float t = pre + erMs[k] * 0.001f * sr * sz;
                        el += preL.readLinear (t + 1.0f) * erGain[k];
                        er += preR.readLinear (t * 1.07f + 1.0f) * erGain[k];
                    }
                    wl += el * erLevel * 0.45f;
                    wr += er * erLevel * 0.45f;
                }

                outL[i] = wl;
                outR[i] = wr;
            }
        }

    private:
        float loopApSeconds (int i) const { return (float) loopAp[i].buf.size() / sr; }

        float sr = 44100.0f;
        DelayBuffer preL, preR;
        std::array<Allpass, 4> diffL, diffR;
        std::array<DelayBuffer, N> lines;
        std::array<Allpass, N> loopAp;
        std::array<OnePole, N> damp;
        std::array<float, N> lfoPhase {}, state {};
        Ramp preDelay, sizeFactor, freezeAmt, inputGain;
        EngineType lastType = EngineType::hall;
        bool first = true;
    };
}
