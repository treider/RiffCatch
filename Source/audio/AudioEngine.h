#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <vector>
#include "Track.h"

// Audio-thread entry point. Owns the sample-counting transport clock that
// count-in, click, loop and record start are all synchronised to.
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    static constexpr int maxTracks = 8;
    enum State { Stopped = 0, Playing = 1, Recording = 2, CountIn = 3 };

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void prepareForTest (double sr, int numIn) { sampleRate = sr; numInputs = numIn; }
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                           float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;

    // Transport (message thread)
    void play();
    bool record();          // false if no armed track. Always starts at bar 1 (after count-in).
    void stop();
    void pollRecording();   // call regularly from the UI thread: commits finished loop passes
    State getState() const noexcept { return (State) state.load(); }
    juce::int64 getPosition() const noexcept { return playPos.load(); }
    juce::int64 getCountInPosition() const noexcept { return countPos.load(); }
    juce::int64 getCountInLength() const noexcept { return countInLen.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

    // Tempo / loop (change only while stopped)
    double getSamplesPerBeat() const noexcept { return sampleRate.load() * 60.0 / juce::jmax (1.0, bpm.load()); }
    juce::int64 getLoopLength() const noexcept { return (juce::int64) std::llround (loopBars.load() * 4.0 * getSamplesPerBeat()); }
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> loopBars { 4 };
    std::atomic<bool> loopEnabled { true };
    std::atomic<int> countInBars { 1 };          // 0 = off, else whole bars of 4 beats
    std::atomic<bool> clickEnabled { true };
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
    void removeTrack (int slot);
    void removeAllTracks();
    std::vector<int> order;
    int getNumInputs() const noexcept { return numInputs.load(); }

    Track tracks[maxTracks];
    std::atomic<float> masterVolume { 1.0f };
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> inputLevel[16] {};   // decaying peak per active input channel

private:
    static constexpr int chunk = 4096;
    int processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n);

    std::atomic<int> state { Stopped };
    std::atomic<juce::int64> playPos { 0 };     // position inside loop (or linear position)
    std::atomic<juce::int64> countPos { 0 };    // samples elapsed in count-in
    std::atomic<juce::int64> countInLen { 0 };
    std::atomic<bool> rewind { false };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> numInputs { 0 };
    int sessionLen = 0;                         // loop length (samples) of current record session

    float mixL[chunk] {}, mixR[chunk] {}, nudgeL[chunk] {}, nudgeR[chunk] {};
    float masterCur = 1.0f;

    // click voice (audio thread)
    int clickLeft = 0, clickLen = 1;
    float clickPhase = 0.0f, clickInc = 0.0f;
};
