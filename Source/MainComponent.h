#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "audio/AudioEngine.h"
#include "project/Exporter.h"
#include "ui/ThemeEditor.h"
#include "audio/SilenceTrim.h"
#include "audio/MidiPedal.h"
#include "WinFileAssoc.h"
#include "ui/TrackRow.h"
#include "ui/DrumPanel.h"
#include "ui/BassPanel.h"
#include "ui/PatternTabs.h"
#include "ui/LibraryCounter.h"
#include "ui/SegmentDisplay.h"
#include "ui/SongBar.h"
#include "ui/AnalysisCard.h"
#include "ui/MixerPanel.h"
#include "ui/Undo.h"

class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener,
                      public juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // The production bottom-row layout, also exercised without devices/settings by selftest.
    static void layoutBottomRow (juce::Rectangle<int>& remaining, DrumPanel&, BassPanel&, MixerPanel&, int tab);
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showAudioSettings();
    void newProject();
    void openProject();
    void saveProject (bool saveAs);
    void doSave (const juce::File& file);
    void importAudio();
    void doImport (const juce::File& file);
    static juce::File autosaveDir();
    static juce::File autosaveFile() { return autosaveDir().getChildFile ("autosave.riffcatch"); }
    void autosaveTick();
    void offerRestore();
    int autosaveTicks = 0, editCounter = 0;
    juce::int64 lastAutosaveSig = 0;
    void doOpen (const juce::File& file);
public:
    void openFromCommandLine (const juce::String& commandLine);   // a .riffcatch path from Explorer or a second start
private:
    void refreshStrips();
    void addTrack();
    void deleteTrack (TrackRow*);
    void duplicateTrack (int slot);
    TrackRow* selectionRow();
    int selSlot = -1;
    void setStatus (const juce::String&, int ticks = 90);
    void lookAndFeelChanged() override;
    void restyle();
    void applyTheme (int index);
    void refreshTheme();
    void updateNotesButton();
    void addRecent (const juce::File& dir);
    juce::RecentlyOpenedFilesList recent;
    void editNotes();
    void openThemeEditor();
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
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
        int slot = -1, orderPos = 0, input = 0, offset = 0, echoDiv = 1;
    bool echoOn = false, reverbOn = false;
        juce::String name;
        float vol = 1.0f, pan = 0.0f;
        bool mute = false, solo = false, valid = false;
        juce::AudioBuffer<float> audio;
    };
    TrackSnap captureTrack (int slot);
    int restoreTrack (const TrackSnap&);
    void exportDialog();
    void fileMenu();
    void doExport (const juce::File&, int kind, int loops, Exporter::Source source, int pattern);
    void saveLatencyAdjust();
    void setBpm (double v);
    void updateSettingsFromEngine();

    juce::UndoManager undoManager;
    juce::ApplicationProperties props;
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;

    juce::OwnedArray<TrackRow> rows;
    TrackList list { engine, rows };
    // Ctrl+wheel zooms the time axis, Shift+wheel pans; the plain wheel keeps scrolling the tracks vertically
    struct ZoomViewport : juce::Viewport
    {
        std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onZoomWheel;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (onZoomWheel && (e.mods.isCtrlDown() || e.mods.isShiftDown() || w.deltaX != 0.0f)) onZoomWheel (e, w);
            else juce::Viewport::mouseWheelMove (e, w);
        }
    } viewport;
    double zoom = 1.0, zoomStart = 0.0, fullView = 8.0;      // visible window = [zoomStart, zoomStart + fullView / zoom)
    double lastVis = -1.0, lastStart = -1.0, lastFull = -1.0;
    void handleWheel (const juce::MouseEvent&, const juce::MouseWheelDetails&, bool plainZooms);
    void zoomAt (double frac, double factor);
    void clampZoom();
    MixerPanel mixer { engine };
    DrumPanel drumPanel { engine };
    BassPanel bassPanel { engine };
    void showSeqTab (int tab);
    void bakeBass();
    void bakeBassLive();
    void analyzeTrack (int slot);
    void addBassTrack (std::shared_ptr<TrackSnap> snap, const juce::String& undoName);
    int lastCapState = 0;
    int seqTab = 0;
    PatternTabs patternTabs { engine };
    juce::Rectangle<int> seqTabsRect;
    void setupLibraryCounter();
    SongBar songBar { engine };
    juce::TextButton songBtn { "Song" };
    void setSongMode (bool on);
    bool startSong();
    void setupPatternTabs();
    void switchPattern (int i);
    Ruler ruler { engine };

    IconButton recBtn  { IconButton::Record, Theme::red };
    IconButton playBtn { IconButton::Play,   Theme::green };
    IconButton stopBtn { IconButton::Stop,   Theme::text };
    IconButton skipStartBtn { IconButton::SkipStart, Theme::text }, skipEndBtn { IconButton::SkipEnd, Theme::text };
    juce::TextButton punchBtn { "Punch" };
    IconButton loopBtn { IconButton::Loop,   Theme::accent };
    ToolButton zoomInBtn { ToolButton::ZoomIn }, zoomOutBtn { ToolButton::ZoomOut }, zoomFitBtn { ToolButton::ZoomFit };
    ToolButton undoBtn { ToolButton::Undo }, redoBtn { ToolButton::Redo };
    ToolButton cutBtn { ToolButton::Cut }, copyBtn { ToolButton::Copy }, pasteBtn { ToolButton::Paste }, silenceBtn { ToolButton::Silence },
               fadeInBtn { ToolButton::FadeIn }, fadeOutBtn { ToolButton::FadeOut }, dupBtn { ToolButton::Duplicate },
        selectAllBtn { ToolButton::SelectAll }, reverseBtn { ToolButton::Reverse }, cropBtn { ToolButton::Crop };
    ToolButton fileBtn { ToolButton::Menu };
    ToolButton folderBtn { ToolButton::Folder };
    juce::File workDir() const;
    void setWorkDir (const juce::File& dir);
    void openCurrentFolder();
    void chooseWorkDir();
    void refreshFolderTip();
    AddTrackButton addTrackBtn;
    juce::TextButton notesBtn { "Notes" };
    // Catch (always-on capture) and the one-button looper
    juce::TextButton catchBtn { "Catch" }, looperBtn { "Looper" };
    juce::ComboBox catchLenBox;
    MidiPedal pedal;
    enum class LooperState { Idle, FirstTake, Finishing, Playing, Queued, Overdub };
    LooperState looperState = LooperState::Idle;
    int looperSlot = -1, looperLayerSlot = -1, looperSavedCountIn = 1;
    juce::int64 looperFirstSamples = 0;
    void catchNow();
    void looperPress();
    void looperTick();
    void looperReset (const juce::String& why = {});
    void looperStartFirstTake();
    void looperFinishFirstTake();
    void looperQueueOverdub();
    SegmentClock clock;
    juce::Label timeLabel, barLabel, projectLabel, statusLabel, titleLabel, bpmCaption, countCaption;
    juce::Label bpmLabel;
    juce::TextButton tapBtn { "Tap" }, clickBtn { "Click" };
    juce::ComboBox barsBox, countInBox, timeSigBox;
    void syncTimeSigBox();
    juce::Slider clickVol, latAdjust;
    juce::Label latLabel;
    std::vector<double> taps;

    juce::File projectFile;          // the open .riffcatch file (empty = untitled)
    int statusTicks = 0;
    juce::Rectangle<int> barArea, settingsArea;
    bool hoverTip = false;     // info bar currently shows a hover hint

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::Component> audioDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
