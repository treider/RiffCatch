#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Finds the audible part of a clip so silence before and after it can be cut away.
namespace SilenceTrim
{
    // Returns true when something can be trimmed. [start, end) is the part to keep, with a little padding.
    inline bool findRange (const juce::AudioBuffer<float>& b, int len, double sr,
                           juce::int64& start, juce::int64& end, float thresholdDb = -50.0f, double padMs = 15.0)
    {
        len = juce::jmin (len, b.getNumSamples());
        if (len <= 0) return false;
        const float thr = juce::Decibels::decibelsToGain (thresholdDb);
        auto exceeds = [&] (int i)
        {
            for (int c = 0; c < b.getNumChannels(); ++c) if (std::abs (b.getSample (c, i)) > thr) return true;
            return false;
        };
        int first = -1, last = -1;
        for (int i = 0; i < len; ++i) if (exceeds (i)) { first = i; break; }
        if (first < 0) return false;                       // all silence: leave it alone
        for (int i = len - 1; i >= first; --i) if (exceeds (i)) { last = i; break; }
        const int pad = (int) (padMs * 0.001 * sr);
        start = juce::jmax (0, first - pad);
        end = juce::jmin (len, last + 1 + pad * 3);        // keep a slightly longer tail so decays are not clipped
        return start > 0 || end < len;
    }
}
