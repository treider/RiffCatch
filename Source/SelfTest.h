#pragma once
#include "audio/AudioEngine.h"
#include "project/Exporter.h"

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

    log << (ok ? "ALL OK" : "FAILURES") << "\n";
    return log;
}
