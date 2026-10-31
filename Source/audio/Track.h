#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>
#include "HitEditor.h"
#include "TrackFx.h"

// Fader position (0..1) <-> linear gain. Linear in dB from -60 to +6, 0 = -inf.
inline float faderToGain (float pos)
{
    return pos <= 0.001f ? 0.0f : juce::Decibels::decibelsToGain (-60.0f + pos * 66.0f);
}
inline float gainToFader (float g)
{
    return g <= 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (g) + 60.0f) / 66.0f);
}

// One stereo track. Live parameters are atomics (written by UI, read by audio thread).
// Audio buffer is guarded by a SpinLock: the audio thread only try-locks, the UI thread
// holds it only while (re)allocating or swapping buffers.
class Track
{
public:
    static constexpr int maxRecordSeconds = 300;

    // --- lock-free live parameters ---
    std::atomic<float> volume { 1.0f };   // linear gain
    std::atomic<float> pan { 0.0f };      // -1..+1
    std::atomic<bool> mute { false }, solo { false }, armed { false };
    std::atomic<bool> echoOn { false }, reverbOn { false };
    std::atomic<float> mixPeakL { 0.0f }, mixPeakR { 0.0f };     // peaks for the mixer strip (the track row has its own)
    std::atomic<int> echoDiv { TrackFx::Quarter };
    TrackFx fx;                                        // audio-thread state (delay line, reverb)
    std::atomic<bool> active { false };            // slot in use
    std::atomic<unsigned> revision { 0 };          // bumped when audio is replaced/truncated (waveform cache)

    // --- loop recording (staging buffers: a pass never overwrites the take until it completes) ---
    std::atomic<bool> recActive { false };         // armed at record start
    std::atomic<bool> recToStage { false };        // loop-mode session
    std::atomic<int> writeStage { 0 }, completedStage { 0 };
    std::atomic<unsigned> stageSeq { 0 };          // completed passes
    std::atomic<juce::int64> stagePos { 0 };       // samples written in current pass
    juce::AudioBuffer<float> stage[2];
    // message-thread session bookkeeping + take history
    unsigned seenSeq = 0; bool recSession = false; int sessionCommits = 0; int idleTicks = 0;
    static constexpr int maxTakes = 10;
    std::vector<juce::AudioBuffer<float>> takes;
    int currentTake = -1;

    // hit (transient) editing, message thread only. Non-destructive: editSource keeps the unedited take.
    bool editing = false;
    std::vector<Hit> hits;
    juce::AudioBuffer<float> editSource;
    unsigned editGen = 0;     // bumped when the edit baseline changes; older undo actions then become no-ops
    void resetEdit() { editing = false; hits.clear(); editSource.setSize (0, 0); ++editGen; }

    void beginEdit (double sr, float riseDb)
    {
        const auto len = (int) length.load();
        if (len <= 0) return;
        editSource.setSize (2, len);
        {
            juce::SpinLock::ScopedLockType l (lock);
            for (int c = 0; c < 2; ++c) editSource.copyFrom (c, 0, data, c, 0, len);
        }
        hits.clear();
        ++editGen;
        for (auto p : HitEditor::detectOnsets (editSource, sr, riseDb)) hits.push_back ({ p, p });
        editing = true;
    }

