#pragma once
#include "BassMachine.h"

// Random bass line inside the current key and scale: notes on a 1/4, 1/8 or 1/16 grid, with a density, optional random note
// lengths, a preference for the root and fifth, and small steps between notes so it still sounds like a line.
namespace BassRandom
{
    struct Options
    {
        int grid = 1;               // 0 = 1/4, 1 = 1/8, 2 = 1/16
        float density = 0.7f;       // chance that a grid position gets a note
        bool randomLength = false;  // lengths of 1..n grid units (up to the next note) instead of one grid unit
        int octaves = 2;            // range above the root
        bool repeatBar = false;     // generate bar 1 and copy it to the other bars
        float rootBias = 0.35f;     // chance of root / fifth
        float accent = 0.15f;
        float slide = 0.0f;
    };

    inline BassMachine::Snapshot generate (const BassMachine::Snapshot& cur, const Options& o, juce::Random& rng)
    {
        using BM = BassMachine;
        auto out = cur;
        std::memset (out.steps, 0, sizeof (out.steps));
        const int bs = cur.barSteps, n = BM::scaleSize (cur.scale);
        const int g = o.grid == 0 ? 4 : o.grid == 1 ? 2 : 1;
        const int maxDeg = juce::jmax (1, n * juce::jlimit (1, 3, o.octaves));
        const int fifth = BM::nearestDegree (cur.scale, 7);
        int prev = 0;

        const int barsToMake = o.repeatBar ? 1 : cur.bars;
        for (int b = 0; b < barsToMake; ++b)
        {
            std::vector<int> ks;
            for (int k = 0; k < bs; k += g)
                if ((k == 0 && rng.nextFloat() < juce::jmax (0.9f, o.density)) || (k != 0 && rng.nextFloat() < o.density)) ks.push_back (k);
            if (ks.empty()) ks.push_back (0);
            for (size_t i = 0; i < ks.size(); ++i)
            {
                int d;
                const bool strong = ks[i] % 4 == 0;
                if (rng.nextFloat() < o.rootBias * (strong ? 1.4f : 0.8f))
                    d = rng.nextBool() || fifth < 0 || fifth >= maxDeg ? 0 : fifth;
                else if (rng.nextFloat() < 0.65f)                       // a small step from the last note
                    d = prev + (rng.nextBool() ? 1 : -1) * (1 + rng.nextInt (2));
                else
                    d = rng.nextInt (maxDeg);
                d = juce::jlimit (0, maxDeg - 1, d);
                prev = d;
                const int span = (i + 1 < ks.size() ? ks[i + 1] : bs) - ks[i];
                int len = g;
                if (o.randomLength) len = g * (1 + rng.nextInt (juce::jmax (1, span / g)));
                len = juce::jlimit (1, juce::jmax (1, span), len);
                const bool legato = i > 0 && ks[i - 1] + BM::lenOf (out.steps[b * bs + ks[i - 1]]) >= ks[i];
                const bool sl = legato && rng.nextFloat() < o.slide;
                out.steps[b * bs + ks[i]] = BM::pack (d, len, sl, rng.nextFloat() < o.accent || ks[i] == 0);
            }
        }
        if (o.repeatBar)
            for (int b = 1; b < cur.bars; ++b)
                for (int k = 0; k < bs; ++k) out.steps[b * bs + k] = out.steps[k];
        return out;
    }
}
