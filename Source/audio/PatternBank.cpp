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
    p.drums = e.drums.snapshot();
    p.bass = e.bass.snapshot();
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
    e.drums.apply (p.drums);
    e.bass.apply (p.bass);
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
    auto& p = pats[(size_t) i];
    p = Pattern();
    p.used = true;
    p.name = defaultName (i);
    p.colour = i;
    p.loopBars = e.loopBars.load();
    PatternTrack t;
    t.name = "Track 1";
    if (! e.order.empty()) t.input = e.tracks[e.order.front()].inputCode.load();
    p.tracks.push_back (std::move (t));
    p.drums.barSteps = e.drums.barSteps.load();
    p.bass = e.bass.snapshot();                       // same key and bar layout, no notes
    std::memset (p.bass.steps, 0, sizeof (p.bass.steps));
    std::memset (p.bass.shift, 0, sizeof (p.bass.shift));
    p.bass.bars = 1;
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
    pats[(size_t) i] = pats[(size_t) src];
    pats[(size_t) i].name = pats[(size_t) src].name + " copy";
    return i;
}

void PatternBank::clearPattern (AudioEngine& e, int i)
{
    if (! isUsed (i)) return;
    if (i == current) storeCurrent (e);
    auto& p = pats[(size_t) i];
    for (auto& t : p.tracks) { t.audio.setSize (0, 0); t.offset = 0; }
    std::memset (p.drums.steps, 0, sizeof (p.drums.steps));
    std::memset (p.bass.steps, 0, sizeof (p.bass.steps));
    std::memset (p.bass.shift, 0, sizeof (p.bass.shift));
    if (i == current) loadCurrent (e);
}

bool PatternBank::remove (AudioEngine& e, int i)
{
    if (! isUsed (i) || numUsed() <= 1) return false;
    const bool wasCurrent = i == current;
    pats[(size_t) i] = Pattern();
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
    for (int v = 0; v < DrumMachine::numVoices; ++v) for (int k = 0; k < DrumMachine::maxSteps; ++k) if (p.drums.steps[v][k] != 0) return true;
    for (int k = 0; k < BassMachine::maxSteps; ++k) if (BassMachine::isNote (p.bass.steps[k])) return true;
    return false;
}