    // Re-render the take from editSource + hits and swap it in.
    void applyEdit (double sr)
    {
        if (editSource.getNumSamples() == 0) return;
        auto out = HitEditor::render (editSource, hits, sr);
        juce::SpinLock::ScopedLockType l (lock);
        data.setSize (2, out.getNumSamples(), false, false, true);
        for (int c = 0; c < 2; ++c) data.copyFrom (c, 0, out, c, 0, out.getNumSamples());
        length = out.getNumSamples();
        ++revision;
    }
    std::atomic<int> offset { 0 };        // where the clip starts on the timeline, in samples (0 = bar 1)
    std::vector<int> takeOffsets;         // clip offset of each take (parallel to `takes`)
    void setOffset (int v) { offset = v; if (currentTake >= 0 && currentTake < (int) takeOffsets.size()) takeOffsets[(size_t) currentTake] = v; }
    std::atomic<int> inputCode { 0 };     // 0..N-1 = mono input N, 100+i = stereo pair (i, i+1)
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };   // peak since last UI read

    // --- audio data ---
    juce::SpinLock lock;
    juce::AudioBuffer<float> data;                   // 2 channels
    std::atomic<juce::int64> length { 0 };           // valid samples

    // --- message thread only ---
    juce::String name;

    // audio-thread-only smoothing state
    float curGainL = 0.0f, curGainR = 0.0f;

    void resetToDefaults (int index)
    {
        name = "Track " + juce::String (index + 1);
        volume = 1.0f; pan = 0.0f; mute = false; solo = false; armed = false; inputCode = 0; offset = 0; echoOn = false; reverbOn = false; echoDiv = TrackFx::Quarter;
        clearAudio();
    }

    void clearAudio()
    {
        takes.clear(); takeOffsets.clear(); currentTake = -1; resetEdit();
        juce::SpinLock::ScopedLockType l (lock);
        length = 0;
        ++revision;
    }

    // Allocate capacity and truncate the take at fromPos.
    void prepareRecording (juce::int64 fromPos, double sampleRate)
    {
        takes.clear(); takeOffsets.clear(); currentTake = -1; resetEdit();
        juce::SpinLock::ScopedLockType l (lock);
        const auto cap = (int) (sampleRate * maxRecordSeconds);
        if (data.getNumSamples() < cap)
            data.setSize (2, cap, true, true, false);
        length = juce::jmin<juce::int64> (length.load(), fromPos);
        ++revision;
    }

    void prepareStage (int len)
    {
        for (auto& s : stage) s.setSize (2, len, false, true, true);
        writeStage = 0; stagePos = 0;
    }

    // Replace track audio with a completed record pass; older audio is kept as a take.
    // shiftEarlier: input/output latency compensation. Circular when the pass is the whole loop (the take wraps),
    // otherwise the first samples are dropped and the tail is padded with silence.
    // Returns {previous take index (-1 = none), new take index} for undo.
    std::pair<int, int> commitTake (const juce::AudioBuffer<float>& src, int len, int shiftEarlier = 0,
                                    bool circular = true, int startOffset = 0)
    {
        if (takes.empty() && length.load() > 0)
        {
            juce::AudioBuffer<float> old (2, (int) length.load());
            {
                juce::SpinLock::ScopedLockType l (lock);
                for (int c = 0; c < 2; ++c) old.copyFrom (c, 0, data, c, 0, old.getNumSamples());
            }
            takes.push_back (std::move (old));
            takeOffsets.push_back (offset.load());
            currentTake = 0;
        }
        const int prev = currentTake;
        juce::AudioBuffer<float> t (2, len);
        t.clear();
        const int sh = circular ? (len > 0 ? ((shiftEarlier % len) + len) % len : 0) : juce::jlimit (0, len, shiftEarlier);
        for (int c = 0; c < 2; ++c)
        {
            if (sh < len) t.copyFrom (c, 0, src, c, sh, len - sh);
            if (circular && sh > 0) t.copyFrom (c, len - sh, src, c, 0, sh);
        }
        declickSeam (t, len, circular ? (len - sh) % juce::jmax (1, len) : 0);
        takes.push_back (std::move (t));
        takeOffsets.push_back (startOffset);
        if ((int) takes.size() > maxTakes) { takes.erase (takes.begin()); takeOffsets.erase (takeOffsets.begin()); }
        applyTake ((int) takes.size() - 1);
        return { prev, currentTake };
    }

    // Linear (non-loop) recording finished: compensate latency, round the clip up to whole bars, set its position.
    void finishLinearRecording (int comp, juce::int64 barSamples, int startOffset)
    {
        shiftEarlier (comp);
        juce::SpinLock::ScopedLockType l (lock);
        const auto len = length.load();
        if (len > 0 && barSamples > 0)
        {
            const auto rounded = juce::jmin<juce::int64> (((len + barSamples - 1) / barSamples) * barSamples, data.getNumSamples());
            for (int c = 0; c < 2; ++c) data.clear (c, (int) len, (int) (rounded - len));
            length = rounded;
        }
        offset = startOffset;
        ++revision;
    }
    // --- region editing (message thread): used by selection cut/fade/paste, all undoable by swapping regions ---
    juce::AudioBuffer<float> copyRegion (juce::int64 s0, juce::int64 s1)
    {
        const auto len = length.load();
        s0 = juce::jlimit<juce::int64> (0, len, s0);
        s1 = juce::jlimit<juce::int64> (s0, len, s1);
        juce::AudioBuffer<float> out (2, (int) (s1 - s0));
        juce::SpinLock::ScopedLockType l (lock);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, data, c, (int) s0, out.getNumSamples());
        return out;
    }

    void writeRegion (juce::int64 s0, const juce::AudioBuffer<float>& src)   // overwrite, clipped to the take length
    {
        juce::SpinLock::ScopedLockType l (lock);
        const auto n = juce::jmin<juce::int64> ((juce::int64) src.getNumSamples(), length.load() - s0);
        if (s0 < 0 || n <= 0) return;
        for (int c = 0; c < 2; ++c) data.copyFrom (c, (int) s0, src, c, 0, (int) n);
        ++revision;
    }

    // Short fades remove the click where a pass starts/ends (or where the loop seam falls).
    static constexpr int declickSamples = 96;     // ~2 ms
    static void declickSeam (juce::AudioBuffer<float>& b, int len, int seam)
    {
        if (len < declickSamples * 4) return;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < declickSamples; ++i)
            {
                const float g = (float) (i + 1) / (float) (declickSamples + 1);
                d[(seam + i) % len] *= g;                                  // fade in after the seam
                d[((seam - 1 - i) % len + len) % len] *= g;                // fade out before it
            }
        }
    }

    // Make the track silent without dropping its take history (used when undoing the first recording).
    void silence()
    {
        juce::SpinLock::ScopedLockType l (lock);
        length = 0;
        ++revision;
        currentTake = -1;
        resetEdit();
    }

    void applyTake (int i)
    {
        if (i < 0 || i >= (int) takes.size()) return;
        currentTake = i; resetEdit();
        if (i < (int) takeOffsets.size()) offset = takeOffsets[(size_t) i];
        const auto& src = takes[(size_t) i];
        juce::SpinLock::ScopedLockType l (lock);
        data.setSize (2, src.getNumSamples(), false, false, true);
        for (int c = 0; c < 2; ++c) data.copyFrom (c, 0, src, c, 0, src.getNumSamples());
        length = src.getNumSamples();
        ++revision;
    }

    // Linear (non-loop) recordings: drop the first n samples so the take lines up with the grid.
    void shiftEarlier (int n)
    {
        juce::SpinLock::ScopedLockType l (lock);
        const auto len = (int) length.load();
        if (n > 0 && len > n)
            for (int c = 0; c < 2; ++c)
            {
                auto* d = data.getWritePointer (c);
                std::memmove (d, d + n, sizeof (float) * (size_t) (len - n));
                juce::FloatVectorOperations::clear (d + (len - n), n);
            }
        if (len > declickSamples * 4)       // fade in the first ~2 ms so the take never starts with a click
            for (int c = 0; c < 2; ++c)
            {
                auto* d = data.getWritePointer (c);
                for (int i = 0; i < declickSamples; ++i) d[i] *= (float) (i + 1) / (float) (declickSamples + 1);
            }
        ++revision;
    }

    void setAudio (juce::AudioBuffer<float>&& b)
    {
        takes.clear(); takeOffsets.clear(); currentTake = -1; resetEdit();
        juce::SpinLock::ScopedLockType l (lock);
        const auto n = b.getNumSamples();
        data = std::move (b);
        length = n;
        ++revision;
    }
};
