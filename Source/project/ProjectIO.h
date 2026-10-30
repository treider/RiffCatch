#pragma once
#include <juce_core/juce_core.h>
#include "audio/AudioEngine.h"

// Project folder: project.json + one WAV per track.
namespace ProjectIO
{
    bool save (AudioEngine& engine, const juce::File& dir, juce::String& error);
    bool load (AudioEngine& engine, const juce::File& dir, juce::String& error);
}

