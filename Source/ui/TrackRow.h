#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "audio/AudioEngine.h"
#include "audio/SilenceTrim.h"
#include "Undo.h"

// Record-arm button drawn as a record dot (filled red button with dark dot when armed).
class ArmButton : public juce::TextButton
{
public:
    ArmButton() = default;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        getLookAndFeel().drawButtonBackground (g, *this, findColour (on ? buttonOnColourId : buttonColourId), over, down);
        const auto c = getLocalBounds().toFloat().getCentre();
        g.setColour (on ? Theme::deep : Theme::recordRed);
        g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);
    }
};

// One horizontal track row: header controls + waveform lane.
class TrackRow : public juce::Component
{
public:
    static constexpr int waveLeft = 394, waveRightInset = 8, clipStripH = 14;   // lane geometry shared with the Ruler
    TrackRow (AudioEngine& e, int slot);

    int slot() const noexcept { return index; }
    void refreshFromTrack();
    void syncMix();
    void lookAndFeelChanged() override;     // volume/pan/mute/solo changed elsewhere (mixer strip)
    void setInputChoices (int numInputs);
    void setView (double seconds, double barSecs, double start = 0.0) { viewSeconds = seconds; barSeconds = barSecs; viewStart = start; }
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::MouseCursor getMouseCursor() override;

    std::function<void (TrackRow*)> onDelete, onDragStart, onDragEnd;
    UndoPush onUndoable;
    std::function<void()> onArmChanged;          // a track was armed/disarmed by the user
    void syncArm() { arm.setToggleState (track().armed.load(), juce::dontSendNotification); repaint(); }
    std::function<void()> onReleaseFocus;   // give keyboard focus back to main window (shortcuts)
    std::function<void (TrackRow*, int)> onDragMove;

    // --- selection editing (drag in the waveform to select) ---
    enum SelCmd { SelSilence, SelCut, SelCopy, SelPaste, SelFadeIn, SelFadeOut, SelReverse };
    void selectAll();
    void cropToSelection();
    void trimSilence();
    std::function<void (juce::int64)> onCropped;
    std::function<void (juce::int64)> onClipGrew;   // end of the clip (samples on the timeline) after a duplicate   // length of the cropped clip in samples
    void restyle();
    static bool hasClipboard() { return clipboard.getNumSamples() > 0; }
    bool hasSelection() const noexcept { return selEnd > selStart; }
    juce::int64 selectionStart() const noexcept { return selStart; }
    juce::int64 selectionEnd() const noexcept { return selEnd; }
    int clipOffset() { return track().offset.load(); }
    void clearSelection() { if (selEnd > selStart) { selStart = selEnd = 0; repaint(); } }
    void selectionCommand (SelCmd cmd);
    std::function<void (TrackRow*)> onSelectionStarted, onDuplicate, onAnalyze;
    std::function<void (const juce::String&)> onStatus;

private:
    struct Grip : juce::Component
    {
        explicit Grip (TrackRow& o) : owner (o) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override { if (owner.onDragStart) owner.onDragStart (&owner); }
        void mouseDrag (const juce::MouseEvent& e) override { if (owner.onDragMove) owner.onDragMove (&owner, e.getDistanceFromDragStartY()); }
        void mouseUp (const juce::MouseEvent&) override { if (owner.onDragEnd) owner.onDragEnd (&owner); }
        TrackRow& owner;
    };

    Track& track() { return engine.tracks[index]; }
    void updateDb();
    void updateCache();

