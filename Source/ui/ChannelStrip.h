#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "audio/AudioEngine.h"

class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (AudioEngine& e, int idx);

    void refreshFromTrack();          // pull state from Track (after load)
    void setInputChoices (int numInputs);
    void tick();                      // meters, called from UI timer
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioEngine& engine;
    int index;
    juce::Colour accent;
    juce::int64 lastLen = -1;
    Track& track() { return engine.tracks[index]; }
    void updateDb();

    juce::Label nameLabel, dbLabel;
    juce::ComboBox input;
    juce::TextButton arm { "R" }, mute { "M" }, solo { "S" }, clear { "Clear" };
    juce::Slider pan, fader;
    LevelMeter meter;
};

class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (AudioEngine& e);
    void refreshFromEngine();
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioEngine& engine;
    juce::Label title, dbLabel;
    juce::Slider fader;
    LevelMeter meter;
};
