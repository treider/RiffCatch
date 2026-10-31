#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "TrackFx.h"

// Monophonic bass synth with a step sequencer, laid out like DrumMachine: the pattern is written by the message thread
// and read lock-free by the audio thread, anchored to the transport clock (step k sounds at sample ceil(k * stepLength)).
// A step holds one note: a scale degree, a length in steps, and accent / slide flags. Slide = glide into the note
// without retriggering the envelopes. Each bar can be transposed (chord progression).
class BassMachine
{
public:
    static constexpr int maxBars = 8, stepsPerBar = 16, maxStepsPerBar = 20, maxSteps = maxBars * maxStepsPerBar;
    static constexpr int numRows = 11, degreeBase = -2;          // the grid shows degrees degreeBase .. degreeBase + numRows - 1
    static constexpr int rootMin = 28, rootMax = 39;             // E1 .. D#2
    static constexpr int numScales = 7, maxLen = 160;     // a drone can last the whole 8-bar pattern

    // ---- one step cell: bits 0-7 degree + 64 (0 = rest), 8-15 length, 16 slide, 17 accent
    static uint32_t pack (int degree, int len, bool slide, bool accent)
    {
        return (uint32_t) (juce::jlimit (-63, 63, degree) + 64) | ((uint32_t) juce::jlimit (1, maxLen, len) << 8)
               | (slide ? 0x10000u : 0u) | (accent ? 0x20000u : 0u);
    }
    static bool isNote (uint32_t c) noexcept { return (c & 0xffu) != 0; }
    static int degreeOf (uint32_t c) noexcept { return (int) (c & 0xffu) - 64; }
    static int lenOf (uint32_t c) noexcept { return juce::jmax (1, (int) ((c >> 8) & 0xffu)); }
    static bool slideOf (uint32_t c) noexcept { return (c & 0x10000u) != 0; }
    static bool accentOf (uint32_t c) noexcept { return (c & 0x20000u) != 0; }

    // ---- scales
    static const char* scaleName (int s)
    {
        static const char* const n[numScales] = { "Minor pentatonic", "Major pentatonic", "Minor", "Major", "Blues", "Dorian", "Chromatic" };
        return n[juce::jlimit (0, numScales - 1, s)];
    }
    static int scaleSize (int s)
    {
        static const int n[numScales] = { 5, 5, 7, 7, 6, 7, 12 };
        return n[juce::jlimit (0, numScales - 1, s)];
    }
    static int scaleSemi (int s, int idx)
    {
        static const int t[numScales][12] = { { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 },
                                              { 0, 3, 5, 6, 7, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } };
        return t[juce::jlimit (0, numScales - 1, s)][juce::jlimit (0, 11, idx)];
    }
    // semitones above the root for a scale degree (negative degrees go below the root)
    static int semitones (int scale, int degree)
    {
        const int n = scaleSize (scale);
        int oct = degree / n, idx = degree % n;
        if (idx < 0) { idx += n; --oct; }
        return oct * 12 + scaleSemi (scale, idx);
    }
    // the scale degree closest to a number of semitones above the root
    static int nearestDegree (int scale, int semis)
    {
        const int n = scaleSize (scale);
        int best = 0, bestD = 1000;
        for (int d = -n; d <= 2 * n; ++d)
        {
            const int dist = std::abs (semitones (scale, d) - semis);
            if (dist < bestD) { bestD = dist; best = d; }
        }
        return best;
    }
    static bool hasSemitone (int scale, int semis)
    {
        for (int i = 0; i < scaleSize (scale); ++i) if (scaleSemi (scale, i) == semis) return true;
        return false;
    }
    static const char* noteName (int midi)
    {
        static const char* const n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n[((midi % 12) + 12) % 12];
    }

    // ---- sounds
    struct Preset { const char* name; int wave; float sub, cutoff, reso, env, decay, sustain, glide, drive; };
    static constexpr int numPresets = 5;
    static const Preset& preset (int i)
    {
        static const Preset p[numPresets] = {
            { "Sub",      2, 0.60f,  420.0f, 0.10f, 0.10f, 0.40f, 0.90f,  0.0f, 0.10f },
            { "Pluck",    0, 0.30f,  260.0f, 0.25f, 0.60f, 0.18f, 0.00f,  0.0f, 0.20f },
            { "Fat saw",  0, 0.50f,  650.0f, 0.20f, 0.30f, 0.50f, 0.80f, 20.0f, 0.40f },
            { "Acid",     0, 0.00f,  300.0f, 0.70f, 0.75f, 0.25f, 0.30f, 60.0f, 0.50f },
            { "Square",   1, 0.30f,  900.0f, 0.15f, 0.35f, 0.30f, 0.70f,  0.0f, 0.25f } };
        return p[juce::jlimit (0, numPresets - 1, i)];
    }
    static const char* waveName (int w) { static const char* const n[3] = { "Saw", "Square", "Triangle" }; return n[juce::jlimit (0, 2, w)]; }

