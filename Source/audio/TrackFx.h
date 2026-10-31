#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Per-track insert effects: tempo-synced echo and a small reverb. Not thread-safe: the audio thread owns it
// once prepared (prepare() allocates and must be called from the message/device thread, never from the callback).
class TrackFx
{
public:
    enum Div { Half = 0, Quarter, DottedEighth, Eighth, Sixteenth, numDivs };
    static double divBeats (int d) { static const double b[numDivs] = { 2.0, 1.0, 0.75, 0.5, 0.25 }; return b[juce::jlimit (0, (int) numDivs - 1, d)]; }
    static const char* divName (int d) { static const char* const n[numDivs] = { "1/2", "1/4", "1/8 dotted", "1/8", "1/16" }; return n[juce::jlimit (0, (int) numDivs - 1, d)]; }

    void prepare (double sr)
    {
        sampleRate = sr;
        maxDelay = juce::jmax (1024, (int) (4.0 * sr));
        bufL.assign ((size_t) maxDelay, 0.0f);
        bufR.assign ((size_t) maxDelay, 0.0f);
        w = 0;
        reverb.setSampleRate (sr);
        juce::Reverb::Parameters p;
        p.roomSize = 0.6f; p.damping = 0.45f; p.wetLevel = 0.10f; p.dryLevel = 0.5f; p.width = 1.0f; p.freezeMode = 0.0f;   // juce scales wet x3, dry x2
        reverb.setParameters (p);
        reverb.reset();
        echoWasOn = reverbWasOn = false;
    }

    bool ready() const noexcept { return maxDelay > 0; }
    bool wasActive() const noexcept { return echoWasOn || reverbWasOn; }

    // Processes n samples in place (L/R hold the dry track signal).
    void process (float* L, float* R, int n, bool echoOn, int div, bool reverbOn, double samplesPerBeat)
    {
        if (! ready()) return;
        if (echoOn)
        {
            const int D = juce::jlimit (1, maxDelay - 1, (int) std::lround (divBeats (div) * samplesPerBeat));
            for (int i = 0; i < n; ++i)
            {
                int r = w - D; if (r < 0) r += maxDelay;
                const float dl = bufL[(size_t) r], dr = bufR[(size_t) r];
                const float inL = L[i], inR = R[i];
                L[i] = inL + dl * echoMix;
                R[i] = inR + dr * echoMix;
                bufL[(size_t) w] = inL + dl * feedback;
                bufR[(size_t) w] = inR + dr * feedback;
                if (++w >= maxDelay) w = 0;
            }
        }
        else if (echoWasOn)
        {
            std::fill (bufL.begin(), bufL.end(), 0.0f);
            std::fill (bufR.begin(), bufR.end(), 0.0f);
        }
        echoWasOn = echoOn;

        if (reverbOn) reverb.processStereo (L, R, n);
        else if (reverbWasOn) reverb.reset();
        reverbWasOn = reverbOn;
    }

private:
    static constexpr float echoMix = 0.38f, feedback = 0.42f;
    double sampleRate = 0.0;
    int maxDelay = 0, w = 0;
    std::vector<float> bufL, bufR;
    juce::Reverb reverb;
    bool echoWasOn = false, reverbWasOn = false;
};
