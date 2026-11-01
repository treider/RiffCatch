#pragma once
#include "audio/AudioEngine.h"
#include "project/Exporter.h"
#include "ui/Undo.h"
#include "audio/DrumTemplates.h"
#include "audio/BassTemplates.h"
#include "audio/PitchDetector.h"
#include "ui/BassPanel.h"
#include "audio/BassSuggest.h"
#include "project/AudioImport.h"
#include "ui/ThemeStore.h"
#include "ui/TunerCard.h"
#include "project/ProjectIO.h"

// kept out of runEngineSelfTest: that function's stack frame is already large
inline void patternSelfTest (const std::function<void (bool, const juce::String&)>& check)
{
    auto peP = std::make_unique<AudioEngine>(); auto& pe = *peP;
    pe.prepareForTest (44100.0, 2);
    pe.addTrack(); pe.addTrack();
    juce::AudioBuffer<float> a (2, 44100); a.clear(); a.setSample (0, 10, 0.5f);
    pe.tracks[0].setAudio (std::move (a));
    pe.tracks[0].offset = 123;
    pe.tracks[0].name = "Guitar";
    pe.tracks[1].volume = 0.7f;
    pe.drums.steps[0][0] = 1;
    pe.drums.volume = 0.6f;
    pe.bass.steps[0] = BassMachine::pack (3, 4, false, false);
    pe.loopBars = 2;
    auto& bank = pe.patterns;
    bank.storeCurrent (pe);
    const int n = bank.addBlank (pe);
    check (n == 1 && bank.numUsed() == 2, "pattern: a new pattern is added");
    bank.current = n; bank.loadCurrent (pe);
    check (bank.pats[(size_t) n].drumRef == bank.pats[0].drumRef && bank.pats[(size_t) n].bassRef == bank.pats[0].bassRef, "pattern: a new pattern starts on the same drum and bass patterns");
    bank.selectItem (pe, PatternBank::Drums, 1); bank.selectItem (pe, PatternBank::Bass, 1);       // give it its own
    check (pe.order.size() == 1 && pe.tracks[pe.order[0]].length.load() == 0 && pe.drums.steps[0][0].load() == 0 && ! pe.bass.hasAnySteps(),
           "pattern: the new pattern has one empty track and no notes");
    for (int i = 0; i < 4; ++i) pe.addTrack();
    juce::AudioBuffer<float> b (2, 22050); b.clear(); b.setSample (1, 5, 0.25f);
    pe.tracks[pe.order[1]].setAudio (std::move (b));
    pe.tracks[pe.order[0]].volume = 0.3f;
    pe.tracks[pe.order[0]].name = "Chorus gtr";
    pe.drums.steps[1][4] = 2;
    pe.drums.volume = 0.9f;
    pe.loopBars = 3;
    bank.pats[(size_t) n].name = "Chorus";
    bank.storeCurrent (pe);
    bank.current = 0; bank.loadCurrent (pe);
    check (pe.order.size() == 2 && pe.tracks[pe.order[0]].length.load() == 44100 && std::abs (pe.tracks[pe.order[0]].data.getSample (0, 10) - 0.5f) < 1.0e-6f
           && pe.tracks[pe.order[0]].offset.load() == 123 && pe.tracks[pe.order[0]].name == "Guitar" && std::abs (pe.tracks[pe.order[1]].volume.load() - 0.7f) < 1.0e-6f
           && pe.drums.steps[0][0].load() == 1 && pe.drums.steps[1][4].load() == 0 && std::abs (pe.drums.volume.load() - 0.6f) < 1.0e-6f
           && pe.bass.hasAnySteps() && pe.loopBars.load() == 2, "pattern: switching back restores the tracks, mix, drums, bass and loop length");

    auto pdir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-pattern-test");
    pdir.deleteRecursively();
    juce::String perr;
    check (ProjectIO::save (pe, ProjectIO::fileInFolder (pdir), perr), "pattern: project saved " + perr);
    auto pe2P = std::make_unique<AudioEngine>(); auto& pe2 = *pe2P;
    pe2.prepareForTest (44100.0, 2);
    check (ProjectIO::load (pe2, ProjectIO::fileInFolder (pdir), perr), "pattern: project loaded " + perr);
    auto& b2 = pe2.patterns;
    check (b2.numUsed() == 2 && b2.current == 0 && b2.pats[1].name == "Chorus" && b2.pats[1].loopBars == 3 && b2.pats[1].tracks.size() == 5
           && b2.pats[1].tracks[1].audio.getNumSamples() == 22050 && std::abs (b2.pats[1].tracks[1].audio.getSample (1, 5) - 0.25f) < 1.0e-5f
           && std::abs (b2.pats[1].tracks[0].volume - 0.3f) < 1.0e-4f && b2.pats[1].tracks[0].name == "Chorus gtr"
           && b2.drumLib[(size_t) b2.pats[1].drumRef].steps[1][4] == 2 && std::abs (b2.pats[1].drumMix.volume - 0.9f) < 1.0e-4f && pe2.order.size() == 2,
           "pattern: patterns with their own tracks round-trip through the project file");
    b2.storeCurrent (pe2);
    b2.current = 1; b2.loadCurrent (pe2);
    check (pe2.order.size() == 5 && pe2.tracks[pe2.order[1]].length.load() == 22050 && pe2.drums.steps[1][4].load() == 2 && pe2.loopBars.load() == 3,
           "pattern: the second pattern has 5 tracks and its own content");
    b2.current = 0; b2.loadCurrent (pe2);
    check (pe2.order.size() == 2 && pe2.tracks[pe2.order[0]].length.load() == 44100, "pattern: back to the first pattern's 2 tracks");
    bank.clearPattern (pe, 0);
    check (pe.order.size() == 2 && pe.tracks[pe.order[0]].length.load() == 0 && pe.drums.hasAnySteps(), "pattern: clearing empties the audio clips but keeps the tracks and the shared drum and bass patterns");
    check (bank.remove (pe, 1) && bank.numUsed() == 1 && ! bank.remove (pe, 0), "pattern: the last pattern cannot be deleted");
    {   // libraries: shared drum and bass patterns
        auto p3P = std::make_unique<AudioEngine>(); auto& p3 = *p3P;
        p3.prepareForTest (44100.0, 2);
        p3.addTrack();
        p3.drums.steps[0][0] = 1;
        auto& bk = p3.patterns;
        bk.storeCurrent (p3);
        const int d2 = bk.newItem (p3, PatternBank::Drums, false);
        check (d2 == 1 && bk.currentRef (PatternBank::Drums) == 1 && ! p3.drums.hasAnySteps(), "library: a new drum pattern is empty and selected");
        p3.drums.steps[2][3] = 2;
        bk.selectItem (p3, PatternBank::Drums, 0);
        check (p3.drums.steps[0][0].load() == 1 && p3.drums.steps[2][3].load() == 0 && bk.drumLib[1].steps[2][3] == 2, "library: switching keeps each drum pattern's own notes");
        const int np = bk.addBlank (p3);
        check (np == 1 && bk.pats[1].drumRef == bk.pats[0].drumRef && bk.pats[1].bassRef == bk.pats[0].bassRef, "library: a new pattern starts on the same drum and bass patterns");
        bk.pats[1].drumRef = 0;
        check (bk.usedBy (PatternBank::Drums, 0).size() == 2 && ! bk.deleteItem (PatternBank::Drums, 0), "library: a drum pattern used by two patterns is shared and cannot be deleted");
        const int dup = bk.duplicate (p3, 0);
        check (dup >= 0 && bk.pats[(size_t) dup].drumRef == bk.pats[0].drumRef, "library: duplicating a pattern shares its drum pattern");
        check (bk.deleteItem (PatternBank::Drums, 1) && ! bk.itemUsed (PatternBank::Drums, 1), "library: an unused drum pattern can be deleted");
        bk.current = 1; bk.loadCurrent (p3);
        check (p3.drums.steps[0][0].load() == 1, "library: the second pattern plays the shared drum pattern");
        {   // the counter: a number that does not exist starts empty, an empty unplayed one disappears
            auto p4P = std::make_unique<AudioEngine>(); auto& p4 = *p4P;
            p4.prepareForTest (44100.0, 2);
            p4.addTrack();
            p4.drums.steps[0][0] = 1;
            auto& b4 = p4.patterns;
            b4.storeCurrent (p4);
            b4.selectItem (p4, PatternBank::Drums, 1);
            check (b4.itemUsed (PatternBank::Drums, 1) && ! p4.drums.hasAnySteps() && b4.currentRef (PatternBank::Drums) == 1, "counter: counting up to a new number starts an empty pattern");
            b4.selectItem (p4, PatternBank::Drums, 0);
            check (! b4.itemUsed (PatternBank::Drums, 1) && p4.drums.steps[0][0].load() == 1, "counter: leaving an empty, unplayed pattern removes it");
            b4.selectItem (p4, PatternBank::Drums, 3);
            p4.drums.steps[1][1] = 1;
            b4.selectItem (p4, PatternBank::Drums, 0);
            check (b4.itemUsed (PatternBank::Drums, 3) && b4.drumLib[3].steps[1][1] == 1, "counter: a pattern with notes stays in the library");
        }    }    pdir.deleteRecursively();
}
// Headless engine check, run with: RiffCatch.exe --selftest   (writes selftest.txt next to the exe)
inline juce::String runEngineSelfTest()
{
    juce::String log;
    bool ok = true;
    auto check = [&] (bool c, const juce::String& what) { log << (c ? "PASS " : "FAIL ") << what << "\n"; ok &= c; };

    auto eng = std::make_unique<AudioEngine>();
    const double sr = 48000.0;
    eng->prepareForTest (sr, 2);
    eng->bpm = 120.0; eng->loopBars = 1; eng->loopEnabled = true; eng->countInBars = 1;
    eng->clickEnabled = true;
    eng->latencyAdjustMs = 10.0f;                       // 480 samples of compensation @48k
    const int comp = eng->getCompensationSamples();
    check (comp == 480, "latency compensation = 480 samples (got " + juce::String (comp) + ")");
    const int slot = eng->addTrack();
    eng->tracks[slot].armed = true;
    eng->tracks[slot].volume = 0.0f;   // mute monitoring so only click energy is measured on the output

    const int loopLen = (int) eng->getLoopLength();     // 1 bar @120 = 96000
    check (loopLen == 96000, "loop length = 96000 samples (got " + juce::String (loopLen) + ")");
    check (eng->record(), "record() accepted");
    check (eng->getState() == AudioEngine::CountIn, "state is CountIn");
    check (eng->getCountInLength() == 96000, "count-in = 4 beats = 96000 samples");

    constexpr int block = 480;
    std::vector<float> inBuf (block), outL (block), outR (block);
    const float* ins[2] = { inBuf.data(), inBuf.data() };
    float* outs[2] = { outL.data(), outR.data() };
    juce::int64 global = 0;
    double countInEnergy = 0.0;
    bool recordingStartedAtBeat = false;
    juce::AudioIODeviceCallbackContext ctx;

    auto runBlocks = [&] (juce::int64 samples)
    {
        for (juce::int64 done = 0; done < samples; done += block)
        {
            for (int i = 0; i < block; ++i) inBuf[(size_t) i] = (float) (global + i) * 1.0e-7f;   // unique ramp
            eng->audioDeviceIOCallbackWithContext (ins, 2, outs, 2, block, ctx);
            if (eng->getState() == AudioEngine::CountIn)
                for (int i = 0; i < block; ++i) countInEnergy += std::abs (outL[(size_t) i]);
            global += block;
            if ((global / block) % 5 == 0) eng->pollRecording();
        }
    };

    runBlocks (96000 - block);
    check (eng->getState() == AudioEngine::CountIn, "still counting in just before the end");
    check (countInEnergy > 1.0, "click audible during count-in (energy " + juce::String (countInEnergy, 1) + ")");
    runBlocks (block);                                   // last count-in block
    const juce::int64 recStart = global;                 // first recorded sample = global index at start of pass 1
    runBlocks (block);                                   // first recorded block
    recordingStartedAtBeat = eng->getState() == AudioEngine::Recording && eng->getPosition() == block;
    check (recordingStartedAtBeat, "recording starts on the first sample after count-in, at bar 1 (pos " + juce::String (eng->getPosition()) + ")");

    runBlocks ((juce::int64) loopLen * 2 + 24000);       // two full passes + a partial third
    check (eng->getState() == AudioEngine::Recording, "still recording after 2.25 passes");
    eng->stop();
    runBlocks (block * 20);
    eng->pollRecording(); eng->pollRecording(); eng->pollRecording();

    auto& tr = eng->tracks[slot];
    check (tr.length.load() == loopLen, "track length = exactly one loop (" + juce::String (tr.length.load()) + ")");
    check ((int) tr.takes.size() == 2, "two completed passes kept as takes (" + juce::String ((int) tr.takes.size()) + ")");
    const float expected = (float) (recStart + loopLen + comp) * 1.0e-7f;   // pass 2 starts one loop after pass 1
    check (tr.length.load() == loopLen && std::abs (tr.data.getSample (0, 0) - expected) < 1.0e-4f,
           "last completed pass wins, shifted earlier by latency compensation; partial pass 3 discarded");
    tr.applyTake (0);
    check (std::abs (tr.data.getSample (0, 0) - (float) (recStart + comp) * 1.0e-7f) < 1.0e-4f, "take 1 can be restored");

    tr.volume = 1.0f;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_export.wav");
    juce::String err;
    check (Exporter::bounceWav (*eng, tmp, 2, err), "bounceWav succeeds " + err);
    check (tmp.getSize() > 2 * loopLen * 2 * 3, "exported WAV has 2 loops of 24-bit stereo (" + juce::String (tmp.getSize()) + " bytes)");
    tmp.deleteFile();
    if (Exporter::mp3Available)
    {
        auto mp3 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_export.mp3");
        juce::String e2;
        check (Exporter::bounceMp3 (*eng, mp3, 2, 192, e2), "bounceMp3 succeeds " + e2);
        juce::MemoryBlock mb; mp3.loadFileAsData (mb);
        const auto* b = (const unsigned char*) mb.getData();
        const bool sync = mb.getSize() > 4 && ((b[0] == 0xFF && (b[1] & 0xE0) == 0xE0) || (b[0] == 'I' && b[1] == 'D' && b[2] == '3'));
        // 2 loops @120 BPM, 1 bar each = 4 s at 192 kbps = ~96 kB
        check (sync && mb.getSize() > 60000 && mb.getSize() < 130000, "MP3 has valid frame sync and plausible size (" + juce::String ((int) mb.getSize()) + " bytes)");
        mp3.deleteFile();
    }

    // ---- hit detection + re-timing
    {
        const int len = (int) (sr * 3.0);
        juce::AudioBuffer<float> take (2, len);
        take.clear();
        const double times[] = { 0.50, 1.04, 1.50, 2.03 };      // two hits are ~35 ms off the 0.5 s grid
        for (double tm : times)
        {
            const int s0 = (int) (tm * sr);
            for (int i = 0; i < (int) (0.4 * sr) && s0 + i < len; ++i)
            {
                const float v = std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sr)
                                * std::exp (-(float) i / (float) (0.12 * sr)) * 0.7f;
                take.setSample (0, s0 + i, v); take.setSample (1, s0 + i, v);
            }
        }
        auto found = HitEditor::detectOnsets (take, sr, 6.0f);
        check (found.size() == 4, "onset detection found 4 hits (" + juce::String ((int) found.size()) + ")");
        bool near = found.size() == 4;
        for (size_t i = 0; i < found.size() && i < 4; ++i)
            near &= std::abs ((double) found[i] / sr - times[i]) < 0.01;
        check (near, "detected positions within 10 ms of the real hits");

        std::vector<Hit> hits;
        const double grid[] = { 0.50, 1.00, 1.50, 2.00 };
        for (size_t i = 0; i < found.size(); ++i) hits.push_back ({ found[i], (juce::int64) (grid[i] * sr) });
        auto edited = HitEditor::render (take, hits, sr);
        check (edited.getNumSamples() == len, "re-timed take keeps its length");
        bool onGrid = true, finite = true;
        for (int i = 0; i < 4; ++i)
        {
            const int g = (int) (grid[i] * sr);
            int first = -1;
            for (int s = g - (int) (0.06 * sr); s < g + (int) (0.06 * sr); ++s)
                if (std::abs (edited.getSample (0, s)) > 0.05f) { first = s; break; }
            onGrid &= first >= 0 && std::abs (first - g) < (int) (0.004 * sr);   // within 4 ms
        }
        for (int s = 0; s < len; ++s) finite &= std::isfinite (edited.getSample (0, s));
        check (onGrid, "every moved hit now sounds on the grid (within 4 ms)");
        check (finite, "re-timed audio has no NaN/inf");
    }

    // ---- playhead cursor / seek
    {
        AudioEngine e3;
        e3.prepareForTest (sr, 2);
        e3.loopBars = 4;
        e3.addTrack();
        std::vector<float> z (block, 0.0f), l (block), r (block);
        const float* zi[2] = { z.data(), z.data() };
        float* zo[2] = { l.data(), r.data() };
        juce::AudioIODeviceCallbackContext c3;
        auto run = [&] { e3.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, c3); };
        e3.seek (48000);
        run();
        check (e3.getPosition() == 48000 && e3.getState() == AudioEngine::Stopped, "seek while stopped moves the stopped playhead to the cursor");
        e3.play(); run();
        check (e3.getPosition() == 48000 + block, "play starts from the cursor (pos " + juce::String (e3.getPosition()) + ")");
        e3.seek (1000); run();
        check (e3.getPosition() == 1000 + block, "seek while playing jumps immediately");
        e3.stop(); run();
        check (e3.getPosition() == 1000, "stop returns the playhead to the cursor");
        e3.setCursor (0); run();
        check (e3.getPosition() == 0, "setCursor(0) rewinds");
    }

    // ---- undo building blocks
    {
        AudioEngine e2;
        e2.prepareForTest (sr, 2);
        const int sl = e2.addTrackAt (3);
        check (sl == 3 && e2.order.size() == 1, "addTrackAt reuses the requested slot");
        auto& t = e2.tracks[sl];
        juce::AudioBuffer<float> a (2, 1000), b (2, 1000);
        a.clear(); b.clear();
        for (int i = 0; i < 1000; ++i) { a.setSample (0, i, 0.25f); b.setSample (0, i, 0.5f); }
        const auto p1 = t.commitTake (a, 1000);
        check (p1.first == -1 && p1.second == 0 && t.length.load() == 1000, "first commit reports prev=-1, new=0");
        const auto p2 = t.commitTake (b, 1000);
        check (p2.first == 0 && p2.second == 1 && std::abs (t.data.getSample (0, 500) - 0.5f) < 1.0e-6f, "second commit reports prev=0, new=1");
        t.applyTake (p2.first);
        check (std::abs (t.data.getSample (0, 500) - 0.25f) < 1.0e-6f, "undo of a take restores the previous take");
        t.applyTake (p2.second);
        check (std::abs (t.data.getSample (0, 0)) < 0.05f && std::abs (t.data.getSample (0, 999)) < 0.05f, "loop seam is faded (no click at take start/end)");
        {   // region editing: copy, silence, restore
            auto reg = t.copyRegion (100, 200);
            check (reg.getNumSamples() == 100 && std::abs (reg.getSample (0, 50) - 0.5f) < 1.0e-6f, "copyRegion returns the selected samples");
            juce::AudioBuffer<float> z (2, 100); z.clear();
            t.writeRegion (100, z);
            check (t.data.getSample (0, 150) == 0.0f && std::abs (t.data.getSample (0, 99) - 0.5f) < 1.0e-6f && std::abs (t.data.getSample (0, 200) - 0.5f) < 1.0e-6f, "writeRegion silences only the selection");
            t.writeRegion (100, reg);
            check (std::abs (t.data.getSample (0, 150) - 0.5f) < 1.0e-6f, "writeRegion restores the region (undo)");
            t.writeRegion (950, reg);
            check (t.length.load() == 1000, "writeRegion never extends the take");
        }
        check (std::abs (t.data.getSample (0, 500) - 0.5f) < 1.0e-6f, "redo of a take brings the new take back");

        juce::UndoManager um;
        int value = 1;
        um.beginNewTransaction ("set");
        um.perform (new LambdaAction ([&] { value = 2; }, [&] { value = 1; }));
        check (value == 2, "perform applies the change");
        um.undo();
        check (value == 1, "undo reverts it");
        um.redo();
        check (value == 2, "redo re-applies it");

        t.silence();
        check (t.length.load() == 0 && t.takes.size() == 2, "silence() mutes the track but keeps take history");
        e2.removeTrack (sl);
        check (e2.order.empty() && ! t.active.load(), "removeTrack frees the slot");
    }

    // ---- clips: record from a later bar, whole-bar length, playback without wrap
    {
        std::vector<float> in5 (block, 0.25f), l5 (block), r5 (block);
        const float* ins5[2] = { in5.data(), in5.data() };
        float* outs5[2] = { l5.data(), r5.data() };
        juce::AudioIODeviceCallbackContext c5;

        AudioEngine e5;
        e5.prepareForTest (sr, 2);
        e5.bpm = 120.0; e5.loopBars = 4; e5.loopEnabled = false; e5.countInBars = 0; e5.clickEnabled = false;
        const int s5 = e5.addTrack();
        auto& t5 = e5.tracks[s5];
        t5.armed = true; t5.volume = 0.0f;
        const juce::int64 bar = e5.getBarLength();
        e5.setCursor (2 * bar);
        check (e5.record(), "record from bar 3 accepted");
        check (e5.getRecordStart() == 2 * bar, "recording starts at the bar of the playhead (bar 3)");
        for (juce::int64 i = 0; i * block < bar * 6 / 5; ++i) e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        e5.stop();
        for (int i = 0; i < 4; ++i) e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        for (int i = 0; i < 3; ++i) e5.pollRecording();
        check (t5.offset.load() == 2 * bar, "clip offset = recording start (" + juce::String (t5.offset.load()) + ")");
        check (t5.length.load() == bar * 2, "1.2 bars recorded are rounded up to 2 whole bars (" + juce::String (t5.length.load()) + ")");
        check (std::abs (t5.data.getSample (0, 5000) - 0.25f) < 1.0e-4f && std::abs (t5.data.getSample (0, (int) (bar * 2) - 10)) < 1.0e-6f,
               "recorded audio is at the start of the clip, padded with silence at the end");

        t5.armed = false; t5.volume = 1.0f;
        e5.setCursor (0);
        e5.play();
        double before = 0.0, after = 0.0;
        for (int i = 0; (juce::int64) (i + 1) * block < 2 * bar + 20000; ++i)
        {
            e5.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
            const juce::int64 end = (juce::int64) (i + 1) * block;
            for (int k = 0; k < block; ++k)
            {
                if (end <= 2 * bar) before += std::abs (l5[(size_t) k]);
                else if (end > 2 * bar + 1000) after += std::abs (l5[(size_t) k]);
            }
        }
        check (before == 0.0, "nothing plays before the clip start (no wrapping)");
        check (after > 100.0, "the clip plays from its start position (energy " + juce::String (after, 1) + ")");

        // loop mode: the pass window runs from the chosen bar to the loop end
        AudioEngine e6;
        e6.prepareForTest (sr, 2);
        e6.bpm = 120.0; e6.loopBars = 4; e6.loopEnabled = true; e6.countInBars = 0; e6.clickEnabled = false;
        const int s6 = e6.addTrack();
        auto& t6 = e6.tracks[s6];
        t6.armed = true; t6.volume = 0.0f;
        e6.setCursor (2 * bar);
        e6.record();
        for (juce::int64 i = 0; i < (2 * bar) / block + 4; ++i) e6.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        e6.stop();
        for (int i = 0; i < 4; ++i) e6.audioDeviceIOCallbackWithContext (ins5, 2, outs5, 2, block, c5);
        for (int i = 0; i < 3; ++i) e6.pollRecording();
        check (t6.length.load() == 2 * bar && t6.offset.load() == 2 * bar && t6.takes.size() == 1,
               "loop recording from bar 3 keeps bars 3-4 as one clip (len " + juce::String (t6.length.load()) + ")");
    }

    // ---- live bass capture: one whole loop pass, starting at the loop start
    {
        std::vector<float> zin (block, 0.0f), ol (block), orr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { ol.data(), orr.data() };
        juce::AudioIODeviceCallbackContext cc;
        auto ecP = std::make_unique<AudioEngine>(); auto& ec = *ecP;
        ec.prepareForTest (44100.0, 2);
        ec.bpm = 120.0; ec.loopBars = 1; ec.loopEnabled = true; ec.countInBars = 0; ec.clickEnabled = false;
        ec.bass.volume = 1.0f;
        ec.bass.steps[0] = BassMachine::pack (5, 4, false, false);
        ec.play();
        for (int i = 0; i < 20000 / block; ++i) ec.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);   // mid-loop
        check (ec.startBassCapture() && ec.bassCaptureState() == 1, "bass capture arms while looping");
        for (int i = 0; i < 200000 / block && ec.bassCaptureState() != 3; ++i) ec.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);
        juce::AudioBuffer<float> got;
        const bool took = ec.takeBassCapture (got);
        check (took && got.getNumSamples() == (int) ec.getLoopLength() && ec.bassCaptureState() == 0, "bass capture delivers exactly one loop");
        float first = 0.0f, quiet = 0.0f;
        for (int i = 2000; i < 6000 && took; ++i) first = std::max (first, std::abs (got.getSample (0, i)));
        for (int i = 40000; i < 44000 && took; ++i) quiet = std::max (quiet, std::abs (got.getSample (0, i)));
        check (first > 0.05f && quiet < 0.001f, "bass capture starts at the loop start and holds the pattern (" + juce::String (first, 3) + " / " + juce::String (quiet, 4) + ")");
        ec.loopEnabled = false;
        check (! ec.startBassCapture(), "bass capture needs the loop");

        // from standstill: count-in first, then the pass is captured from the loop start
        auto ecnP = std::make_unique<AudioEngine>(); auto& ecn = *ecnP;
        ecn.prepareForTest (44100.0, 2);
        ecn.bpm = 120.0; ecn.loopBars = 1; ecn.loopEnabled = true; ecn.countInBars = 1; ecn.clickEnabled = false;
        ecn.bass.volume = 1.0f;
        ecn.bass.steps[0] = BassMachine::pack (5, 4, false, false);
        check (ecn.startBassCapture() && ecn.getState() == AudioEngine::CountIn, "bass capture starts with a count-in from standstill");
        int blocks = 0;
        while (ecn.bassCaptureState() != 3 && blocks++ < 400000 / block) ecn.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cc);
        juce::AudioBuffer<float> got2;
        const bool took2 = ecn.takeBassCapture (got2);
        float f2 = 0.0f;
        for (int i = 2000; i < 6000 && took2; ++i) f2 = std::max (f2, std::abs (got2.getSample (0, i)));
        check (took2 && f2 > 0.05f && blocks * block >= 88200 + 80000 && ecn.getState() == AudioEngine::Playing, "bass capture after the count-in holds a full pass and playback continues");
    }

    // ---- bass panel: click adds a note, dragging right makes it longer
    {
        auto epP = std::make_unique<AudioEngine>(); auto& ep = *epP;
        ep.prepareForTest (44100.0, 2);
        BassPanel panel (ep);
        panel.setSize (1500, 210);
        panel.setView (8.0, 2.0, 0.0, 8.0);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p, juce::Point<float> down, bool dragged)
        {
            return juce::MouseEvent (src, p, juce::ModifierKeys::currentModifiers.withoutMouseButtons().withFlags (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &panel, &panel, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        };
        const float x0 = (float) TrackRow::waveLeft + 5.0f, y0 = (float) (BassPanel::headerH + BassPanel::shiftH + 5 * BassPanel::rowH + 5);
        const float stepPx = (float) (1500 - TrackRow::waveLeft - TrackRow::waveRightInset) / 64.0f;     // 2 s per bar, 8 s view = 4 bars of 16 steps
        panel.mouseDown (ev ({ x0, y0 }, { x0, y0 }, false));
        panel.mouseDrag (ev ({ x0 + 3.0f * stepPx, y0 }, { x0, y0 }, true));
        panel.mouseUp (ev ({ x0 + 3.0f * stepPx, y0 }, { x0, y0 }, true));
        const auto c = ep.bass.steps[0].load();
        check (BassMachine::isNote (c) && BassMachine::lenOf (c) == 4, "bass panel: dragging right lengthens the new note (len " + juce::String (BassMachine::lenOf (c)) + ")");

        // resize an existing note by its right edge; the pressed note keeps its start
        const float cellRight = (float) TrackRow::waveLeft + 4.0f * stepPx;       // note covers steps 0-3
        panel.mouseDown (ev ({ cellRight - 3.0f, y0 }, { cellRight - 3.0f, y0 }, false));
        panel.mouseDrag (ev ({ (float) TrackRow::waveLeft + 8.5f * stepPx, y0 }, { cellRight, y0 }, true));
        panel.mouseUp (ev ({ (float) TrackRow::waveLeft + 8.5f * stepPx, y0 }, { cellRight, y0 }, true));
        check (BassMachine::lenOf (ep.bass.steps[0].load()) == 9, "bass panel: dragging the right edge of a note resizes it (len " + juce::String (BassMachine::lenOf (ep.bass.steps[0].load())) + ")");

        // wipe: press on an existing note and drag over others in the row
        ep.bass.bars = 4;
        for (int k = 20; k <= 26; k += 2) ep.bass.steps[k] = BassMachine::pack (3, 1, false, false);
        ep.bass.steps[22] = BassMachine::pack (4, 1, false, false);                // other row: must survive
        const float wx = (float) TrackRow::waveLeft;
        panel.mouseDown (ev ({ wx + 20.0f * stepPx + 3.0f, y0 }, { wx, y0 }, false));
        panel.mouseDrag (ev ({ wx + 26.0f * stepPx + 3.0f, y0 }, { wx, y0 }, true));
        panel.mouseUp (ev ({ wx + 26.0f * stepPx + 3.0f, y0 }, { wx, y0 }, true));
        check (! BassMachine::isNote (ep.bass.steps[20].load()) && ! BassMachine::isNote (ep.bass.steps[24].load()) && ! BassMachine::isNote (ep.bass.steps[26].load())
               && BassMachine::isNote (ep.bass.steps[22].load()), "bass panel: dragging from a note wipes notes in that row only");

        // chromatic scale: 13 rows, every semitone from the root up an octave
        ep.bass.scale = 6; ep.bass.root = 36; ep.bass.bars = 4;
        panel.refreshFromEngine();
        const float yBottom = (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3), yTop = (float) (BassPanel::headerH + BassPanel::shiftH + 3);
        panel.mouseDown (ev ({ x0 + 50.0f * stepPx, yBottom }, { x0, yBottom }, false)); panel.mouseUp (ev ({ x0 + 50.0f * stepPx, yBottom }, { x0, yBottom }, false));
        panel.mouseDown (ev ({ x0 + 52.0f * stepPx, yTop }, { x0, yTop }, false)); panel.mouseUp (ev ({ x0 + 52.0f * stepPx, yTop }, { x0, yTop }, false));
        panel.mouseDown (ev ({ x0 + 54.0f * stepPx, (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3 - 11 * 3) }, { x0, yTop }, false));
        panel.mouseUp (ev ({ x0 + 54.0f * stepPx, (float) (BassPanel::headerH + BassPanel::shiftH + BassPanel::gridH - 3 - 11 * 3) }, { x0, yTop }, false));
        check (BassMachine::degreeOf (ep.bass.steps[50].load()) == 0 && BassMachine::degreeOf (ep.bass.steps[52].load()) == 12 && BassMachine::degreeOf (ep.bass.steps[54].load()) == 3,
               "bass panel: chromatic scale shows C2 to C3 (degrees " + juce::String (BassMachine::degreeOf (ep.bass.steps[50].load())) + ", " + juce::String (BassMachine::degreeOf (ep.bass.steps[52].load())) + ", " + juce::String (BassMachine::degreeOf (ep.bass.steps[54].load())) + ")");
        check (ep.bass.root.load() + BassMachine::semitones (6, 12) == 48, "chromatic: root C gives C2 up to C3 (MIDI 36 to 48)");
        ep.bass.scale = 0; ep.bass.root = 33; panel.refreshFromEngine();

        // plain click on a note removes it
        ep.bass.steps[40] = BassMachine::pack (3, 3, false, false);
        panel.mouseDown (ev ({ wx + 41.0f * stepPx, y0 }, { wx, y0 }, false));
        panel.mouseUp (ev ({ wx + 41.0f * stepPx, y0 }, { wx, y0 }, false));
        check (! BassMachine::isNote (ep.bass.steps[40].load()), "bass panel: clicking inside a long note removes it");
    }

    // ---- riff analysis: tempo, key, chords, rhythm, bass suggestions (synthetic guitar-like signals)
    {
        const double sr = 44100.0, spb = sr * 0.5;                  // 120 BPM
        const int barLen = (int) (spb * 4.0);
        auto tone = [&] (juce::AudioBuffer<float>& b, double f, int start, int lenS)
        {
            for (int i = 0; i < lenS && start + i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                double v = 0.0;
                for (int h = 1; h <= 4; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * f * h * t) / h;
                v *= std::exp (-t * 5.0) * std::min (1.0, t / 0.003) * 0.25;
                b.addSample (0, start + i, (float) v); b.addSample (1, start + i, (float) v);
            }
        };
        // chord riff: E5 E5 C5 D5, power chords hit on every quarter note
        juce::AudioBuffer<float> riff (2, barLen * 4);
        riff.clear();
        const double bassHz[4] = { 82.41, 82.41, 65.41, 73.42 }, fifthHz[4] = { 123.47, 123.47, 98.0, 110.0 };
        for (int bar = 0; bar < 4; ++bar)
            for (int q = 0; q < 4; ++q)
            {
                tone (riff, bassHz[bar], bar * barLen + (int) (q * spb), (int) spb);
                tone (riff, fifthHz[bar], bar * barLen + (int) (q * spb), (int) spb);
            }
        const auto ra = Analysis::analyse (riff, riff.getNumSamples(), 0, sr, spb, 4.0, 8);
        check (ra.valid && ra.bars == 4, "analysis: finds four bars of audio");
        check (std::abs (Analysis::foldNear (ra.tempo, 120.0) - 120.0) < 3.0, "analysis: tempo near 120 BPM (" + juce::String (ra.tempo, 1) + ")");
        const auto chords = ra.valid ? Analysis::detectChords (ra, ra.keys[0]) : std::vector<Analysis::Chord>();
        check (chords.size() == 4 && chords[0].rootPc == 4 && chords[1].rootPc == 4 && chords[2].rootPc == 0 && chords[3].rootPc == 2,
               "analysis: chord roots E E C D per bar" + (chords.size() == 4 ? juce::String (" (got ") + Analysis::chordName (chords[0]) + " " + Analysis::chordName (chords[1]) + " " + Analysis::chordName (chords[2]) + " " + Analysis::chordName (chords[3]) + ")" : juce::String()));
        bool onsetsOk = ra.valid;
        for (int bar = 0; bar < 4 && onsetsOk; ++bar)
            onsetsOk = ra.onsets[(size_t) bar][0] > 0.2f && ra.onsets[(size_t) bar][4] > 0.2f && ra.onsets[(size_t) bar][8] > 0.2f && ra.onsets[(size_t) bar][12] > 0.2f
                       && ra.onsets[(size_t) bar][2] < 0.2f && ra.onsets[(size_t) bar][6] < 0.2f;
        check (onsetsOk, "analysis: riff rhythm lands on steps 0, 4, 8, 12");

        // pentatonic melody: E minor pentatonic with E as the home note
        juce::AudioBuffer<float> mel (2, barLen * 4);
        mel.clear();
        const double notes[8] = { 82.41, 98.0, 110.0, 82.41, 123.47, 146.83, 82.41, 164.81 };
        for (int i = 0; i < 32; ++i) tone (mel, notes[i % 8], (int) (i * spb * 0.5), (int) (spb * 0.5));
        const auto ma = Analysis::analyse (mel, mel.getNumSamples(), 0, sr, spb, 4.0, 8);
        check (ma.valid && ma.keys[0].rootPc == 4 && ma.keys[0].scale == 0, "analysis: E minor pentatonic melody -> " + (ma.valid ? Analysis::keyName (ma.keys[0]) : juce::String ("nothing")));

        // bass suggestions follow the chords
        if (ra.valid)
        {
            const auto key = ra.keys[0];
            const auto sg = BassSuggest::suggest (ra, key, chords, 16, 4, nullptr);
            check (sg.size() >= 5, "bass suggestions: at least five lines");
            bool good = ! sg.empty();
            int expectShift = (((0 - key.rootPc) % 12) + 12) % 12; if (expectShift > 6) expectShift -= 12;
            for (const auto& s : sg)
            {
                good &= s.snap.bars == 4 && s.snap.scale == key.scale && s.snap.shift[2] == expectShift;
                int notes2 = 0;
                for (int k = 0; k < 64; ++k)
                    if (BassMachine::isNote (s.snap.steps[k]))
                    {
                        ++notes2;
                        const int row = BassMachine::degreeOf (s.snap.steps[k]) - BassMachine::degreeBase;
                        good &= BassMachine::lenOf (s.snap.steps[k]) >= 1 && row > -20 && row < 40;
                    }
                good &= notes2 >= 4;
            }
            check (good, "bass suggestions: four bars, in key, transposed per chord");
            bool pedal = false;
            for (const auto& s : sg) if (s.name == "Pedal") { int c = 0; for (int k = 0; k < 64; ++k) c += BassMachine::isNote (s.snap.steps[k]) ? 1 : 0; pedal = c == 4 && BassMachine::lenOf (s.snap.steps[0]) == 16; }
            check (pedal, "bass suggestions: Pedal is one bar-long note per bar");
        }
    }

    // ---- random bass line
    {
        juce::Random rnd (1234);
        BassMachine::Snapshot cur; cur.bars = 2; cur.scale = 0; cur.root = 33;
        BassRandom::Options o; o.grid = 1; o.density = 1.0f; o.octaves = 2; o.repeatBar = true; o.randomLength = false;
        const auto full = BassRandom::generate (cur, o, rnd);
        int notes = 0; bool grid = true, inRange = true, same = true;
        for (int k = 0; k < 32; ++k)
        {
            const auto c = full.steps[k];
            if (BassMachine::isNote (c)) { ++notes; grid &= k % 2 == 0; inRange &= BassMachine::degreeOf (c) >= 0 && BassMachine::degreeOf (c) < 10; }
            else grid &= k % 2 == 1;
            if (k >= 16) same &= full.steps[k] == full.steps[k - 16];
        }
        check (notes == 16 && grid && inRange && same, "random bass: every eighth, inside two octaves of the scale, bar 1 repeated (" + juce::String (notes) + " notes)");
        o.density = 0.3f; o.randomLength = true; o.repeatBar = false; o.grid = 2;
        int sparse = 0, dense = 0; bool lensOk = true;
        for (int run = 0; run < 20; ++run)
        {
            const auto s = BassRandom::generate (cur, o, rnd);
            for (int k = 0; k < 32; ++k)
                if (BassMachine::isNote (s.steps[k]))
                {
                    ++sparse;
                    int next = k + 1; while (next < 32 && ! BassMachine::isNote (s.steps[next])) ++next;
                    lensOk &= BassMachine::lenOf (s.steps[k]) >= 1 && k % 16 + BassMachine::lenOf (s.steps[k]) <= 16 && BassMachine::lenOf (s.steps[k]) <= next - k + (next >= 32 ? 16 : 0);
                }
        }
        o.density = 1.0f;
        for (int run = 0; run < 20; ++run) { const auto s = BassRandom::generate (cur, o, rnd); for (int k = 0; k < 32; ++k) dense += BassMachine::isNote (s.steps[k]) ? 1 : 0; }
        check (sparse < dense / 2 && lensOk, "random bass: density thins the line, random lengths stay inside the gaps (" + juce::String (sparse) + " vs " + juce::String (dense) + ")");
        BassMachine::Snapshot chromatic = cur; chromatic.scale = 6;
        const auto cs = BassRandom::generate (chromatic, o, rnd);
        bool chromOk = true; for (int k = 0; k < 32; ++k) if (BassMachine::isNote (cs.steps[k])) chromOk &= BassMachine::degreeOf (cs.steps[k]) >= 0 && BassMachine::degreeOf (cs.steps[k]) < 24;
        check (chromOk, "random bass: chromatic scale stays inside the range");
    }

    // ---- drum machine: sample-accurate steps, phase kept across loop passes
    {
        std::vector<float> zin (block, 0.0f), dl (block), dr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { dl.data(), dr.data() };
        juce::AudioIODeviceCallbackContext cd;
        auto energy = [] (const std::vector<float>& v, juce::int64 a, juce::int64 b)
        {
            double e = 0.0;
            for (juce::int64 i = a; i < b && i < (juce::int64) v.size(); ++i) e += std::abs (v[(size_t) i]);
            return e;
        };

        AudioEngine ed;
        ed.prepareForTest (sr, 2);
        ed.bpm = 120.0; ed.loopBars = 1; ed.loopEnabled = true; ed.countInBars = 0; ed.clickEnabled = false;
        ed.drums.volume = 1.0f;
        ed.drums.steps[DrumMachine::Hat][0] = 2;
        ed.drums.steps[DrumMachine::Hat][4] = 2;        // beat 2 = sample 24000
        ed.play();
        std::vector<float> cap;
        for (int i = 0; i < 96000 / block; ++i)
        {
            ed.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cd);
            cap.insert (cap.end(), dl.begin(), dl.end());
        }
        check (energy (cap, 0, 2000) > 5.0, "drum step 1 sounds at the start of the bar");
        check (energy (cap, 12000, 23990) < 0.01, "silent between hits (hat has decayed)");
        check (energy (cap, 24000, 26000) > 5.0, "drum step 5 sounds exactly on beat 2");
        check (energy (cap, 48000, 96000) < 0.01, "no hits where the pattern has none");

        // two-bar pattern over a one-bar loop: step 16 (bar 2) must sound in the second pass only
        AudioEngine ee;
        ee.prepareForTest (sr, 2);
        ee.bpm = 120.0; ee.loopBars = 1; ee.loopEnabled = true; ee.countInBars = 0; ee.clickEnabled = false;
        ee.drums.volume = 1.0f;
        ee.drums.bars = 2;
        ee.drums.steps[DrumMachine::Snare][16] = 2;
        ee.play();
        std::vector<float> cap2;
        for (int i = 0; i < 192000 / block; ++i)
        {
            ee.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cd);
            cap2.insert (cap2.end(), dl.begin(), dl.end());
        }
        check (energy (cap2, 0, 96000) < 0.01, "6-over-4 style: bar 2 of the pattern is silent in loop pass 1");
        check (energy (cap2, 96000, 98000) > 5.0, "bar 2 of the pattern sounds in loop pass 2 (phase kept across passes)");

        // export renders the same drums
        AudioEngine ex;
        ex.prepareForTest (sr, 2);
        ex.bpm = 120.0; ex.loopBars = 1; ex.loopEnabled = true;
        ex.drums.volume = 1.0f;
        ex.drums.steps[DrumMachine::Kick][0] = 2;
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_drums.wav");
        juce::String derr;
        check (Exporter::bounceWav (ex, wav, 1, derr), "export works with drums only " + derr);
        check (wav.getSize() > 96000 * 2 * 3, "exported drum loop has the expected length");
        wav.deleteFile();
    }

    // ---- track echo (tempo synced) and reverb
    {
        std::vector<float> zin (block, 0.0f), fl (block), fr (block);
        const float* zi[2] = { zin.data(), zin.data() };
        float* zo[2] = { fl.data(), fr.data() };
        juce::AudioIODeviceCallbackContext cf;
        auto runFx = [&] (bool echo, bool reverb, std::vector<float>& cap)
        {
            AudioEngine ef;
            ef.prepareForTest (sr, 2);
            ef.bpm = 120.0; ef.loopBars = 1; ef.loopEnabled = true; ef.countInBars = 0; ef.clickEnabled = false;
            const int sf = ef.addTrack();
            auto& tf = ef.tracks[sf];
            juce::AudioBuffer<float> imp (2, 96000);
            imp.clear();
            imp.setSample (0, 5000, 1.0f); imp.setSample (1, 5000, 1.0f);
            tf.setAudio (std::move (imp));
            tf.echoOn = echo; tf.reverbOn = reverb; tf.echoDiv = TrackFx::Quarter;   // 1/4 @120 = 24000 samples
            ef.play();
            for (int i = 0; i < 96000 / block; ++i)
            {
                ef.audioDeviceIOCallbackWithContext (zi, 2, zo, 2, block, cf);
                cap.insert (cap.end(), fl.begin(), fl.end());
            }
        };
        std::vector<float> dry, echo, rev;
        runFx (false, false, dry); runFx (true, false, echo); runFx (false, true, rev);
        float pkDry = 0.0f, pkEcho = 0.0f, tailRev = 0.0f;
        for (int i = 28990; i < 29010; ++i) { pkDry = juce::jmax (pkDry, std::abs (dry[(size_t) i])); pkEcho = juce::jmax (pkEcho, std::abs (echo[(size_t) i])); }
        for (int i = 8000; i < 25000; ++i) tailRev = juce::jmax (tailRev, std::abs (rev[(size_t) i]));
        check (pkDry < 1.0e-6f && pkEcho > 0.1f, "echo 1/4 repeats the hit exactly one beat later (peak " + juce::String (pkEcho, 3) + ")");
        check (std::abs (echo[5000]) > 0.3f && std::abs (dry[5000]) > 0.3f, "the dry hit is unchanged with echo on");
        check (tailRev > 0.001f, "reverb adds a tail after the hit (" + juce::String (tailRev, 4) + ")");
    }

    {   // punch in/out: the old clip stays outside the punch window, the new take replaces it inside
        Track tr;
        juce::AudioBuffer<float> oldB (2, 1000), newB (2, 1000);
        for (int i = 0; i < 1000; ++i) for (int c = 0; c < 2; ++c) { oldB.setSample (c, i, 0.25f); newB.setSample (c, i, 0.75f); }
        tr.data.setSize (2, 1000); tr.data.makeCopyOf (oldB); tr.length = 1000; tr.offset = 0;
        tr.commitTake (newB, 1000, 0, false, 0, 400, 600);
        const auto& d = tr.data;
        check (tr.length.load() == 1000, "punch keeps the clip length");
        check (std::abs (d.getSample (0, 100) - 0.25f) < 1.0e-5f && std::abs (d.getSample (0, 800) - 0.25f) < 1.0e-5f, "punch: old audio kept outside the window");
        check (std::abs (d.getSample (0, 500) - 0.75f) < 1.0e-3f, "punch: new audio inside the window");
    }

    {   // audio import: a mono 44.1 kHz WAV becomes stereo at the device rate
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch_import_test.wav");
        f.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 44100.0, 1, 16, {}, 0));
            juce::AudioBuffer<float> b (1, 44100);
            for (int i = 0; i < 44100; ++i) b.setSample (0, i, 0.5f * std::sin (i * 0.05f));
            if (w) w->writeFromAudioSampleBuffer (b, 0, 44100);
        }
        juce::AudioBuffer<float> out; double fr = 0; juce::String err;
        const bool okRead = AudioImport::readFile (f, 48000.0, out, fr, err);
        check (okRead, "import reads the WAV");
        check (out.getNumChannels() == 2 && std::abs (out.getNumSamples() - 48000) <= 2, "import: stereo, resampled to 48 kHz (" + juce::String (out.getNumSamples()) + " samples)");
        check (out.getNumSamples() > 100 && std::abs (out.getSample (0, 5000) - out.getSample (1, 5000)) < 1.0e-6f && std::abs (out.getSample (0, 5000)) > 0.01f, "import: mono copied to both channels");
        check (! AudioImport::readFile (juce::File ("Z:/does/not/exist.wav"), 48000.0, out, fr, err), "import: missing file is refused");
        f.deleteFile();
    }

    {   // drum templates
        int n = 0;
        const auto* t = DrumTemplates::all (n);
        check (n == 21, "21 drum templates (got " + juce::String (n) + ")");
        {
            bool lengthsOk = true;
            for (int i = 0; i < n; ++i)
            {
                const juce::String sig (t[i].sig);
                const int num = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue(), den = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
                const int steps = (int) std::lround (num * 4.0 * 4.0 / den);
                for (int b = 0; b < 2; ++b)
                    for (int v = 0; v < DrumMachine::numVoices; ++v)
                        lengthsOk &= (int) std::strlen (t[i].bar[b][v]) == steps;
            }
            check (lengthsOk, "every template row has exactly one character per step of its time signature");
        }
        bool lenOk = true, kickOk = true;
        for (int i = 0; i < n; ++i)
            for (int b = 0; b < 2; ++b)
            {
                for (int v = 0; v < DrumMachine::numVoices; ++v) lenOk &= juce::String (t[i].sig) != "4/4" || juce::String (t[i].bar[b][v]).length() == DrumMachine::stepsPerBar;
                kickOk &= t[i].bar[b][DrumMachine::Kick][0] == 'x';
            }
        check (lenOk, "every template row has 16 steps");
        check (kickOk, "every template has a kick on the one");
        DrumMachine::Snapshot cur;
        cur.bars = 1;
        cur.steps[DrumMachine::Cymbal][5] = 1;
        auto a = DrumTemplates::apply (cur, 0);
        check (a.bars == 2, "a template makes the pattern two bars");
        check (a.steps[DrumMachine::Snare][4] == 2 && a.steps[DrumMachine::Snare][12] == 2, "Rock 1: accented snare on 2 and 4");
        check (a.steps[DrumMachine::Cymbal][5] == 0, "old steps are replaced");
        cur.bars = 4;
        auto b4 = DrumTemplates::apply (cur, 0);
        check (b4.bars == 4 && std::memcmp (&b4.steps[1][0], &b4.steps[1][32], 16) == 0, "longer patterns repeat the two bars");
    }

    {   // project files from other people: hardening
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-security-test");
        root.deleteRecursively();
        const auto pdir = root.getChildFile ("p");
        check (ProjectIO::isSafeTrackFile (pdir, "track1.wav"), "plain track file name is accepted");
        check (! ProjectIO::isSafeTrackFile (pdir, "..\\evil.wav") && ! ProjectIO::isSafeTrackFile (pdir, "../evil.wav")
               && ! ProjectIO::isSafeTrackFile (pdir, "C:\\Windows\\x.wav") && ! ProjectIO::isSafeTrackFile (pdir, "sub\\a.wav")
               && ! ProjectIO::isSafeTrackFile (pdir, "track1.exe") && ! ProjectIO::isSafeTrackFile (pdir, ""), "paths, traversal and other extensions are rejected");

        // a hand-made project.json with hostile values
        pdir.createDirectory();
        const auto longNotes = juce::String::repeatedString ("x", 50000);
        pdir.getChildFile ("p.riffcatch").replaceWithText (
            "{\"tracks\":[{\"name\":\"" + juce::String::repeatedString ("n", 500) + "\",\"volume\":1e9,\"pan\":-50,\"input\":-5,\"offsetMs\":1e15,\"file\":\"..\\\\..\\\\x.wav\"}],"
            "\"bpm\":99999,\"timeSignature\":\"99/3\",\"notes\":\"" + longNotes + "\"}");
        auto he = std::make_unique<AudioEngine>();
        he->prepareForTest (48000.0, 2);
        juce::String herr;
        check (ProjectIO::load (*he, pdir.getChildFile ("p.riffcatch"), herr), "a hostile project file still loads (values are clamped)");
        const auto& ht = he->tracks[he->order.front()];
        check (ht.volume.load() <= 4.0f && std::abs (ht.pan.load()) <= 1.0f && ht.inputCode.load() >= 0 && ht.name.length() <= ProjectIO::maxNameLength
               && std::abs (ht.offset.load()) <= 48000 * 60 * 20 && ht.length.load() == 0, "hostile track values are clamped and the escaping file is ignored");
        check (he->projectNotes.length() == ProjectIO::maxNotesLength && he->bpm.load() <= 300.0 && he->tsNum.load() <= 16 && he->tsDen.load() == 4,
               "notes, tempo and time signature are limited");

        // saving never touches a folder with someone else's files
        const auto foreign = root.getChildFile ("foreign");
        foreign.createDirectory();
        foreign.getChildFile ("track1.wav").replaceWithText ("my precious file");
        juce::String serr;
        auto se = std::make_unique<AudioEngine>();
        se->prepareForTest (48000.0, 2);
        se->addTrack();
        se->tracks[se->order.front()].setAudio (juce::AudioBuffer<float> (2, 4800));
        check (! ProjectIO::save (*se, foreign.getChildFile ("foreign.riffcatch"), serr) && foreign.getChildFile ("track1.wav").loadFileAsString() == "my precious file", "saving into a foreign non-empty folder is refused");
        // a real project folder: stale track files are removed, unrelated files are kept
        const auto mine = root.getChildFile ("mine");
        check (ProjectIO::save (*se, ProjectIO::fileInFolder (mine), serr), "saving into a new folder works");
        mine.getChildFile ("track7.wav").replaceWithText ("not written by RiffCatch");
        se->tracks[se->order.front()].clearAudio();
        const bool saved2 = ProjectIO::save (*se, ProjectIO::fileInFolder (mine), serr);
        check (saved2 && ! mine.getChildFile ("track1.wav").existsAsFile() && mine.getChildFile ("track7.wav").existsAsFile(),
               "only the files RiffCatch wrote are cleaned up (saved " + juce::String ((int) saved2) + ", err " + serr + ", track1 exists " + juce::String ((int) mine.getChildFile ("track1.wav").existsAsFile()) + ", track7 exists " + juce::String ((int) mine.getChildFile ("track7.wav").existsAsFile()) + ")");
        check (! ProjectIO::save (*se, mine.getChildFile ("other.riffcatch"), serr) && ! mine.getChildFile ("other.riffcatch").existsAsFile(),
               "a second project in the same folder is refused");
        check (ProjectIO::isProjectFile (juce::File ("C:\\music\\song1\\song1.riffcatch")) && ! ProjectIO::isProjectFile (juce::File ("C:\\music\\song1\\project.json")),
               "only .riffcatch files count as projects");
        root.deleteRecursively();
    }
    {   // Catch: always-on ring
        auto ce = std::make_unique<AudioEngine>();
        ce->prepareForTest (48000.0, 2);
        const int total = 48000 * 10;
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i) { l[(size_t) i] = (float) i / (float) total; r[(size_t) i] = -(float) i / (float) total; }
        ce->pushCatchForTest (l.data(), r.data(), total);
        juce::AudioBuffer<float> got;
        check (ce->readCatch (got, 2.0) && got.getNumSamples() == 96000, "catch returns the requested 2 s");
        check (std::abs (got.getSample (0, 95999) - l[(size_t) total - 1]) < 1e-6f && std::abs (got.getSample (1, 0) - r[(size_t) total - 96000]) < 1e-6f,
               "catch holds the newest samples, both channels");
        check (ce->readCatch (got, 60.0) && got.getNumSamples() == total, "catch returns only what was recorded so far");
        // wraps around the 70 s ring
        for (int k = 0; k < 8; ++k) ce->pushCatchForTest (l.data(), r.data(), total);
        check (ce->readCatch (got, 30.0) && got.getNumSamples() == 30 * 48000 && std::abs (got.getSample (0, 30 * 48000 - 1) - l[(size_t) total - 1]) < 1e-6f, "catch keeps working after the ring wraps");
    }
    {   // looper building block: record starts exactly when the loop wraps
        auto le = std::make_unique<AudioEngine>();
        le->prepareForTest (48000.0, 1);
        le->bpm = 120.0; le->loopBars = 1; le->loopEnabled = true; le->countInBars = 0;
        const int s = le->addTrack();
        le->tracks[s].volume = 0.0f;
        le->play();
        std::vector<float> in (512, 0.1f), outL (512), outR (512);
        const float* ins[1] = { in.data() };
        float* outs[2] = { outL.data(), outR.data() };
        juce::AudioIODeviceCallbackContext ctx;
        auto run = [&] (int blocks) { for (int i = 0; i < blocks; ++i) { le->audioDeviceIOCallbackWithContext (ins, 1, outs, 2, 512, ctx); if (i % 8 == 0) le->pollRecording(); } };
        run (4);
        check (! le->recordAtNextLoop(), "overdub needs an armed track");
        le->tracks[s].armed = true;
        check (le->recordAtNextLoop(), "overdub can be queued while playing");
        run (100);                                                // 51200 samples, the loop is 96000
        check (le->getState() == AudioEngine::Playing, "still playing before the loop ends");
        run (100);                                                // past the loop end
        check (le->getState() == AudioEngine::Recording, "recording started at the loop wrap");
        run (200);                                                // the whole recorded pass
        for (int i = 0; i < 4; ++i) le->pollRecording();
        auto& lt = le->tracks[s];
        check (lt.length.load() == le->getLoopLength() && std::abs (lt.data.getSample (0, 48000) - 0.1f) < 0.01f,
               "the queued overdub recorded a full pass (length " + juce::String ((juce::int64) lt.length.load()) + ", mid sample " + juce::String (lt.data.getSample (0, 48000), 3) + ")");
        le->stop();
    }
    {   // time signatures
        auto tsP = std::make_unique<AudioEngine>(); auto& ts = *tsP;
        ts.prepareForTest (48000.0, 1);
        ts.bpm = 120.0;
        const double spb = 24000.0;
        check (ts.getBarLength() == (juce::int64) (4 * spb), "4/4 bar = 4 quarters");
        ts.setTimeSignature (3, 4);
        check (ts.getBarLength() == (juce::int64) (3 * spb) && ts.drums.barSteps.load() == 12, "3/4 bar = 3 quarters, 12 drum steps");
        ts.setTimeSignature (6, 8);
        check (ts.getBarLength() == (juce::int64) (3 * spb) && ts.getPulsesPerBar() == 6 && std::abs (ts.getPulseSamples() - spb / 2) < 1e-6, "6/8 bar = 3 quarters, 6 eighth pulses");
        ts.setTimeSignature (7, 8);
        check (ts.getBarLength() == (juce::int64) (3.5 * spb) && ts.drums.barSteps.load() == 14, "7/8 bar = 3.5 quarters, 14 drum steps");
        ts.setTimeSignature (5, 4);
        check (ts.getBarLength() == (juce::int64) (5 * spb) && ts.drums.barSteps.load() == 20, "5/4 bar = 5 quarters, 20 drum steps");
        ts.loopBars = 2;
        check (ts.getLoopLength() == (juce::int64) (10 * spb), "loop length follows the time signature");

        // drum pattern survives a layout change as far as it fits
        auto dm4P = std::make_unique<AudioEngine>(); auto& dm4 = *dm4P;
        dm4.setTimeSignature (4, 4);
        dm4.drums.steps[0][0] = 2; dm4.drums.steps[0][12] = 1; dm4.drums.steps[0][16] = 1;
        dm4.drums.bars = 2;
        dm4.setTimeSignature (3, 4);
        check (dm4.drums.steps[0][0].load() == 2 && dm4.drums.steps[0][12].load() == 1 && dm4.drums.steps[0][13].load() == 0, "3/4: bar 2 starts at step 12 (the cut 4/4 tail is dropped)");
        dm4.setTimeSignature (4, 4);
        check (dm4.drums.barSteps.load() == 16, "back to 4/4: 16 steps");

        // time signature in the project file
        auto peP = std::make_unique<AudioEngine>(); auto& pe = *peP;
        pe.prepareForTest (44100.0, 2);
        pe.addTrack();
        pe.setTimeSignature (6, 8);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-ts-test");
        dir.deleteRecursively();
        juce::String err;
        check (ProjectIO::save (pe, ProjectIO::fileInFolder (dir), err), "time signature test: saved");
        auto pe2P = std::make_unique<AudioEngine>(); auto& pe2 = *pe2P;
        pe2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (pe2, ProjectIO::fileInFolder (dir), err) && pe2.tsNum.load() == 6 && pe2.tsDen.load() == 8 && pe2.drums.barSteps.load() == 12, "time signature round-trips through the project file");
        dir.deleteRecursively();
    }
    {   // bass synth
        const double sr = 44100.0, stepLen = sr * 60.0 / 120.0 / 4.0;
        auto bmP = std::make_unique<BassMachine>(); auto& bm = *bmP;
        bm.prepare (sr);
        bm.wave = 2; bm.sub = 0.0f; bm.drive = 0.0f; bm.cutoff = 2000.0f; bm.envAmt = 0.0f; bm.sustain = 1.0f; bm.reso = 0.0f; bm.glide = 0.0f;
        bm.root = 33; bm.scale = 0;
        check (BassMachine::semitones (0, 5) == 12 && BassMachine::semitones (0, -1) == -2 && BassMachine::semitones (2, 7) == 12, "bass scales: degrees map to semitones, also below the root");
        check (BassMachine::nearestDegree (0, 7) == 3 && BassMachine::nearestDegree (3, 4) == 2, "bass: nearest degree for fifth and major third");
        const auto cell = BassMachine::pack (-2, 5, true, true);
        check (BassMachine::degreeOf (cell) == -2 && BassMachine::lenOf (cell) == 5 && BassMachine::slideOf (cell) && BassMachine::accentOf (cell), "bass cell packs degree, length and flags");

        bm.steps[2] = BassMachine::pack (5, 4, false, false);          // A2 on step 3
        std::vector<float> L (44100, 0.0f), R (44100, 0.0f);
        bm.process (0, 44100, stepLen, 1.0f, L.data(), R.data(), true);
        float pre = 0.0f, post = 0.0f;
        for (int i = 0; i < 11000; ++i) pre = std::max (pre, std::abs (L[(size_t) i]));
        for (int i = 11025; i < 16000; ++i) post = std::max (post, std::abs (L[(size_t) i]));
        check (pre == 0.0f && post > 0.05f, "bass note starts at its step (sample 11025)");
        float tail = 0.0f;
        for (int i = 40000; i < 44100; ++i) tail = std::max (tail, std::abs (L[(size_t) i]));
        check (tail < 0.001f, "bass note ends after its length");
        PitchDetector det;
        const auto pr = det.detect (L.data() + 14000, 4096, sr);
        const double cents = pr.valid ? 1200.0 * std::log2 (pr.freq / 110.0) : 999.0;
        check (pr.valid && std::abs (cents) < 25.0, "bass plays A2 (110 Hz), off by " + juce::String (cents, 1) + " cents");

        // chunked rendering gives the same audio as one block
        auto bm2P = std::make_unique<BassMachine>(); auto& bm2 = *bm2P;
        bm2.copyFrom (bm); bm2.prepare (sr);
        std::vector<float> L2 (44100, 0.0f), R2 (44100, 0.0f);
        for (int p = 0; p < 44100; p += 777)
        {
            const int n = std::min (777, 44100 - p);
            bm2.process (p, n, stepLen, 1.0f, L2.data() + p, R2.data() + p, true);
        }
        float diff = 0.0f;
        for (int i = 0; i < 44100; ++i) diff = std::max (diff, std::abs (L[(size_t) i] - L2[(size_t) i]));
        check (diff < 1e-4f, "bass renders identically in small chunks");

        // transposition per bar and bar layout
        bm.bars = 2; bm.shift[1] = 5;
        check (bm.midiFor (cell, 1) == bm.midiFor (cell, 0) + 5, "bass: per-bar transposition");
        bm.barSteps = 16; bm.steps[16] = BassMachine::pack (0, 2, false, false);
        bm.setBarSteps (12);
        check (BassMachine::isNote (bm.steps[12].load()) && ! BassMachine::isNote (bm.steps[16].load()), "bass: 3/4 moves bar 2 to step 12");
        bm.setBarSteps (16);

        // templates
        BassMachine::Snapshot cur;
        auto root = BassTemplates::apply (cur, 0, nullptr, 4);
        int cnt = 0; for (int k = 0; k < 16; ++k) cnt += BassMachine::isNote (root.steps[k]) ? 1 : 0;
        check (cnt == 4 && BassMachine::degreeOf (root.steps[4]) == 0, "bass template: root pulse = four root notes");
        DrumMachine::Snapshot ds;
        ds.bars = 2; ds.steps[DrumMachine::Kick][0] = 2; ds.steps[DrumMachine::Kick][8] = 1; ds.steps[DrumMachine::Kick][20] = 1;
        auto lk = BassTemplates::apply (cur, BassTemplates::lockToKickId, &ds, 4);
        check (lk.bars == 2 && BassMachine::isNote (lk.steps[0]) && BassMachine::isNote (lk.steps[8]) && BassMachine::isNote (lk.steps[20])
               && ! BassMachine::isNote (lk.steps[4]) && BassMachine::accentOf (lk.steps[0]) && BassMachine::lenOf (lk.steps[0]) == 4, "bass template: lock to kick follows the drums");
        auto pg = BassTemplates::applyProgression (root, 1);
        check (pg.bars == 4 && pg.shift[1] == 5 && pg.shift[2] == 7 && BassMachine::isNote (pg.steps[3 * 16]), "bass chords: I-IV-V-IV extends to four bars");
        for (int t = 0; t < BassTemplates::numPatterns; ++t)
        {
            auto s = BassTemplates::apply (cur, t, nullptr, 4);
            bool inRange = true;
            for (int k = 0; k < 16; ++k)
                if (BassMachine::isNote (s.steps[k])) { const int r = BassMachine::degreeOf (s.steps[k]) - BassMachine::degreeBase; inRange &= r >= 0 && r < BassMachine::numRows; }
            check (inRange, juce::String ("bass template stays inside the grid: ") + BassTemplates::name (t));
        }

        // project round trip and export
        auto beP = std::make_unique<AudioEngine>(); auto& be = *beP;
        be.prepareForTest (44100.0, 2);
        be.addTrack();
        be.bass.root = 31; be.bass.scale = 4; be.bass.bars = 2; be.bass.cutoff = 777.0f; be.bass.shift[1] = 7; be.bass.mute = true;
        be.bass.steps[3] = BassMachine::pack (4, 3, true, true);
        auto bdir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-bass-test");
        bdir.deleteRecursively();
        juce::String berr;
        check (ProjectIO::save (be, ProjectIO::fileInFolder (bdir), berr), "bass test: project saved");
        auto be2P = std::make_unique<AudioEngine>(); auto& be2 = *be2P;
        be2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (be2, ProjectIO::fileInFolder (bdir), berr), "bass test: project loaded");
        check (be2.bass.snapshot() == be.bass.snapshot() && std::abs (be2.bass.cutoff.load() - 777.0f) < 0.01f && be2.bass.mute.load(), "bass pattern and sound round-trip through the project file");
        bdir.deleteRecursively();

        auto bxP = std::make_unique<AudioEngine>(); auto& bx = *bxP;
        bx.prepareForTest (44100.0, 2);
        bx.bass.volume = 1.0f;
        bx.bass.steps[0] = BassMachine::pack (0, 4, false, false);
        auto bwav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc_selftest_bass.wav");
        check (Exporter::bounceWav (bx, bwav, 1, berr), "export works with bass only " + berr);
        bwav.deleteFile();

        // bass insert effects: echo repeats the note after the dry note has ended, reverb adds a tail
        auto tailEnergy = [&] (bool echo, bool rev)
        {
            auto fP = std::make_unique<BassMachine>(); auto& f = *fP;
            f.prepare (sr); f.fx.prepare (sr);
            f.wave = 2; f.sub = 0.0f; f.drive = 0.0f; f.sustain = 1.0f;
            f.steps[0] = BassMachine::pack (5, 1, false, false);
            f.echoOn = echo; f.reverbOn = rev; f.echoDiv = TrackFx::Quarter;
            std::vector<float> a (44100, 0.0f), b (44100, 0.0f);
            for (int p = 0; p < 44100; p += 1024)
            {
                const int n = std::min (1024, 44100 - p);
                f.render (p, n, stepLen, 1.0f, a.data() + p, b.data() + p, true, sr * 0.5);
            }
            float e = 0.0f;
            for (int i = 24000; i < 44100; ++i) e += std::abs (a[(size_t) i]);
            return e;
        };
        const float dry = tailEnergy (false, false), wet = tailEnergy (true, false), rv = tailEnergy (false, true);
        check (wet > dry + 5.0f && rv > dry + 1.0f, "bass echo and reverb add a tail (dry " + juce::String (dry, 2) + ", echo " + juce::String (wet, 2) + ", reverb " + juce::String (rv, 2) + ")");
    }
    {   // silence trimming
        juce::AudioBuffer<float> b (2, 48000);
        b.clear();
        for (int i = 20000; i < 30000; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.5f); }
        juce::int64 a = 0, z = 0;
        check (SilenceTrim::findRange (b, 48000, 48000.0, a, z), "silence trim finds audio");
        check (a > 19000 && a <= 20000 && z >= 30000 && z < 33000, "silence trim keeps the audible part with a little padding (" + juce::String (a) + ".." + juce::String (z) + ")");
        juce::AudioBuffer<float> full (2, 1000);
        for (int i = 0; i < 1000; ++i) { full.setSample (0, i, 0.5f); full.setSample (1, i, 0.5f); }
        check (! SilenceTrim::findRange (full, 1000, 48000.0, a, z), "nothing to trim when the clip is full");
        juce::AudioBuffer<float> empty (2, 1000);
        empty.clear();
        check (! SilenceTrim::findRange (empty, 1000, 48000.0, a, z), "all-silent clips are left alone");
    }
    {   // tuner: pitch detection on synthetic guitar-range tones, through the real input ring
        for (double sr : { 44100.0, 48000.0 })
            for (double f : { 82.41, 110.0, 196.0, 329.63, 440.0, 659.26 })
            {
                AudioEngine te;
                te.prepareForTest (sr, 1);
                std::vector<float> sig (4096);
                for (int i = 0; i < 4096; ++i)
                    sig[(size_t) i] = (float) (0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / sr)
                                             + 0.15 * std::sin (4.0 * juce::MathConstants<double>::pi * f * i / sr));
                te.pushTunerSamples (sig.data(), 4096);
                std::vector<float> got (4096);
                te.readTunerSamples (got.data(), 4096);
                PitchDetector det;
                const auto r = det.detect (got.data(), 4096, sr);
                const double cents = r.valid ? 1200.0 * std::log2 (r.freq / f) : 999.0;
                check (r.valid && std::abs (cents) < 3.0, "tuner detects " + juce::String (f, 2) + " Hz at " + juce::String ((int) sr) + " (off by " + juce::String (cents, 1) + " cents)");
            }
        std::vector<float> quiet (4096, 0.0f);
        PitchDetector det2;
        check (! det2.detect (quiet.data(), 4096, 44100.0).valid, "tuner ignores silence");
        check (TunerCard::noteName (69) == "A" && TunerCard::octaveOf (69) == 4 && TunerCard::noteName (40) == "E" && TunerCard::octaveOf (40) == 2, "tuner note names (A4, E2)");
    }
    {   // project notes survive save and load
        auto neP = std::make_unique<AudioEngine>(); auto& ne = *neP;
        ne.prepareForTest (44100.0, 2);
        ne.addTrack();
        ne.projectNotes = "E minor, capo 2\nidea: slower";
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffcatch-notes-test");
        dir.deleteRecursively();
        juce::String err;
        check (ProjectIO::save (ne, ProjectIO::fileInFolder (dir), err), "notes test: project saved");
        auto ne2P = std::make_unique<AudioEngine>(); auto& ne2 = *ne2P;
        ne2.prepareForTest (44100.0, 2);
        check (ProjectIO::load (ne2, ProjectIO::fileInFolder (dir), err), "notes test: project loaded");
        check (ne2.projectNotes == ne.projectNotes, "project notes round-trip");
        dir.deleteRecursively();
    }
    patternSelfTest (check);
    {   // custom themes
        const auto dk = Theme::generate ("selftest-theme", juce::Colour (0xffea580c), juce::Colour (0xfffbbf24), true);
        const auto lt = Theme::generate ("selftest-light", juce::Colour (0xff2563eb), juce::Colour (0xff06b6d4), false);
        auto contrast = [] (juce::uint32 a, juce::uint32 b) { return std::abs (juce::Colour (a).getPerceivedBrightness() - juce::Colour (b).getPerceivedBrightness()); };
        check (contrast (dk.background, dk.text) > 0.6f, "generated dark theme: text contrasts with background");
        check (contrast (lt.background, lt.text) > 0.6f, "generated light theme: text contrasts with background");
        check (juce::Colour (dk.background).getPerceivedBrightness() < juce::Colour (lt.background).getPerceivedBrightness(), "dark base is darker than light base");
        check (ThemeStore::save (dk), "custom theme saved to disk");
        ThemeStore::reload();
        const int idx = ThemeStore::indexOf ("selftest-theme");
        check (idx >= Theme::builtinCount, "custom theme found after reload");
        if (idx >= 0)
        {
            const auto& r = Theme::presets[(size_t) idx];
            check (r.primary == dk.primary && r.accent == dk.accent && r.background == dk.background && r.surface == dk.surface && r.text == dk.text,
                   "custom theme round-trips through JSON");
            check (ThemeStore::remove (idx) && ThemeStore::indexOf ("selftest-theme") < 0, "custom theme deleted");
        }
        check (Theme::numPresets() == Theme::builtinCount, "only built-in themes remain");
    }

    log << (ok ? "ALL OK" : "FAILURES") << "\n";



    return log;
}
