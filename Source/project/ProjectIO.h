#pragma once
#include <juce_core/juce_core.h>
#include "audio/AudioEngine.h"

// A project is a folder that holds <name>.riffcatch (the project file, JSON) and one WAV per track.
namespace ProjectIO
{
    inline const char* const extension = ".riffcatch";
    inline bool isProjectFile (const juce::File& f) { return f.hasFileExtension ("riffcatch"); }
    // The project file for a project folder: <folder>/<folder name>.riffcatch
    inline juce::File fileInFolder (const juce::File& folder) { return folder.getChildFile (folder.getFileName() + extension); }
    constexpr int maxNotesLength = 20000;
    constexpr int maxNameLength = 120;
    // A track file name from project.json must be a plain "<name>.wav" in the project folder (no paths, no "..").
    bool isSafeTrackFile (const juce::File& dir, const juce::String& name);
    bool save (AudioEngine& engine, const juce::File& projectFile, juce::String& error);
    bool load (AudioEngine& engine, const juce::File& projectFile, juce::String& error);
}

