#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// A detected hit: `src` = position in the original take, `dst` = where it should sound (edited).
struct Hit { juce::int64 src = 0, dst = 0; };

namespace HitEditor
{
    // Energy-rise onset detection. riseDb: how sharp the level jump must be (lower = more hits).
    std::vector<juce::int64> detectOnsets (const juce::AudioBuffer<float>& take, double sampleRate, float riseDb);

    // Non-destructive re-timing: each region between two hits is moved/time-stretched (pitch preserved,
    // WSOLA) so every hit lands on its `dst`. The first ~15 ms of each hit is kept untouched to keep the attack.
    juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& take, const std::vector<Hit>& hits, double sampleRate);
}
