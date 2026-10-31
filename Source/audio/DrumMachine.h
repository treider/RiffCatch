#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

// Step-sequenced drum machine with six synthesised voices (no samples).
// The pattern (1-8 bars of 16 steps) is written by the message thread and read lock-free by the audio thread.
// It is anchored to the transport clock: step k sounds at sample ceil(k * stepLength), the pattern repeats
// every `bars` bars and keeps its phase across loop passes (a 6-bar pattern over a 4-bar loop works).
class DrumMachine
{
public:
    static constexpr int numVoices = 6, maxBars = 8, stepsPerBar = 16, maxSteps = maxBars * stepsPerBar;
    enum Voice { Kick = 0, Snare, Clap, Hat, OpenHat, Cymbal };
    static const char* voiceName (int v)
    {
        static const char* const n[numVoices] = { "Kick", "Snare", "Clap", "Hat", "Open hat", "Cymbal" };
        return n[v];
    }

    // A complete copy of the pattern, used for undo.
    struct Snapshot
    {
        uint8_t steps[numVoices][maxSteps];
        int bars = 1;
        Snapshot() { std::memset (steps, 0, sizeof (steps)); }
        bool operator== (const Snapshot& o) const { return bars == o.bars && std::memcmp (steps, o.steps, sizeof (steps)) == 0; }
        bool operator!= (const Snapshot& o) const { return ! (*this == o); }
    };

