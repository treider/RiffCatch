#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "audio/AudioEngine.h"
#include "Theme.h"
#include "TrackRow.h"
#include "Undo.h"
#include "SeqTabs.h"
#include "LibraryCounter.h"
#include "SeqHeaderLayout.h"
#include "BassLightPoc.h"
#include "audio/BassRandom.h"

// Bass synth panel: a piano-roll-like grid in the chosen scale, aligned with the ruler / clip lanes (like the drum panel).
// Click = add a note, drag right = length, drag up/down = pitch, click a note = remove it,
// right-click = accent, Ctrl+click = slide. Click a bar label above the grid to transpose that bar.
class BassPanel : public juce::Component
{
public:
    static constexpr int headerH = 42, shiftH = 13, rowH = 13, gridH = 143, bottomPad = 8;   // rowH: the normal row height (11 rows); the chromatic scale shows 13 thinner rows

    explicit BassPanel (AudioEngine& e);

    LibraryCounter& counter() { return libCounter; }
    SeqTabs& seqTabs() { return tabs; }                // shown by the main window above the card
    // Width is supplied by the parent before setBounds, so crossing a breakpoint
    // never uses last frame's width. Page controls also need room in the header.
    bool hasPages() const { return prevPage.isVisible(); }
    void setSharedHeader (int width, bool pages) { sharedHeaderWidth = width; sharedHeaderPages = pages; }
    SeqHeaderLayout::Controls headerControls() { return { &chevron, &libCounter, &barsBox, &templateBox,
        &prevPage, &pageLabel, &nextPage, &clearBtn, &muteBtn, &soloBtn, &volume }; }
    int headerHeight (int width) const { return SeqHeaderLayout::height (width, sharedHeaderWidth > 0 ? sharedHeaderPages : hasPages()); }
    int headerHeight() const { return headerHeight (getWidth()); }
    int preferredHeight (int width) const { return headerHeight (width) + (expanded ? shiftH + gridH + bottomPad : 0); }
    int preferredHeight() const { return preferredHeight (getWidth()); }
    bool isExpanded() const noexcept { return expanded; }
    void setExpanded (bool b);
    void setView (double seconds, double barSecs, double start = 0.0, double full = -1.0);
    void refreshFromEngine();
    void tick();

    std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        // plain wheel scrolls the piano roll up and down; Ctrl / Shift / sideways keep zooming and panning the time axis
        if (e.mods.isCtrlDown() || e.mods.isShiftDown() || w.deltaY == 0.0f) { if (onWheel) onWheel (e, w); return; }
        wheelAcc += w.deltaY * 7.0f;
        const int n = (int) wheelAcc;
        if (n == 0) return;
        wheelAcc -= (float) n;
        scrollView (n);
    }

    void paint (juce::Graphics&) override;
    void paintOuterLight (juce::Graphics&, juce::Component& paintOwner);
    void lookAndFeelChanged() override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    UndoPush onUndoable;
    std::function<void()> onLayoutChanged, onReleaseFocus, onBake, onBakeLive;
    void setLiveState (int state);                       // 0 idle, 1 armed, 2 recording (button look)
    std::function<void (int)> onSelectTab;
    std::function<void (const juce::String&)> onInfo;