    // A complete copy of the pattern (notes, transposition, key), used for undo.
    struct Snapshot
    {
        uint32_t steps[maxSteps];
        int shift[maxBars];
        int bars = 1, barSteps = stepsPerBar, root = 33, scale = 0;
        Snapshot() { std::memset (steps, 0, sizeof (steps)); std::memset (shift, 0, sizeof (shift)); }
        bool operator== (const Snapshot& o) const
        {
            return bars == o.bars && barSteps == o.barSteps && root == o.root && scale == o.scale
                   && std::memcmp (steps, o.steps, sizeof (steps)) == 0 && std::memcmp (shift, o.shift, sizeof (shift)) == 0;
        }
        bool operator!= (const Snapshot& o) const { return ! (*this == o); }
    };

    // ---- pattern + mixer (message thread writes, audio thread reads)
    std::atomic<uint32_t> steps[maxSteps];
    std::atomic<int> shift[maxBars];                      // per-bar transposition in semitones
    std::atomic<int> bars { 1 }, barSteps { stepsPerBar }, root { 33 }, scale { 0 };
    std::atomic<float> volume { 0.8f };
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<bool> echoOn { false }, reverbOn { false };   // insert effects (same as the tracks)
    std::atomic<int> echoDiv { TrackFx::Quarter };
    TrackFx fx;                                           // call fx.prepare() from the message/device thread
    std::atomic<int> auditionNote { -1 };                 // one-shot preview (MIDI note), -1 = none
    // synth
    std::atomic<int> wave { 0 };
    std::atomic<float> sub { 0.5f }, cutoff { 400.0f }, reso { 0.2f }, envAmt { 0.4f }, decay { 0.3f }, sustain { 0.7f }, glide { 0.0f }, drive { 0.2f };

    BassMachine()
    {
        for (auto& s : steps) s.store (0);
        for (auto& s : shift) s.store (0);
        applyPreset (2);
    }

    void applyPreset (int i)
    {
        const auto& p = preset (i);
        wave = p.wave; sub = p.sub; cutoff = p.cutoff; reso = p.reso; envAmt = p.env; decay = p.decay; sustain = p.sustain; glide = p.glide; drive = p.drive;
    }

    int patternSteps() const noexcept { return juce::jlimit (1, maxBars, bars.load()) * juce::jlimit (4, maxStepsPerBar, barSteps.load()); }

    void setBarSteps (int n)
    {
        n = juce::jlimit (4, maxStepsPerBar, n);
        if (n == barSteps.load()) return;
        auto s = snapshot();
        barSteps = n;
        apply (s);
    }

    bool hasAnySteps() const
    {
        const int n = patternSteps();
        for (int k = 0; k < n; ++k) if (isNote (steps[k].load())) return true;
        return false;
    }

    Snapshot snapshot() const
    {
        Snapshot s;
        s.bars = bars.load(); s.barSteps = barSteps.load(); s.root = root.load(); s.scale = scale.load();
        for (int k = 0; k < maxSteps; ++k) s.steps[k] = steps[k].load();
        for (int b = 0; b < maxBars; ++b) s.shift[b] = shift[b].load();
        return s;
    }

    void apply (const Snapshot& s)
    {
        bars = juce::jlimit (1, maxBars, s.bars);
        root = juce::jlimit (rootMin, rootMax, s.root);
        scale = juce::jlimit (0, numScales - 1, s.scale);
        for (int b = 0; b < maxBars; ++b) shift[b] = juce::jlimit (-24, 24, s.shift[b]);
        const int nb = barSteps.load(), sb = juce::jlimit (4, maxStepsPerBar, s.barSteps);
        if (sb == nb) { for (int k = 0; k < maxSteps; ++k) steps[k] = s.steps[k]; return; }
        for (int bar = 0; bar < maxBars; ++bar)           // different bar layout: keep what fits, per bar
            for (int k = 0; k < nb; ++k)
                steps[bar * nb + k] = k < sb ? s.steps[bar * sb + k] : 0u;
    }

    void clearPattern()
    {
        auto s = snapshot();
        std::memset (s.steps, 0, sizeof (s.steps));
        apply (s);
    }

    void resetAll()
    {
        Snapshot s;
        s.barSteps = barSteps.load();
        apply (s);
        volume = 0.8f; mute = false; solo = false;
        echoOn = false; reverbOn = false; echoDiv = TrackFx::Quarter;
        applyPreset (2);
    }

