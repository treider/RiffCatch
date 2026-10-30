#pragma once
#include "audio/AudioEngine.h"
#include "project/Exporter.h"
#include "ui/Undo.h"

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
        check (p2.first == 0 && p2.second == 1 && std::abs (t.data.getSample (0, 5) - 0.5f) < 1.0e-6f, "second commit reports prev=0, new=1");
        t.applyTake (p2.first);
        check (std::abs (t.data.getSample (0, 5) - 0.25f) < 1.0e-6f, "undo of a take restores the previous take");
        t.applyTake (p2.second);
        check (std::abs (t.data.getSample (0, 5) - 0.5f) < 1.0e-6f, "redo of a take brings the new take back");

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

    log << (ok ? "ALL OK" : "FAILURES") << "\n";
    return log;
}