private:
    struct Chevron : juce::Button
    {
        Chevron() : juce::Button ({}) { setWantsKeyboardFocus (false); }
        bool open = true;
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (over ? Theme::text : Theme::dim);
            juce::Path p;
            const auto c = b.getCentre();
            if (open) p.addTriangle (c.x - 5.0f, c.y - 2.5f, c.x + 5.0f, c.y - 2.5f, c.x, c.y + 3.5f);
            else      p.addTriangle (c.x - 2.5f, c.y - 5.0f, c.x - 2.5f, c.y + 5.0f, c.x + 3.5f, c.y);
            g.fillPath (p);
        }
    };
    struct FxToggle : juce::TextButton
    {
        std::function<void()> onRight;
        void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } juce::TextButton::mouseDown (e); }
    };
    struct Knob
    {
        explicit Knob (bool sub) : slider (sub ? new BassLightPoc::Slider : new juce::Slider), s (*slider) {}
        std::unique_ptr<juce::Slider> slider;
        juce::Slider& s; // keep all bindings unchanged; the other seven remain plain JUCE Sliders
        juce::String name;
        std::function<juce::String (double)> fmt;
    };
    friend struct BassLightPocTest;
    BassLightPoc::Frame frameLight;
    BassLightPoc::Sub subLight;
    juce::Rectangle<int> lastSubLightBounds, lastSubInvalidation;
    double lastSubLightValue = -1.0;
    int lastSubLightState = -1, subInvalidations = 0;
    void invalidateSubLight();
    juce::Rectangle<float> subSliderArea();

    juce::Rectangle<int> gridArea() const;                       // the note rows
    juce::Rectangle<int> stripArea() const;                      // bar transposition labels
    double stepSec() const;
    int viewSteps() const;
    int cellAt (float x, bool clamp) const;
    int stepForCell (int cell) const;
    int rowAt (int y) const;                                     // 0 = lowest, -1 outside
    // rows shown: 11 scale degrees, or the 13 semitones of an octave (C2-C3 with root C) for the chromatic scale; Oct-/Oct+ move the window
    int rowsNow() const { return engine.bass.scale.load() == 6 ? 13 : BassMachine::numRows; }
    int rowHeight() const { return gridH / rowsNow(); }
    int baseDegree() const { return (engine.bass.scale.load() == 6 ? 0 : BassMachine::degreeBase) + viewShift; }
    float rowY (int row) const { return (float) (headerHeight() + shiftH + (rowsNow() - 1 - row) * rowHeight()); }
    void pushPattern (const juce::String& name, const BassMachine::Snapshot& before, const BassMachine::Snapshot& after);
    void updateControls();
    void info (const juce::String& what, const juce::String& value);
    void audition (int step, int degree);
    int effectiveLen (int step) const;
    void showShiftMenu (int bar);

    AudioEngine& engine;
    bool expanded = true;
    int sharedHeaderWidth = 0;
    bool sharedHeaderPages = false;
    double viewSeconds = 8.0, barSeconds = 2.0, viewStart = 0.0, fullSeconds = 8.0;
    int page = 0;

    Chevron chevron;
    SeqTabs tabs { 1, Theme::cyan };
    LibraryCounter libCounter { engine, PatternBank::Bass, Theme::cyan };
    juce::ComboBox barsBox, rootBox, scaleBox, templateBox, progBox, presetBox, waveBox;
    juce::TextButton clearBtn { "Clear" }, bakeBtn { "Bake" }, liveBtn { "Rec" }, prevPage { "<" }, nextPage { ">" }, muteBtn { "M" }, soloBtn { "S" };
    FxToggle echoBtn, reverbBtn;
    juce::Label pageLabel;
    juce::Slider volume;
    LevelMeter meter;
    juce::OwnedArray<Knob> knobs;
    juce::TextButton randBtn { "Random" };
    BassRandom::Options randOpts;
    juce::Random rng;
    void showRandomOptions();
    void generateRandom();
    int viewShift = 0;        // piano roll scroll in scale degrees (the wheel moves it)
    float wheelAcc = 0.0f;
    void scrollView (int rows);

    // mouse editing: Create = new note being dragged out, Pending = pressed an existing note (click removes it, drag erases),
    // Resize = dragging the right edge of a note, Erase = wiping notes under the pointer
    enum class Mode { None, Create, Pending, Resize, Erase };
    Mode mode = Mode::None;
    int drawStep = 0, drawCell = 0, drawDegree = 0, drawAccent = 0;
    int pendingHead = 0, pendingJ = 0, lastCell = 0;
    float cellX (int cell) const;                                // left x of a visible cell
    int findNoteAt (int step, int degree) const;                 // head step of the note covering this step in that row, else -1
    void eraseRange (int cellA, int cellB, int row);
    BassMachine::Snapshot beforePaint;
    float lastPlayX = -1.0f;
};
