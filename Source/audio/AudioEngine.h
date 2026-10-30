#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include "Track.h"

// Audio-thread entry point. Owns the sample-counting transport clock that later
// phases (count-in, loop, record start) synchronise to.
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    static constexpr int maxTracks = 8;
    enum State { Stopped = 0, Playing = 1, Recording = 2 };

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                           float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;

    // Transport (message thread)
    void play();
    bool record();          // false if no armed track
    void stop();
    State getState() const noexcept { return (State) state.load(); }
    juce::int64 getPosition() const noexcept { return playPos.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

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
    void processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n);

    std::atomic<int> state { Stopped };
    std::atomic<juce::int64> playPos { 0 };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> numInputs { 0 };

    float mixL[chunk] {}, mixR[chunk] {};
    float masterCur = 1.0f;
};
