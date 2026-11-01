#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "DrumMachine.h"
#include "BassMachine.h"

class AudioEngine;

// One track of a pattern: its mixer settings and its audio clip.
struct PatternTrack
{
    juce::String name;
    float volume = 1.0f, pan = 0.0f;
    bool mute = false, solo = false, armed = false, echoOn = false, reverbOn = false;
    int input = 0, echoDiv = 1, offset = 0;
    juce::AudioBuffer<float> audio;
    bool has() const noexcept { return audio.getNumSamples() > 0; }
};

// Up to 16 patterns (the tracker idea). A pattern owns its tracks (with their mix), its drum and bass pattern and the mix of
// those two, and its loop length. One pattern is "open": its content lives in the tracks and the drum / bass machines;
// the others are stored here. Switching stores the open one and loads the other.
// Tempo, time signature, master volume and the synth sound belong to the whole project. Message thread only.
class PatternBank
{
public:
    static constexpr int maxPatterns = 16;

    struct DrumMix { float volume = 0.8f, level[DrumMachine::numVoices], pan[DrumMachine::numVoices]; bool mute = false, solo = false, voiceMute[DrumMachine::numVoices];
                     DrumMix() { for (int v = 0; v < DrumMachine::numVoices; ++v) { level[v] = 1.0f; pan[v] = 0.0f; voiceMute[v] = false; } } };
    struct BassMix { float volume = 0.8f; bool mute = false, solo = false, echoOn = false, reverbOn = false; int echoDiv = 1; };

    struct Pattern
    {
        bool used = false;
        juce::String name;
        int colour = 0;                    // index into Theme::trackColour
        int loopBars = 4;
        std::vector<PatternTrack> tracks;
        DrumMachine::Snapshot drums;
        BassMachine::Snapshot bass;
        DrumMix drumMix;
        BassMix bassMix;
    };

    std::vector<Pattern> pats = std::vector<Pattern> ((size_t) maxPatterns);      // on the heap: engines are also created on the stack
    int current = 0;

    PatternBank() { reset(); }

    void reset()
    {
        for (auto& p : pats) p = Pattern();
        pats[0].used = true; pats[0].name = defaultName (0); pats[0].colour = 0;
        current = 0;
    }

    static juce::String defaultName (int i) { return "Pattern " + juce::String (i + 1); }
    int numUsed() const { int n = 0; for (auto& p : pats) n += p.used ? 1 : 0; return n; }
    int firstFree() const { for (int i = 0; i < maxPatterns; ++i) if (! pats[(size_t) i].used) return i; return -1; }
    bool isUsed (int i) const { return i >= 0 && i < maxPatterns && pats[(size_t) i].used; }

    // copy the live state (tracks, drums, bass, loop length) into the open pattern
    void storeCurrent (AudioEngine& e);
    // rebuild the engine's tracks and the machines from the open pattern
    void loadCurrent (AudioEngine& e);

    // a new pattern in the first free slot: one empty track, no notes, same loop length and key as the open one; -1 if full
    int addBlank (AudioEngine& e);
    int duplicate (AudioEngine& e, int src);
    void clearPattern (AudioEngine& e, int i);       // empty clips and notes, tracks and names stay
    bool remove (AudioEngine& e, int i);             // false for the last pattern

    bool hasContent (AudioEngine& e, int i) const;

    static DrumMix captureDrumMix (const DrumMachine& d);
    static BassMix captureBassMix (const BassMachine& b);
};