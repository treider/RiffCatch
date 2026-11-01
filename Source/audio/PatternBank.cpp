#include "PatternBank.h"
#include "AudioEngine.h"

PatternBank::DrumMix PatternBank::captureDrumMix (const DrumMachine& d)
{
    DrumMix m;
    m.volume = d.volume.load(); m.mute = d.mute.load(); m.solo = d.solo.load();
    for (int v = 0; v < DrumMachine::numVoices; ++v) { m.level[v] = d.level[v].load(); m.pan[v] = d.pan[v].load(); m.voiceMute[v] = d.voiceMute[v].load(); }
    return m;
}

PatternBank::BassMix PatternBank::captureBassMix (const BassMachine& b)
{
    BassMix m;
    m.volume = b.volume.load(); m.mute = b.mute.load(); m.solo = b.solo.load();
    m.echoOn = b.echoOn.load(); m.reverbOn = b.reverbOn.load(); m.echoDiv = b.echoDiv.load();
    return m;
}

DrumMachine::Snapshot PatternBank::emptyDrums (AudioEngine& e)
{
    DrumMachine::Snapshot s;
    s.barSteps = e.drums.barSteps.load();
    return s;
}

BassMachine::Snapshot PatternBank::emptyBass (AudioEngine& e)
{
    auto s = e.bass.snapshot();                       // same key and bar layout, no notes
    std::memset (s.steps, 0, sizeof (s.steps));
    std::memset (s.shift, 0, sizeof (s.shift));
    s.bars = 1;
    return s;
}

int PatternBank::allocDrum (const DrumMachine::Snapshot& s)
{
    for (int i = 0; i < maxItems; ++i) if (! drumUsed[i]) { drumLib[(size_t) i] = s; drumUsed[i] = true; return i; }
    return -1;
}

int PatternBank::allocBass (const BassMachine::Snapshot& s)
{
    for (int i = 0; i < maxItems; ++i) if (! bassUsed[i]) { bassLib[(size_t) i] = s; bassUsed[i] = true; return i; }
    return -1;
}

std::vector<int> PatternBank::usedBy (Kind k, int item) const
{
    std::vector<int> v;
    for (int i = 0; i < maxPatterns; ++i)
        if (pats[(size_t) i].used && refOf (pats[(size_t) i], k) == item) v.push_back (i);
    return v;
}

bool PatternBank::itemEmpty (Kind k, int i) const
{
    if (k == Drums)
    {
        const auto& d = drumLib[(size_t) i];
        for (int v = 0; v < DrumMachine::numVoices; ++v) for (int s = 0; s < DrumMachine::maxSteps; ++s) if (d.steps[v][s] != 0) return false;
        return true;
    }
    const auto& b = bassLib[(size_t) i];
    for (int s = 0; s < BassMachine::maxSteps; ++s) if (BassMachine::isNote (b.steps[s])) return false;
    return true;
}

void PatternBank::prune()          // the library is global: only empty patterns that nobody plays are dropped
{
    for (int i = 0; i < maxItems; ++i)
    {
        if (drumUsed[i] && usedBy (Drums, i).empty() && itemEmpty (Drums, i)) drumUsed[i] = false;
        if (bassUsed[i] && usedBy (Bass, i).empty() && itemEmpty (Bass, i)) bassUsed[i] = false;
    }
}
void PatternBank::storeMachines (AudioEngine& e)
{
    const auto& p = pats[(size_t) juce::jlimit (0, maxPatterns - 1, current)];
    if (itemUsed (Drums, p.drumRef)) drumLib[(size_t) p.drumRef] = e.drums.snapshot();
    if (itemUsed (Bass, p.bassRef))  bassLib[(size_t) p.bassRef] = e.bass.snapshot();
}