    // ---- pattern + mixer (message thread writes, audio thread reads)
    std::atomic<uint8_t> steps[numVoices][maxSteps];   // 0 = off, 1 = hit, 2 = accent
    std::atomic<int> bars { 1 };
    std::atomic<float> volume { 0.8f };
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<float> level[numVoices];
    std::atomic<float> pan[numVoices];                 // -1..1 per voice
    std::atomic<float> cymbalPitch { 0.0f };           // semitones, -12..12
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f }; // output peak since last read (VU)
    std::atomic<bool> voiceMute[numVoices];
    std::atomic<bool> audition[numVoices];             // one-shot preview request

    DrumMachine()
    {
        for (auto& row : steps) for (auto& s : row) s.store (0);
        for (int v = 0; v < numVoices; ++v) { level[v] = 1.0f; pan[v] = 0.0f; voiceMute[v] = false; audition[v] = false; }
    }

    int patternSteps() const noexcept { return juce::jlimit (1, maxBars, bars.load()) * stepsPerBar; }

    bool hasAnySteps() const
    {
        const int n = patternSteps();
        for (int v = 0; v < numVoices; ++v)
            for (int k = 0; k < n; ++k)
                if (steps[v][k].load() != 0) return true;
        return false;
    }

    Snapshot snapshot() const
    {
        Snapshot s;
        s.bars = bars.load();
        for (int v = 0; v < numVoices; ++v)
            for (int k = 0; k < maxSteps; ++k) s.steps[v][k] = steps[v][k].load();
        return s;
    }

    void apply (const Snapshot& s)
    {
        bars = juce::jlimit (1, maxBars, s.bars);
        for (int v = 0; v < numVoices; ++v)
            for (int k = 0; k < maxSteps; ++k) steps[v][k] = s.steps[v][k];
    }

    void clearPattern() { Snapshot s; s.bars = bars.load(); apply (s); }

    void resetAll()
    {
        Snapshot s;
        apply (s);
        volume = 0.8f; mute = false; solo = false;
        cymbalPitch = 0.0f;
        for (int v = 0; v < numVoices; ++v) { level[v] = 1.0f; pan[v] = 0.0f; voiceMute[v] = false; }
    }

    void copyFrom (const DrumMachine& o)
    {
        apply (o.snapshot());
        volume = o.volume.load(); mute = o.mute.load(); solo = o.solo.load();
        cymbalPitch = o.cymbalPitch.load();
        for (int v = 0; v < numVoices; ++v) { level[v] = o.level[v].load(); pan[v] = o.pan[v].load(); voiceMute[v] = o.voiceMute[v].load(); }
    }

    // ---- audio thread
    void prepare (double sampleRate)
    {
        sr = juce::jmax (8000.0, sampleRate);
        dt = (float) (1.0 / sr);
        auto hpCoef = [this] (double fc) { const double rc = 1.0 / (juce::MathConstants<double>::twoPi * fc); return (float) (rc / (rc + 1.0 / sr)); };
        hpSnare = hpCoef (1800.0); hpClap = hpCoef (900.0); hpHat = hpCoef (7000.0); hpCym = hpCoef (5000.0);
        reset();
    }

    void reset() { for (auto& v : voices) v.active = false; }

    // Adds n samples of drums to L/R. `absStart` is the absolute sample position (loop passes counted),
    // `stepLen` the length of one 1/16 step in samples. With sequencer = false only auditions/ringing voices render.
    void process (juce::int64 absStart, int n, double stepLen, float gain, float* L, float* R, bool sequencer)
    {
        struct Trig { int off, voice; int vel; };
        Trig trig[96];
        int nTrig = 0;

        if (sequencer && stepLen > 1.0)
        {
            const int ps = patternSteps();
            juce::int64 k = (juce::int64) std::floor ((double) absStart / stepLen);
            while (k > 0 && (juce::int64) std::ceil ((double) (k - 1) * stepLen) >= absStart) --k;
            while ((juce::int64) std::ceil ((double) k * stepLen) < absStart) ++k;
            for (;; ++k)
            {
                const juce::int64 off = (juce::int64) std::ceil ((double) k * stepLen) - absStart;
                if (off >= n) break;
                const int s = (int) (k % ps);
                for (int v = 0; v < numVoices; ++v)
                {
                    const int val = steps[v][s].load();
                    if (val != 0 && ! voiceMute[v].load() && nTrig < 96) trig[nTrig++] = { (int) off, v, val };
                }
            }
        }
        for (int v = 0; v < numVoices; ++v)
            if (audition[v].exchange (false) && nTrig < 96) trig[nTrig++] = { 0, v, 2 };

        bool anyActive = false;
        for (auto& v : voices) anyActive |= v.active;
        if (nTrig == 0 && ! anyActive) { curGain = gain; return; }

        cymAmt = std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, cymbalPitch.load()) / 12.0f) - 1.0f;
        float vl[numVoices], vr[numVoices];            // equal-power pan, unity at the centre
        for (int v = 0; v < numVoices; ++v)
        {
            const float th = (juce::jlimit (-1.0f, 1.0f, pan[v].load()) + 1.0f) * 0.7853982f;
            vl[v] = std::cos (th) * 1.41421356f * level[v].load();
            vr[v] = std::sin (th) * 1.41421356f * level[v].load();
        }
        float pkL = 0.0f, pkR = 0.0f;
        float g = curGain;
        const float gs = (gain - g) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            g += gs;
            for (int ti = 0; ti < nTrig; ++ti)
                if (trig[ti].off == i) fire (trig[ti].voice, trig[ti].vel);
            float sl = 0.0f, sr2 = 0.0f;
            for (int v = 0; v < numVoices; ++v)
                if (voices[v].active) { const float x = tick (v); sl += x * vl[v]; sr2 += x * vr[v]; }
            sl *= g; sr2 *= g;
            L[i] += sl; R[i] += sr2;
            pkL = juce::jmax (pkL, std::abs (sl)); pkR = juce::jmax (pkR, std::abs (sr2));
        }
        if (pkL > peakL.load()) peakL = pkL;
        if (pkR > peakR.load()) peakR = pkR;
        curGain = gain;
    }

