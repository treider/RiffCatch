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

    // Offline bounce of the master mix (tracks only, no click). `loops` = how many times the loop is rendered.
    bool bounceWav (AudioEngine& engine, const juce::File& file, int loops, juce::String& error);   // 24-bit stereo
    // The whole song (the patterns in play order) as one stereo buffer with a short tail; master volume applied only if asked.
    bool renderSongBuffer (AudioEngine& engine, juce::AudioBuffer<float>& mix, juce::String& error, bool applyMaster);
    bool bounceMp3 (AudioEngine& engine, const juce::File& file, int loops, int kbps, juce::String& error);
}
