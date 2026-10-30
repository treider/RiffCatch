#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "audio/AudioEngine.h"
#include "ui/Theme.h"
#include "ui/TrackRow.h"

class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showAudioSettings();
    void newProject();
    void openProject();
    void saveProject (bool saveAs);
    void doSave (const juce::File& dir);
    void doOpen (const juce::File& dir);
    void refreshStrips();
    void addTrack();
    void deleteTrack (TrackRow*);
    void setStatus (const juce::String&);
    void toggleRecord();
    void togglePlay();

    juce::ApplicationProperties props;
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;

    juce::OwnedArray<TrackRow> rows;
    TrackList list { engine, rows };
    juce::Viewport viewport;
    MasterStrip master { engine };
    InputBar inputBar { engine };

    IconButton recBtn  { IconButton::Record, Theme::red };
    IconButton playBtn { IconButton::Play,   Theme::green };
    IconButton stopBtn { IconButton::Stop,   Theme::blue };
    juce::TextButton addBtn { "+ Track" }, newBtn { "New" }, openBtn { "Open" }, saveBtn { "Save" }, audioBtn { "Audio" };
    juce::Label timeLabel, projectLabel, statusLabel, titleLabel;

    juce::File projectDir;
    double bpm = 120.0;
    int statusTicks = 0;
    juce::Rectangle<int> barArea;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::Component> audioDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