private:
    struct VoiceState
    {
        bool active = false, choked = false;
        float t = 0.0f, chokeT = 0.0f, gain = 1.0f;
        float phase = 0.0f, phase2 = 0.0f;
        float hpX = 0.0f, hpY = 0.0f, lpY = 0.0f;
        float osc[6] {};
        uint32_t rng = 0x9e3779b9u;
    };

    static constexpr float twoPi = 6.28318530718f;
    double sr = 44100.0;
    float dt = 1.0f / 44100.0f;
    float hpSnare = 0.8f, hpClap = 0.8f, hpHat = 0.8f, hpCym = 0.8f;
    float curGain = 0.0f, cymAmt = 0.0f;
    VoiceState voices[numVoices];

    void fire (int v, int vel)
    {
        auto& s = voices[v];
        const auto rng = s.rng * 1664525u + 1013904223u;     // different noise each hit
        s = VoiceState();
        s.rng = rng | 1u;
        s.active = true;
        s.gain = vel >= 2 ? 1.0f : 0.68f;
        if (v == Hat) voices[OpenHat].choked = true;         // closed hat chokes the open hat
    }

    static float noise (VoiceState& s)
    {
        s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
        return (float) (int32_t) s.rng * (1.0f / 2147483648.0f);
    }

    static float highpass (VoiceState& s, float x, float a)
    {
        const float y = a * (s.hpY + x - s.hpX);
        s.hpX = x; s.hpY = y;
        return y;
    }

    float metal (VoiceState& s, float amount)
    {
        static const float freq[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        float sum = 0.0f;
        for (int i = 0; i < 6; ++i)
        {
            s.osc[i] += freq[i] * dt * (1.0f + amount);
            if (s.osc[i] >= 1.0f) s.osc[i] -= 1.0f;
            sum += s.osc[i] < 0.5f ? 1.0f : -1.0f;
        }
        return sum * 0.18f;
    }

    float tick (int v)
    {
        auto& s = voices[v];
        const float t = s.t;
        float x = 0.0f;
        float life = 0.5f;
        switch (v)
        {
            case Kick:
            {
                const float f = 46.0f + 125.0f * std::exp (-t * 30.0f);
                s.phase += twoPi * f * dt; if (s.phase > twoPi) s.phase -= twoPi;
                x = std::sin (s.phase) * std::exp (-t * 6.5f) + noise (s) * std::exp (-t * 400.0f) * 0.25f;
                x = std::tanh (x * 1.7f) * 0.95f;
                life = 0.7f;
                break;
            }
            case Snare:
            {
                s.phase += twoPi * 185.0f * dt; if (s.phase > twoPi) s.phase -= twoPi;
                s.phase2 += twoPi * 330.0f * dt; if (s.phase2 > twoPi) s.phase2 -= twoPi;
                const float tone = (std::sin (s.phase) * 0.6f + std::sin (s.phase2) * 0.35f) * std::exp (-t * 22.0f);
                const float nz = highpass (s, noise (s), hpSnare) * std::exp (-t * 15.0f);
                x = tone * 0.55f + nz * 0.8f;
                life = 0.45f;
                break;
            }
            case Clap:
            {
                float env = 0.0f;
                for (int k = 0; k < 3; ++k) { const float d = t - 0.011f * (float) k; if (d >= 0.0f) env += std::exp (-d * 380.0f); }
                if (t >= 0.033f) env += 0.8f * std::exp (-(t - 0.033f) * 26.0f);
                const float n = highpass (s, noise (s), hpClap);
                s.lpY += 0.4f * (n - s.lpY);
                x = s.lpY * env * 1.4f;
                life = 0.5f;
                break;
            }
            case Hat:
                x = highpass (s, metal (s, 0.0f) + noise (s) * 0.12f, hpHat) * std::exp (-t * 62.0f) * 1.2f;
                life = 0.12f;
                break;
            case OpenHat:
            {
                float env = std::exp (-t * 8.5f);
                if (s.choked) { s.chokeT += dt; env *= std::exp (-s.chokeT * 600.0f); if (s.chokeT > 0.01f) s.active = false; }
                x = highpass (s, metal (s, 0.0f) + noise (s) * 0.12f, hpHat) * env * 1.1f;
                life = 0.9f;
                break;
            }
            case Cymbal:
                x = highpass (s, metal (s, cymAmt) + noise (s) * 0.25f, hpCym) * std::exp (-t * 2.4f) * 0.9f;
                life = 2.2f;
                break;
        }
        s.t += dt;
        if (s.t > life) s.active = false;
        return x * s.gain * baseGain[v];
    }

    static constexpr float baseGain[numVoices] = { 1.0f, 0.9f, 0.8f, 0.55f, 0.55f, 0.5f };
};
