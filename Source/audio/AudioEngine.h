#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>

// Audio-thread entry point. Owns the sample-counting clock that later phases
// (count-in, loop, record start) are synchronised to.
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;

    juce::int64 getSamplePosition() const noexcept { return samplePos.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

private:
    std::atomic<juce::int64> samplePos { 0 };
    std::atomic<double> sampleRate { 44100.0 };
};
