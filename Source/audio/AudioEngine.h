#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <vector>
#include <array>
#include "Track.h"
#include "DrumMachine.h"

// Audio-thread entry point. Owns the sample-counting transport clock that
// count-in, click, loop and record start are all synchronised to.
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine() { for (auto* b : { &mixL, &mixR, &nudgeL, &nudgeR, &fxL, &fxR }) b->allocate ((size_t) chunk, true); }
    static constexpr int maxTracks = 16;
    enum State { Stopped = 0, Playing = 1, Recording = 2, CountIn = 3 };

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void prepareForTest (double sr, int numIn) { sampleRate = sr; numInputs = numIn; prepareFx(); }
    void prepareFx() { for (auto& t : tracks) t.fx.prepare (sampleRate.load()); }
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                           float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;

    // Transport (message thread)
    void play();
    bool record();          // false if no armed track. Always starts at bar 1 (after count-in).
    void stop();
    // Playhead cursor (set by clicking the ruler). While playing it jumps immediately.
    void seek (juce::int64 pos);
    void setCursor (juce::int64 pos) { cursorPos = juce::jmax<juce::int64> (0, pos); }
    juce::int64 getCursor() const noexcept { return cursorPos.load(); }
    std::function<void (int slot, int prevTake, int newTake)> onTakeCommitted;   // UI thread, for undo
    void pollRecording();   // call regularly from the UI thread: commits finished loop passes
    State getState() const noexcept { return (State) state.load(); }
    juce::int64 getPosition() const noexcept { return playPos.load(); }
    juce::int64 getBarLength() const noexcept { return (juce::int64) std::llround (getBarBeats() * getSamplesPerBeat()); }
    juce::int64 getRecordStart() const noexcept { return recStart.load(); }   // where the current/last recording started
    juce::int64 getCountInPosition() const noexcept { return countPos.load(); }
    juce::int64 getCountInLength() const noexcept { return countInLen.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

    // Tempo / loop (change only while stopped)
    double getSamplesPerBeat() const noexcept { return sampleRate.load() * 60.0 / juce::jmax (1.0, bpm.load()); }
    juce::int64 getLoopLength() const noexcept { return (juce::int64) std::llround (loopBars.load() * getBarBeats() * getSamplesPerBeat()); }
    // Time signature: BPM counts quarter notes. A bar is tsNum * 4 / tsDen quarters (6/8 = 3, 7/8 = 3.5).
    std::atomic<int> tsNum { 4 }, tsDen { 4 };
    double getBarBeats() const noexcept { return tsNum.load() * 4.0 / tsDen.load(); }
    double getPulseSamples() const noexcept { return getSamplesPerBeat() * 4.0 / tsDen.load(); }   // one click / ruler subdivision
    int getPulsesPerBar() const noexcept { return tsNum.load(); }
    void setTimeSignature (int num, int den)
    {
        tsNum = juce::jlimit (1, 16, num); tsDen = (den == 8) ? 8 : 4;
        drums.setBarSteps ((int) std::lround (getBarBeats() * 4.0));
    }
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> loopBars { 4 };
    std::atomic<bool> loopEnabled { true };
    // Punch in/out (loop recording only): a new take replaces the old clip only inside [punchStart, punchEnd) (timeline samples)
    std::atomic<bool> punchEnabled { false };
    std::atomic<juce::int64> punchStart { 0 }, punchEnd { 0 };
    bool punchActive() const noexcept { return punchEnabled.load() && loopEnabled.load() && punchEnd.load() > punchStart.load(); }
    std::atomic<int> countInBars { 1 };          // 0 = off, else whole bars of 4 beats
    std::atomic<bool> clickEnabled { false };
    std::atomic<float> clickVolume { 0.5f };

    // Latency compensation: device-reported in+out latency plus a manual adjustment (ms)
    std::atomic<int> deviceLatency { 0 };
    std::atomic<float> latencyAdjustMs { 0.0f };
    int getCompensationSamples() const noexcept
    {
        return juce::jmax (0, deviceLatency.load() + (int) std::lround (latencyAdjustMs.load() * sampleRate.load() / 1000.0));
    }

    // Track slots (message thread). `order` is the display/save order of active slots.
    int getNumTracks() const noexcept { return (int) order.size(); }
    int addTrack();                  // returns slot index or -1
    int addTrackAt (int slot);       // prefers the given slot (undo/redo), falls back to any free one
    void removeTrack (int slot);
    void removeAllTracks();
    std::vector<int> order;
    int getNumInputs() const noexcept { return numInputs.load(); }

    Track tracks[maxTracks];
    DrumMachine drums;
    std::atomic<float> masterVolume { 1.0f };
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> inputLevel[16] {};   // decaying peak per active input channel

    // Tuner feed: the audio thread copies one input channel into a ring; the UI reads the latest samples.
    static constexpr int tunerRingSize = 8192;
    std::atomic<int> tunerChannel { -1 };                 // -1 = off
    int readTunerSamples (float* dest, int count) const   // newest `count` samples, oldest first
    {
        const auto w = tunerWrite.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
            dest[i] = tunerRing[(size_t) ((w - count + i) & (tunerRingSize - 1))];
        return count;
    }
    void pushTunerSamples (const float* src, int n)       // test hook and audio thread
    {
        auto w = tunerWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) tunerRing[(size_t) ((w + i) & (tunerRingSize - 1))] = src[i];
        tunerWrite.store (w + n, std::memory_order_release);
    }
    juce::String projectNotes;                            // message thread only; saved in project.json

private:
    static constexpr int chunk = 4096;
    int processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n);

    std::atomic<int> state { Stopped };
    std::atomic<juce::int64> playPos { 0 };     // position inside loop (or linear position)
    std::atomic<juce::int64> countPos { 0 };    // samples elapsed in count-in
    std::atomic<juce::int64> countInLen { 0 };
    std::atomic<juce::int64> jumpTo { -1 };      // pending position jump (audio thread consumes it)
    std::atomic<juce::int64> recStart { 0 };     // recording starts here (cursor floored to a bar)
    std::atomic<juce::int64> cursorPos { 0 };    // where Play starts; the stopped playhead
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> numInputs { 0 };
    juce::int64 drumPass = 0;                   // completed loop passes since playback started (drum phase)
    double drumSr = 0.0;
    std::atomic<bool> drumReset { false };
    int sessionLen = 0;                         // loop length (samples) of current record session

    std::array<float, tunerRingSize> tunerRing {};
    std::atomic<juce::int64> tunerWrite { 0 };
    juce::HeapBlock<float> mixL, mixR, nudgeL, nudgeR, fxL, fxR;   // on the heap: engines are also created on the stack in tests
    float masterCur = 1.0f;

    // click voice (audio thread)
    int clickLeft = 0, clickLen = 1;
    float clickPhase = 0.0f, clickInc = 0.0f;
};