    AudioEngine& engine;
    int index;
    juce::Colour accent;
    double viewSeconds = 30.0, barSeconds = 0.0, viewStart = 0.0;
    juce::TextButton prevTake { "<" }, nextTake { ">" };
    juce::Label takeLabel;
    int shownTakes = -1, shownCurrent = -1;
    void takeStep (int d);
    void push (const juce::String& name, std::function<void()> redo, std::function<void()> undo) { if (onUndoable) onUndoable (name, std::move (redo), std::move (undo)); }
    void commitVol();
    void commitPan();
    void pushHitEdit (const juce::String& name, const std::vector<Hit>& before);
    float lastVol = 1.0f, lastPan = 0.0f;
    juce::String lastName;
    bool draggingFader = false, draggingPan = false;
    std::vector<Hit> hitsBeforeDrag;
    int nudgeBeforeEdit = 0;
    void showHitsMenu();
    void quantize (double division, float strength);
    int hitAt (int x) const;
    float sampleToX (juce::int64 s) const;
    juce::int64 xToSample (float x) const;
    juce::TextButton hitsBtn { "Hits" };
    int dragHit = -1;
    int lastGlowState = -1;
    juce::int64 lastTickPos = -1, lastTickCursor = -1, lastTickLen = -1;   // the waveform is only repainted when one of these (or the transport state) changed
    int lastTickState = -1;
    bool nudging = false, selecting = false, moveByRight = false, copying = false;
    int copyOffset = 0;
    void duplicateClipTo (int newOffset);
    juce::int64 selStart = 0, selEnd = 0, selAnchor = 0;      // selection in take samples
    juce::int64 xToTake (float x, bool snap, bool snapBar = false) const;
    void showClipMenu();
    static inline juce::AudioBuffer<float> clipboard;
    int nudgeStartX = 0, nudgeStartVal = 0;
    float riseDb = 6.0f;

    Grip grip { *this };
    juce::Label nameLabel, dbLabel;
    juce::ComboBox input;
    LevelMeter inMeter, meter;
    ArmButton arm;
    juce::TextButton mute { "M" }, solo { "S" }, del { juce::String::fromUTF8 ("\xC3\x97") };
    juce::Slider pan, fader;
    juce::Rectangle<int> wave, nameBar;

    // waveform min/max cache (incremental, 256 samples per bin)
    static constexpr int binSize = 256;
    std::vector<float> mn, mx, mn2, mx2;   // ch0 / ch1
    bool stereo = false;
    juce::int64 done = 0;
    unsigned rev = ~0u;
    // the loop pass being recorded (audio goes to a staging buffer, so the clip has no data yet): its peaks, built from the staging buffer
    std::vector<float> liveLo, liveHi;
    juce::int64 liveDone = 0;
    unsigned liveSeq = ~0u;
    int liveStage = -1;
    void updateLive();
};

// Master bus row.
class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (AudioEngine& e);
    void refreshFromEngine();
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    UndoPush onUndoable;

private:
    float lastMaster = 1.0f;
    bool dragging = false;
    void commitMaster();
    AudioEngine& engine;
    juce::Label title, dbLabel;
    juce::Slider fader;
    LevelMeter meter;
};

// Container laid out by engine.order, with drag-to-reorder.
class TrackList : public juce::Component
{
public:
    static constexpr int rowHeight = 96, gap = 0, addHeight = 0;   // each row has a small margin around its card
    TrackList (AudioEngine& e, juce::OwnedArray<TrackRow>& r) : engine (e), rows (r) {}

    int rowsHeight() const { return engine.getNumTracks() * (rowHeight + gap); }
    int contentHeight() const { return rowsHeight() + (engine.getNumTracks() < AudioEngine::maxTracks ? addHeight : 0); }
    std::function<void()> onAdd;
    std::function<void()> onEmptyMenu;      // right-click in the empty space below the last track
    std::function<void()> onEmptyClick;     // any click in the empty space (gives keyboard focus back to the main window)
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (onEmptyClick) onEmptyClick();
        if (e.mods.isPopupMenu() && onEmptyMenu) onEmptyMenu();
    }
    void layoutRows();
    void dragStart (TrackRow*);
    void dragMove (TrackRow*, int dy);
    void dragEnd (TrackRow*);
    std::function<void (std::vector<int> before, std::vector<int> after)> onReordered;
    void resized() override { layoutRows(); }

private:
    int targetSlot (int y) const;
    AudioEngine& engine;
    juce::OwnedArray<TrackRow>& rows;
    TrackRow* dragging = nullptr;
    int dragStartY = 0, dragY = 0;
};

// Time ruler above the track lanes: bar numbers + beat ticks (or seconds), loop band and playhead.
class Ruler : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Ruler (AudioEngine& e) : engine (e) {}
    void setView (double seconds, double barSecs, double start = 0.0) { viewSeconds = seconds; barSeconds = barSecs; viewStart = start; }
    std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (onWheel) onWheel (e, w); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { seekTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override { seekTo (e); }
    std::function<void (juce::int64)> onSeek;
private:
    void seekTo (const juce::MouseEvent&);
    AudioEngine& engine;
    double viewSeconds = 30.0, barSeconds = 0.0, viewStart = 0.0;
};