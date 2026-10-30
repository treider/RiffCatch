#pragma once
#include <juce_core/juce_core.h>
#include "audio/AudioEngine.h"

namespace Exporter
{
    // Offline bounce of the master mix (tracks only, no click). `loops` = how many times the loop is rendered.
    // Writes 24-bit stereo WAV. Returns false with an error message on failure.
    bool bounceWav (AudioEngine& engine, const juce::File& file, int loops, juce::String& error);
}
