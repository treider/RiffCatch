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

// Up to 16 patterns (the tracker idea). A pattern owns its tracks (with their mix), the mix of the drums and the bass, its loop
// length, and points at one drum pattern (D1..D16) and one bass pattern (B1..B16) from two shared libraries, so the same groove
// or bass line can be used in several patterns. One pattern is "open": its content lives in the tracks and the drum / bass
// machines; the others are stored here. Switching stores the open one and loads the other.
// Tempo, time signature, master volume and the synth sound belong to the whole project. Message thread only.
class PatternBank
{
public:
    static constexpr int maxPatterns = 16, maxItems = 16;
    enum Kind { Drums = 0, Bass = 1 };

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
        int drumRef = 0, bassRef = 0;      // which library items the pattern plays
        DrumMix drumMix;
        BassMix bassMix;
    };

    std::vector<Pattern> pats = std::vector<Pattern> ((size_t) maxPatterns);      // on the heap: engines are also created on the stack
    int current = 0;

    // the libraries: item i is shown as "D<i+1>" / "B<i+1>"; the open pattern's items are stale until storeCurrent / storeMachines
    std::vector<DrumMachine::Snapshot> drumLib = std::vector<DrumMachine::Snapshot> ((size_t) maxItems);
    std::vector<BassMachine::Snapshot> bassLib = std::vector<BassMachine::Snapshot> ((size_t) maxItems);
    bool drumUsed[maxItems] {}, bassUsed[maxItems] {};

    PatternBank() { reset(); }

    void reset()
    {
        for (auto& p : pats) p = Pattern();
        for (auto& d : drumLib) d = DrumMachine::Snapshot();
        for (auto& b : bassLib) b = BassMachine::Snapshot();
        for (int i = 0; i < maxItems; ++i) drumUsed[i] = bassUsed[i] = false;
        drumUsed[0] = bassUsed[0] = true;
        pats[0].used = true; pats[0].name = defaultName (0); pats[0].colour = 0;
        current = 0;
    }

    static juce::String defaultName (int i) { return "Pattern " + juce::String (i + 1); }
    int numUsed() const { int n = 0; for (auto& p : pats) n += p.used ? 1 : 0; return n; }
    int firstFree() const { for (int i = 0; i < maxPatterns; ++i) if (! pats[(size_t) i].used) return i; return -1; }
    bool isUsed (int i) const { return i >= 0 && i < maxPatterns && pats[(size_t) i].used; }

    // library bookkeeping
    bool itemUsed (Kind k, int i) const { return i >= 0 && i < maxItems && (k == Drums ? drumUsed[i] : bassUsed[i]); }
    int numItems (Kind k) const { int n = 0; for (int i = 0; i < maxItems; ++i) n += itemUsed (k, i) ? 1 : 0; return n; }
    int refOf (const Pattern& p, Kind k) const { return k == Drums ? p.drumRef : p.bassRef; }
    int currentRef (Kind k) const { return refOf (pats[(size_t) current], k); }
    std::vector<int> usedBy (Kind k, int item) const;          // the patterns that play this item

    // copy the live state (tracks, drums, bass, loop length) into the open pattern
    void storeCurrent (AudioEngine& e);
    // only the drum and bass notes of the open pattern into their library items (cheap; before saving)
    void storeMachines (AudioEngine& e);
    // rebuild the engine's tracks and the machines from the open pattern
    void loadCurrent (AudioEngine& e);

    // a new pattern in the first free slot: one empty track, the same drum and bass patterns as the open one; -1 if full
    int addBlank (AudioEngine& e);
    int duplicate (AudioEngine& e, int src);         // copies the tracks; the drum and bass patterns are shared with the original
    void clearPattern (AudioEngine& e, int i);       // empties the audio clips (the shared drum and bass patterns stay)
    bool remove (AudioEngine& e, int i);             // false for the last pattern

    bool hasContent (AudioEngine& e, int i) const;

    // the open pattern plays another library item / a new one (empty, or a copy of what is playing now); the machines follow
    void selectItem (AudioEngine& e, Kind k, int item);
    int newItem (AudioEngine& e, Kind k, bool copyCurrent);   // -1 if the library is full
    bool deleteItem (Kind k, int item);                        // false while a pattern plays it

    static DrumMix captureDrumMix (const DrumMachine& d);
    static BassMix captureBassMix (const BassMachine& b);

    int allocDrum (const DrumMachine::Snapshot& s);        // a library item for loading projects; -1 if full
    int allocBass (const BassMachine::Snapshot& s);

 private:
    static DrumMachine::Snapshot emptyDrums (AudioEngine& e);
    static BassMachine::Snapshot emptyBass (AudioEngine& e);
    bool itemEmpty (Kind k, int i) const;
    void prune();                                              // frees empty library items nobody plays
};