    void copyFrom (const BassMachine& o)
    {
        apply (o.snapshot());
        volume = o.volume.load(); mute = o.mute.load(); solo = o.solo.load();
        echoOn = o.echoOn.load(); reverbOn = o.reverbOn.load(); echoDiv = o.echoDiv.load();
        wave = o.wave.load(); sub = o.sub.load(); cutoff = o.cutoff.load(); reso = o.reso.load(); envAmt = o.envAmt.load();
        decay = o.decay.load(); sustain = o.sustain.load(); glide = o.glide.load(); drive = o.drive.load();
    }

    int midiFor (uint32_t cell, int bar) const
    {
        return root.load() + semitones (scale.load(), degreeOf (cell)) + shift[juce::jlimit (0, maxBars - 1, bar)].load();
    }

    // ---- audio thread
    void prepare (double sampleRate)
    {
        sr = juce::jmax (8000.0, sampleRate);
        reset();
    }

    void reset() { v = Voice(); }

    // process() plus the insert effects (echo / reverb); tails keep ringing after the notes. n must be <= fxBlock.
    static constexpr int fxBlock = 4096;
    void render (juce::int64 absStart, int n, double stepLen, float gain, float* L, float* R, bool sequencer, double samplesPerBeat)
    {
        const bool fxOn = echoOn.load() || reverbOn.load();
        if ((! fxOn && ! fx.wasActive()) || ! fx.ready() || n > fxBlock) { process (absStart, n, stepLen, gain, L, R, sequencer); return; }
        juce::FloatVectorOperations::clear (fxL, n);
        juce::FloatVectorOperations::clear (fxR, n);
        process (absStart, n, stepLen, gain, fxL, fxR, sequencer);
        fx.process (fxL, fxR, n, echoOn.load(), echoDiv.load(), reverbOn.load(), samplesPerBeat);
        juce::FloatVectorOperations::add (L, fxL, n);
        juce::FloatVectorOperations::add (R, fxR, n);
    }

    // Adds n samples of bass to L/R. `absStart` is the absolute transport sample (loop passes counted),
    // `stepLen` the length of one 1/16 step in samples. With sequencer = false only auditions / ringing notes render.
    void process (juce::int64 absStart, int n, double stepLen, float gain, float* L, float* R, bool sequencer)
    {
        struct Trig { int off; int midi; bool accent, slide; juce::int64 relAbs; };
        Trig trig[32];
        int nTrig = 0;

        if (sequencer && stepLen > 1.0)
        {
            const int ps = patternSteps(), bs = juce::jlimit (4, maxStepsPerBar, barSteps.load());
            juce::int64 k = (juce::int64) std::floor ((double) absStart / stepLen);
            while (k > 0 && (juce::int64) std::ceil ((double) (k - 1) * stepLen) >= absStart) --k;
            while ((juce::int64) std::ceil ((double) k * stepLen) < absStart) ++k;
            for (;; ++k)
            {
                const juce::int64 startAbs = (juce::int64) std::ceil ((double) k * stepLen);
                const juce::int64 off = startAbs - absStart;
                if (off >= n) break;
                const int s = (int) (k % ps);
                const uint32_t c = steps[s].load();
                if (! isNote (c) || nTrig >= 31) continue;
                const int len = lenOf (c);
                const uint32_t nxt = steps[(s + len) % ps].load();
                const double gap = (isNote (nxt) && slideOf (nxt)) ? 0.0 : 0.15;     // short gap before the next step; a slide into the next note keeps the gate open
                const juce::int64 hold = juce::jmax<juce::int64> (64, (juce::int64) std::llround ((double) len * stepLen - gap * stepLen));
                trig[nTrig++] = { (int) off, midiFor (c, s / bs), accentOf (c), slideOf (c), startAbs + hold };
            }
        }
        const int aud = auditionNote.exchange (-1);
        if (aud >= 0 && nTrig < 32) trig[nTrig++] = { 0, aud, true, false, -1 };

        if (nTrig == 0 && ! v.active) return;

        const float dRaw = decay.load();
        const float susLevel = juce::jlimit (0.0f, 1.0f, sustain.load());
        const float tc = juce::jmax (1.0f, dRaw * (float) sr * 0.33f);
        const float dCoef = 1.0f - std::exp (-1.0f / tc);
        const float fCoef = std::exp (-1.0f / tc);
        const float relCoef = std::exp (-1.0f / (0.02f * (float) sr));
        const float atkStep = 1.0f / juce::jmax (1.0f, 0.0025f * (float) sr);
        const float gl = glide.load();
        const float glCoef = gl <= 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (gl * 0.001f * (float) sr * 0.5f));
        const float cut = juce::jlimit (30.0f, 12000.0f, cutoff.load());
        const float res = juce::jlimit (0.0f, 0.97f, reso.load());
        const float k1 = 1.8f * (1.0f - res) + 0.08f;
        const float envA = envAmt.load() * 4.5f;
        const float subLvl = sub.load();
        const float drv = 1.0f + drive.load() * 5.0f;
        const float drvNorm = 1.0f / std::tanh (drv);
        const int wv = wave.load();
        float pk = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            for (int ti = 0; ti < nTrig; ++ti)
                if (trig[ti].off == i) noteOn (trig[ti].midi, trig[ti].accent, trig[ti].slide, trig[ti].relAbs, trig[ti].relAbs < 0 ? (int) (sr * 0.35) : -1);
            if (! v.active) continue;

