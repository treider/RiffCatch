#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "audio/AudioEngine.h"
#include "ui/Theme.h"
#include "ui/TrackRow.h"
#include "ui/Undo.h"

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
    void duplicateTrack (int slot);
    TrackRow* selectionRow();
    int selSlot = -1;
    void setStatus (const juce::String&, int ticks = 90);
    void stopTransport();
    void disarmAll();
    void warnIfMultipleArmed();
    void toggleRecord();
    void togglePlay();
    void tapTempo();
    void record (const juce::String& name, std::function<void()> redo, std::function<void()> undo);
    void doUndo (bool redo);
    void syncRows();
    struct TrackSnap
    {
        int slot = -1, orderPos = 0, input = 0, nudge = 0;
        juce::String name;
        float vol = 1.0f, pan = 0.0f;
        bool mute = false, solo = false, valid = false;
        juce::AudioBuffer<float> audio;
    };
    TrackSnap captureTrack (int slot);
    int restoreTrack (const TrackSnap&);
    void exportMenu();
    void doExport (const juce::File&, int kind, int loops);
    void saveLatencyAdjust();
    void setBpm (double v);
    void updateSettingsFromEngine();

    juce::UndoManager undoManager;
    juce::ApplicationProperties props;
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;

    juce::OwnedArray<TrackRow> rows;
    TrackList list { engine, rows };
    juce::Viewport viewport;
    MasterStrip master { engine };
    Ruler ruler { engine };

    IconButton recBtn  { IconButton::Record, Theme::red };
    IconButton playBtn { IconButton::Play,   Theme::green };
    IconButton stopBtn { IconButton::Stop,   Theme::blue };
    ToolButton undoBtn { ToolButton::Undo }, redoBtn { ToolButton::Redo };
    ToolButton cutBtn { ToolButton::Cut }, copyBtn { ToolButton::Copy }, pasteBtn { ToolButton::Paste }, silenceBtn { ToolButton::Silence },
               fadeInBtn { ToolButton::FadeIn }, fadeOutBtn { ToolButton::FadeOut }, dupBtn { ToolButton::Duplicate };
    juce::TextButton exportBtn { "Export" }, newBtn { "New" }, openBtn { "Open" }, saveBtn { "Save" }, audioBtn { "Audio" };
    juce::Label timeLabel, barLabel, projectLabel, statusLabel, titleLabel, bpmCaption, countCaption;
    juce::Label bpmLabel;
    juce::TextButton tapBtn { "Tap" }, loopBtn { "Loop" }, clickBtn { "Click" };
    juce::ComboBox barsBox, countInBox;
    juce::Slider clickVol, latAdjust;
    juce::Label latLabel;
    std::vector<double> taps;

    juce::File projectDir;
    int statusTicks = 0;
    juce::Rectangle<int> barArea, settingsArea;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::Component> audioDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
