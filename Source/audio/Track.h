#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>

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
    std::atomic<bool> active { false };            // slot in use
    std::atomic<unsigned> revision { 0 };          // bumped when audio is replaced/truncated (waveform cache)
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
        volume = 1.0f; pan = 0.0f; mute = false; solo = false; armed = false; inputCode = 0;
        clearAudio();
    }

    void clearAudio()
    {
        juce::SpinLock::ScopedLockType l (lock);
        length = 0;
        ++revision;
    }

    // Allocate capacity and truncate the take at fromPos.
    void prepareRecording (juce::int64 fromPos, double sampleRate)
    {
        juce::SpinLock::ScopedLockType l (lock);
        const auto cap = (int) (sampleRate * maxRecordSeconds);
        if (data.getNumSamples() < cap)
            data.setSize (2, cap, true, true, false);
        length = juce::jmin<juce::int64> (length.load(), fromPos);
        ++revision;
    }

    void setAudio (juce::AudioBuffer<float>&& b)
    {
        juce::SpinLock::ScopedLockType l (lock);
        const auto n = b.getNumSamples();
        data = std::move (b);
        length = n;
        ++revision;
    }
};