            if (v.gate)
            {
                if (v.relCount >= 0) { if (--v.relCount <= 0) { v.gate = false; v.stage = 2; } }
                else if (v.relAbs >= 0 && absStart + i >= v.relAbs) { v.gate = false; v.stage = 2; }
            }

            v.freq += (v.targetFreq - v.freq) * glCoef;
            const float inc = juce::jmin (0.45f, v.freq / (float) sr);
            v.phase += inc; if (v.phase >= 1.0f) v.phase -= 1.0f;
            v.subPhase += inc * 0.5f; if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;

            float o;
            if (wv == 0)      o = 2.0f * v.phase - 1.0f - blep (v.phase, inc);
            else if (wv == 1) { float p2 = v.phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f; o = (v.phase < 0.5f ? 1.0f : -1.0f) + blep (v.phase, inc) - blep (p2, inc); }
            else              o = 4.0f * std::abs (v.phase - 0.5f) - 1.0f;
            float x = o * 0.7f + std::sin (v.subPhase * 6.2831853f) * subLvl;

            // amp envelope
            if (v.stage == 0) { v.amp += atkStep; if (v.amp >= 1.0f) { v.amp = 1.0f; v.stage = 1; } }
            else if (v.stage == 1) v.amp += (susLevel - v.amp) * dCoef;
            else v.amp *= relCoef;
            if (v.stage != 0 && v.amp < 0.0003f && (v.stage == 2 || susLevel < 0.001f)) { v.active = false; continue; }
            v.fenv *= fCoef;

            // two state variable low-pass filters in series (24 dB), resonance on the first
            const float fc = juce::jlimit (25.0f, (float) sr * 0.42f, cut * std::pow (2.0f, envA * v.fenv * (v.accent ? 1.3f : 1.0f)));
            const float g = std::tan (3.14159265f * fc / (float) sr);
            x = svf (0, x, g, k1);
            x = svf (1, x, g, 1.4f);
            x = std::tanh (x * drv) * drvNorm;

            const float y = x * v.amp * (v.accent ? 0.62f : 0.5f) * gain;
            L[i] += y; R[i] += y;
            pk = juce::jmax (pk, std::abs (y));
        }
        if (pk > peakL.load()) peakL = pk;
        if (pk > peakR.load()) peakR = pk;
    }

private:
    struct Voice
    {
        bool active = false, gate = false, accent = false;
        int stage = 2, relCount = -1;
        juce::int64 relAbs = -1;
        float phase = 0.0f, subPhase = 0.0f, freq = 55.0f, targetFreq = 55.0f, amp = 0.0f, fenv = 0.0f;
        float ic1[2] { 0.0f, 0.0f }, ic2[2] { 0.0f, 0.0f };
    };
    Voice v;
    double sr = 44100.0;
    float fxL[fxBlock] {}, fxR[fxBlock] {};

    static float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float svf (int i, float in, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = in - v.ic2[i];
        const float v1 = a1 * v.ic1[i] + a2 * v3;
        const float v2 = v.ic2[i] + a2 * v.ic1[i] + a3 * v3;
        v.ic1[i] = 2.0f * v1 - v.ic1[i];
        v.ic2[i] = 2.0f * v2 - v.ic2[i];
        return v2;
    }

    void noteOn (int midi, bool accent, bool slide, juce::int64 relAbs, int relCount)
    {
        const float f = 440.0f * std::pow (2.0f, ((float) midi - 69.0f) / 12.0f);
        const bool legato = slide && v.active && v.gate;
        v.targetFreq = f;
        v.accent = accent;
        v.relAbs = relAbs; v.relCount = relCount;
        if (legato) return;                               // glide only: envelopes keep running
        if (! v.active) { v.freq = f; v.phase = 0.0f; v.subPhase = 0.0f; v.amp = 0.0f; v.ic1[0] = v.ic1[1] = v.ic2[0] = v.ic2[1] = 0.0f; }
        v.active = true; v.gate = true; v.stage = 0; v.fenv = 1.0f;
        if (glide.load() <= 0.5f) v.freq = f;
    }
};