void PatternBank::storeCurrent (AudioEngine& e)
{
    auto& p = pats[(size_t) juce::jlimit (0, maxPatterns - 1, current)];
    p.used = true;
    p.tracks.clear();
    for (int slot : e.order)
    {
        auto& t = e.tracks[slot];
        PatternTrack pt;
        pt.name = t.name; pt.volume = t.volume.load(); pt.pan = t.pan.load();
        pt.mute = t.mute.load(); pt.solo = t.solo.load();     // arming is not stored: only one track is armed in the whole project
        pt.echoOn = t.echoOn.load(); pt.reverbOn = t.reverbOn.load(); pt.echoDiv = t.echoDiv.load();
        pt.input = t.inputCode.load(); pt.offset = t.offset.load();
        const int len = (int) juce::jmin<juce::int64> (t.length.load(), t.data.getNumSamples());
        if (len > 0)
        {
            pt.audio.setSize (2, len);
            juce::SpinLock::ScopedLockType l (t.lock);
            for (int ch = 0; ch < 2; ++ch) pt.audio.copyFrom (ch, 0, t.data, ch, 0, len);
        }
        p.tracks.push_back (std::move (pt));
    }
    storeMachines (e);
    p.drumMix = captureDrumMix (e.drums);
    p.bassMix = captureBassMix (e.bass);
    p.loopBars = e.loopBars.load();
}

void PatternBank::loadCurrent (AudioEngine& e)
{
    auto& p = pats[(size_t) juce::jlimit (0, maxPatterns - 1, current)];
    int armedPos = -1;                                 // arming follows the position of the armed track from the pattern we leave
    { int k = 0; for (int slot : e.order) { if (e.tracks[slot].armed.load() && armedPos < 0) armedPos = k; ++k; } }
    const auto old = e.order;
    for (int slot : old) e.removeTrack (slot);
    e.order.clear();
    if (p.tracks.empty()) p.tracks.emplace_back();
    int n = 0;
    for (auto& pt : p.tracks)
    {
        const int slot = e.addTrack();
        if (slot < 0) break;
        auto& t = e.tracks[slot];
        t.name = pt.name.isEmpty() ? "Track " + juce::String (++n) : pt.name;
        t.volume = pt.volume; t.pan = pt.pan; t.mute = pt.mute; t.solo = pt.solo;
        t.echoOn = pt.echoOn; t.reverbOn = pt.reverbOn; t.echoDiv = pt.echoDiv; t.inputCode = pt.input;
        if (pt.has()) { auto copy = pt.audio; t.setAudio (std::move (copy)); }
        t.offset = pt.has() ? pt.offset : 0;
    }
    if (armedPos >= 0 && ! e.order.empty()) e.tracks[e.order[(size_t) juce::jmin (armedPos, (int) e.order.size() - 1)]].armed = true;
    if (! itemUsed (Drums, p.drumRef)) { const int n = allocDrum (emptyDrums (e)); p.drumRef = n >= 0 ? n : 0; drumUsed[p.drumRef] = true; }
    if (! itemUsed (Bass, p.bassRef))  { const int n = allocBass (emptyBass (e)); p.bassRef = n >= 0 ? n : 0; bassUsed[p.bassRef] = true; }
    e.drums.apply (drumLib[(size_t) p.drumRef]);
    e.bass.apply (bassLib[(size_t) p.bassRef]);
    auto& dm = e.drums; const auto& d = p.drumMix;
    dm.volume = d.volume; dm.mute = d.mute; dm.solo = d.solo;
    for (int v = 0; v < DrumMachine::numVoices; ++v) { dm.level[v] = d.level[v]; dm.pan[v] = d.pan[v]; dm.voiceMute[v] = d.voiceMute[v]; }
    auto& bm = e.bass; const auto& b = p.bassMix;
    bm.volume = b.volume; bm.mute = b.mute; bm.solo = b.solo; bm.echoOn = b.echoOn; bm.reverbOn = b.reverbOn; bm.echoDiv = b.echoDiv;
    e.loopBars = juce::jlimit (1, 64, p.loopBars);
}

int PatternBank::addBlank (AudioEngine& e)
{
    const int i = firstFree();
    if (i < 0) return -1;
    storeMachines (e);
    auto& p = pats[(size_t) i];
    const int cd = currentRef (Drums), cb = currentRef (Bass);
    p = Pattern();
    p.used = true;
    p.name = defaultName (i);
    p.colour = i;
    p.loopBars = e.loopBars.load();
    PatternTrack t;
    t.name = "Track 1";
    if (! e.order.empty()) t.input = e.tracks[e.order.front()].inputCode.load();
    p.tracks.push_back (std::move (t));
    p.drumRef = cd;                                   // the drum and bass patterns are a shared library: a new pattern starts on the same ones
    p.bassRef = cb;
    p.drumMix = captureDrumMix (e.drums);
    p.bassMix = captureBassMix (e.bass);
    return i;
}

