#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "audio/AudioEngine.h"
#include "Theme.h"
#include "TrackRow.h"
#include "Undo.h"
#include "SeqTabs.h"
#include "LibraryCounter.h"
#include "SeqHeaderLayout.h"

// Step sequencer panel above the master strip: six drum lanes, aligned with the ruler / clip lanes.
// Click or drag in the grid to set hits (right-click or Shift = accent). The pattern repeats over the loop.
class DrumPanel : public juce::Component
{
public:
    static constexpr int headerH = 42, laneH = 26, bottomPad = 8;

    explicit DrumPanel (AudioEngine& e);

    LibraryCounter& counter() { return libCounter; }
    SeqTabs& seqTabs() { return tabs; }                // shown by the main window above the card
    bool hasPages() const { return prevPage.isVisible(); }
    void setSharedHeader (int width, bool pages, juce::Point<int> origin)
    { sharedHeaderWidth = width; sharedHeaderPages = pages; headerOrigin = origin; }
    SeqHeaderLayout::Controls headerControls() { return { &chevron, &libCounter, &barsBox, &templateBox,
        &prevPage, &pageLabel, &nextPage, &clearBtn, &muteBtn, &soloBtn, &volume }; }
    int headerHeight() const { return SeqHeaderLayout::height (sharedHeaderWidth > 0 ? sharedHeaderWidth : getWidth(),
                                                             sharedHeaderWidth > 0 ? sharedHeaderPages : hasPages()); }
    int bodyTop() const { return headerHeight() + headerOrigin.y; }
    int preferredHeight() const { return headerHeight() + (expanded ? DrumMachine::numVoices * laneH + bottomPad : 0); }
    bool isExpanded() const noexcept { return expanded; }
    void setExpanded (bool b);
    void refreshTemplates();
    void setView (double seconds, double barSecs, double start = 0.0, double full = -1.0);
    std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (onWheel) onWheel (e, w); }
    void refreshFromEngine();
    void tick();

    void paint (juce::Graphics&) override;
    void lookAndFeelChanged() override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    UndoPush onUndoable;
    std::function<void (int)> onSelectTab;
    std::function<void()> onLayoutChanged;
    std::function<void()> onReleaseFocus;
    std::function<void (const juce::String&)> onInfo;   // value read-out while a knob is turned

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

    juce::Rectangle<int> lanesArea() const;                    // the step grid (aligned with the clip lanes)
    juce::Rectangle<float> laneRect (int voice) const;
    double stepSec() const;
    int viewSteps() const;                                      // cells visible (one per 1/16 over the loop)
    int cellAt (float x) const;                                 // visible cell index, -1 outside
    int stepForCell (int cell) const;                           // pattern step edited by a visible cell
    void setCell (int voice, int cell, int value);
    void pushPattern (const juce::String& name, const DrumMachine::Snapshot& before, const DrumMachine::Snapshot& after);
    void updateControls();
    void info (const juce::String& what, const juce::String& value);
    static juce::Colour voiceColour (int v);

    AudioEngine& engine;
    bool expanded = true;
    int sharedHeaderWidth = 0;
    bool sharedHeaderPages = false;
    juce::Point<int> headerOrigin;
    double viewSeconds = 8.0, barSeconds = 2.0, viewStart = 0.0, fullSeconds = 8.0;   // visible window and the whole loop
    int page = 0;

    Chevron chevron;
    SeqTabs tabs { 0, Theme::orange };
    LibraryCounter libCounter { engine, PatternBank::Drums, Theme::orange };
    juce::ComboBox barsBox, templateBox;
    juce::TextButton clearBtn { "Clear" }, prevPage { "<" }, nextPage { ">" }, muteBtn { "M" }, soloBtn { "S" };
    juce::Label pageLabel;
    juce::Slider volume, laneVol[DrumMachine::numVoices], lanePan[DrumMachine::numVoices], cymPitch;
    LevelMeter meter;

    bool painting = false;
    int paintVoice = 0, paintValue = 0;
    DrumMachine::Snapshot beforePaint;
    float lastPlayX = -1.0f;
};
