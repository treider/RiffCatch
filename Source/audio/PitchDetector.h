#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

// Monophonic pitch detection (YIN). Runs on the UI thread on a copy of the latest input samples.
struct PitchDetector
{
    struct Result { float freq = 0.0f; float clarity = 0.0f; bool valid = false; };

    // x: samples (mono), n: number of samples (>= 2 * maxPeriod + 1 recommended). Returns freq in Hz.
    Result detect (const float* x, int n, double sr, double minHz = 60.0, double maxHz = 1200.0)
    {
        Result res;
        const int tauMax = juce::jmin (n / 2 - 1, (int) (sr / minHz));
        const int tauMin = juce::jmax (2, (int) (sr / maxHz));
        const int w = n / 2;
        if (tauMax <= tauMin + 2 || w < tauMax) return res;

        double energy = 0.0;
        for (int i = 0; i < w; ++i) energy += (double) x[i] * x[i];
        if (energy / w < 1.0e-6) return res;                // too quiet (below about -60 dB RMS)

        diff.assign ((size_t) tauMax + 1, 0.0f);
        cmnd.assign ((size_t) tauMax + 1, 1.0f);
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            for (int i = 0; i < w; ++i) { const double d = (double) x[i] - x[i + tau]; s += d * d; }
            diff[(size_t) tau] = (float) s;
        }
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;
        const float threshold = 0.15f;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0) return res;

        // parabolic interpolation around the minimum
        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        }
        res.freq = (float) (sr / tauF);
        res.clarity = 1.0f - cmnd[(size_t) best];
        res.valid = res.freq >= minHz && res.freq <= maxHz;
        return res;
    }

private:
    std::vector<float> diff, cmnd;
};
