#pragma once
#include <juce_core/juce_core.h>
#include "audio/AudioEngine.h"

namespace Exporter
{
    constexpr bool mp3Available =
#if RC_MP3
        true;
#else
        false;
#endif

    // What an offline bounce renders.
    enum class Source
    {
        Live,       // the live tracks / loop (the song when song mode is on, as before)
        Song,       // the patterns in play order
        Pattern     // one pattern, repeated `loops` times
    };

    // Offline bounce of the master mix (tracks only, no click). `loops` = how many times the loop / pattern is rendered.
    bool bounceWav (AudioEngine& engine, const juce::File& file, Source source, int pattern, int loops, juce::String& error);   // 24-bit stereo
    // The whole song (the patterns in play order) as one stereo buffer with a short tail; master volume applied only if asked.
    bool renderSongBuffer (AudioEngine& engine, juce::AudioBuffer<float>& mix, juce::String& error, bool applyMaster);
    // One pattern from the pattern bank, repeated `repeats` times, plus the same short tail.
    bool renderPatternBuffer (AudioEngine& engine, int pattern, int repeats, juce::AudioBuffer<float>& mix, juce::String& error, bool applyMaster);
    bool bounceMp3 (AudioEngine& engine, const juce::File& file, Source source, int pattern, int loops, int kbps, juce::String& error);
}
