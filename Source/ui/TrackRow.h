#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "audio/AudioEngine.h"

// One horizontal track row: header controls + waveform lane.
class TrackRow : public juce::Component
{
public:
    static constexpr int waveLeft = 380, waveRightInset = 30, clipStripH = 14;   // lane geometry shared with the Ruler
    TrackRow (AudioEngine& e, int slot);

    int slot() const noexcept { return index; }
    void refreshFromTrack();
    void setInputChoices (int numInputs);
    void setView (double seconds, double barSecs) { viewSeconds = seconds; barSeconds = barSecs; }
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (TrackRow*)> onDelete, onDragStart, onDragEnd;
    std::function<void()> onReleaseFocus;   // give keyboard focus back to main window (shortcuts)
    std::function<void (TrackRow*, int)> onDragMove;

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
    double viewSeconds = 30.0, barSeconds = 0.0;
    juce::TextButton prevTake { "<" }, nextTake { ">" };
    juce::Label takeLabel;
    int shownTakes = -1, shownCurrent = -1;
    void takeStep (int d);

    Grip grip { *this };
    juce::Label nameLabel, dbLabel;
    juce::ComboBox input;
    LevelMeter inMeter, meter;
    juce::TextButton arm { "R" }, mute { "M" }, solo { "S" }, del { juce::String::fromUTF8 ("\xC3\x97") };
    juce::Slider pan, fader;
    juce::Rectangle<int> wave, nameBar;

    // waveform min/max cache (incremental, 256 samples per bin)
    static constexpr int binSize = 256;
    std::vector<float> mn, mx, mn2, mx2;   // ch0 / ch1
    bool stereo = false;
    juce::int64 done = 0;
    unsigned rev = ~0u;
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

private:
    AudioEngine& engine;
    juce::Label title, dbLabel;
    juce::Slider fader;
    LevelMeter meter;
};

// Live level of every active input channel; click an input to assign it to armed tracks.
class InputBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit InputBar (AudioEngine& e);
    void setNumInputs (int n);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onPick;

private:
    juce::Rectangle<int> cell (int i) const;
    AudioEngine& engine;
    juce::OwnedArray<LevelMeter> meters;
};

// Container laid out by engine.order, with drag-to-reorder.
class TrackList : public juce::Component
{
public:
    static constexpr int rowHeight = 92, gap = 8;
    TrackList (AudioEngine& e, juce::OwnedArray<TrackRow>& r) : engine (e), rows (r) {}

    int contentHeight() const { return juce::jmax (0, engine.getNumTracks() * (rowHeight + gap) - gap); }
    void layoutRows();
    void dragStart (TrackRow*);
    void dragMove (TrackRow*, int dy);
    void dragEnd (TrackRow*);
    void resized() override { layoutRows(); }

private:
    int targetSlot (int y) const;
    AudioEngine& engine;
    juce::OwnedArray<TrackRow>& rows;
    TrackRow* dragging = nullptr;
    int dragStartY = 0, dragY = 0;
};

// Time ruler above the track lanes: bar numbers + beat ticks (or seconds), loop band and playhead.
class Ruler : public juce::Component
{
public:
    explicit Ruler (AudioEngine& e) : engine (e) {}
    void setView (double seconds, double barSecs) { viewSeconds = seconds; barSeconds = barSecs; }
    void paint (juce::Graphics&) override;
private:
    AudioEngine& engine;
    double viewSeconds = 30.0, barSeconds = 0.0;
};