int PatternBank::duplicate (AudioEngine& e, int src)
{
    if (! isUsed (src)) return -1;
    if (src == current) storeCurrent (e);
    const int i = firstFree();
    if (i < 0) return -1;
    pats[(size_t) i] = pats[(size_t) src];            // same tracks, same drum and bass patterns (they are shared, not copied)
    pats[(size_t) i].name = pats[(size_t) src].name + " copy";
    return i;
}
void PatternBank::clearPattern (AudioEngine& e, int i)
{
    if (! isUsed (i)) return;
    if (i == current) storeCurrent (e);
    for (auto& t : pats[(size_t) i].tracks) { t.audio.setSize (0, 0); t.offset = 0; }     // only this pattern's audio: the drum and bass patterns are shared
    if (i == current) loadCurrent (e);
}
bool PatternBank::remove (AudioEngine& e, int i)
{
    if (! isUsed (i) || numUsed() <= 1) return false;
    const bool wasCurrent = i == current;
    pats[(size_t) i] = Pattern();
    prune();
    if (wasCurrent)
    {
        int next = -1;
        for (int d = 1; d < maxPatterns && next < 0; ++d)         // the nearest pattern to the left, else to the right
        {
            if (isUsed (i - d)) next = i - d;
            else if (isUsed (i + d)) next = i + d;
        }
        current = juce::jmax (0, next);
        loadCurrent (e);
    }
    return true;
}

bool PatternBank::hasContent (AudioEngine& e, int i) const
{
    if (! isUsed (i)) return false;
    if (i == current)
    {
        for (int slot : e.order) if (e.tracks[slot].length.load() > 0) return true;
        return e.drums.hasAnySteps() || e.bass.hasAnySteps();
    }
    const auto& p = pats[(size_t) i];
    for (auto& t : p.tracks) if (t.has()) return true;
    const auto& d = drumLib[(size_t) juce::jlimit (0, maxItems - 1, p.drumRef)];
    const auto& b = bassLib[(size_t) juce::jlimit (0, maxItems - 1, p.bassRef)];
    for (int v = 0; v < DrumMachine::numVoices; ++v) for (int k = 0; k < DrumMachine::maxSteps; ++k) if (d.steps[v][k] != 0) return true;
    for (int k = 0; k < BassMachine::maxSteps; ++k) if (BassMachine::isNote (b.steps[k])) return true;
    return false;
}

void PatternBank::selectItem (AudioEngine& e, Kind k, int item)
{
    if (item < 0 || item >= maxItems) return;
    storeMachines (e);                                 // what is playing goes back into its item
    auto& p = pats[(size_t) current];
    const int old = refOf (p, k);
    if (! itemUsed (k, item))                          // a number that does not exist yet starts as a new empty pattern
    {
        if (k == Drums) { drumLib[(size_t) item] = emptyDrums (e); drumUsed[item] = true; }
        else            { bassLib[(size_t) item] = emptyBass (e);  bassUsed[item] = true; }
    }
    if (k == Drums) { p.drumRef = item; e.drums.apply (drumLib[(size_t) item]); }
    else            { p.bassRef = item; e.bass.apply (bassLib[(size_t) item]); }
    if (old != item && usedBy (k, old).empty() && itemEmpty (k, old))   // an empty pattern that nobody plays disappears when you leave it
        (k == Drums ? drumUsed : bassUsed)[old] = false;
}

int PatternBank::newItem (AudioEngine& e, Kind k, bool copyCurrent)
{
    storeMachines (e);
    auto& p = pats[(size_t) current];
    int idx;
    if (k == Drums) idx = allocDrum (copyCurrent ? drumLib[(size_t) p.drumRef] : emptyDrums (e));
    else            idx = allocBass (copyCurrent ? bassLib[(size_t) p.bassRef] : emptyBass (e));
    if (idx < 0) return -1;
    selectItem (e, k, idx);
    return idx;
}

bool PatternBank::deleteItem (Kind k, int item)
{
    if (! itemUsed (k, item) || numItems (k) <= 1 || ! usedBy (k, item).empty()) return false;
    (k == Drums ? drumUsed : bassUsed)[item] = false;
    return true;
}