#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "audio/AudioEngine.h"

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void resized() override;

private:
    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
