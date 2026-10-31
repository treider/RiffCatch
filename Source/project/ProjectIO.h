#pragma once
#include <juce_core/juce_core.h>
#include "audio/AudioEngine.h"

// Project folder: project.json + one WAV per track.
namespace ProjectIO
{
    constexpr int maxNotesLength = 20000;
    constexpr int maxNameLength = 120;
    // A track file name from project.json must be a plain "<name>.wav" in the project folder (no paths, no "..").
    bool isSafeTrackFile (const juce::File& dir, const juce::String& name);
    bool save (AudioEngine& engine, const juce::File& dir, juce::String& error);
    bool load (AudioEngine& engine, const juce::File& dir, juce::String& error);
}

