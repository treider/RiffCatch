#pragma once
#include "LibraryCounter.h"
#include <array>

// One small geometry value object for the two sequencer headers. Coordinates
// are logical Bass-panel coordinates; the parent supplies the conversion into
// edge-painted Drums. Empty Bass-only slots in Drums are intentional.
struct SeqHeaderLayout
{
    enum Slot { Chevron, Counter, Bars, Template, Previous, PageLabel, Next, Clear, Mute, Solo, Volume, numCommon };
    using Controls = std::array<juce::Component*, numCommon>;
    std::array<juce::Rectangle<int>, numCommon> common;
    juce::Rectangle<int> root, scale, chords, random, record, bake, meter;

    static int height (int width, bool pages) { return width < (pages ? 1134 : 1000) ? 74 : 42; }
    void apply (const Controls& controls, juce::Point<int> origin = {}) const
    {
        for (size_t i = 0; i < controls.size(); ++i)
            controls[i]->setBounds (common[i].translated (origin.x, origin.y));
    }
    static SeqHeaderLayout make (int width, bool pages)
    {
        SeqHeaderLayout a;
        auto r = juce::Rectangle<int> (0, 0, width, 42).reduced (12, 7);
        const int h = r.getHeight(), y = r.getY();
        int x = r.getRight() - 4;
        auto put = [&] (juce::Rectangle<int>& c, int w, int gap)
        { x -= w; c = { x, y, w, h }; x -= gap; };
        put (a.common[Clear], 56, 12); put (a.common[Solo], 30, 4); put (a.common[Mute], 30, 12);
        x -= 32; a.common[Volume] = juce::Rectangle<int> (x, y, 32, h).withSizeKeepingCentre (32, 32);
        x -= 20;
        put (a.bake, 52, 4); put (a.record, 44, 0);
        const int rightFixed = r.getRight() - 4 - x, rightEdge = x - 8;
        a.common[Chevron] = { r.getX() + 6, y, 24, h };
        a.common[Counter] = { LibraryCounter::panelX, y, LibraryCounter::totalW, h };
        const int leftStart = LibraryCounter::panelX + LibraryCounter::totalW + 8;
        const int leftFixed = (leftStart - r.getX()) + 64 + 8 + 30 + (pages ? 134 : 0), comboBase = 496;
        const int meterRoom = r.getWidth() - leftFixed - comboBase - rightFixed - 16;
        const int meterW = meterRoom >= 60 ? juce::jmin (190, meterRoom) : 0;
        if (meterW > 0)
            a.meter = juce::Rectangle<int> (rightEdge - meterW, y, meterW, h).withSizeKeepingCentre (meterW, 14);
        const int groupEnd = meterW > 0 ? rightEdge - meterW - 16 : rightEdge;
        const float f = juce::jlimit (0.5f, 1.0f, (float) (groupEnd - leftStart - (leftFixed - (leftStart - r.getX()))) / comboBase);
        auto w = [f] (int v) { return (int) std::lround (v * f); };
        juce::Rectangle<int> l (leftStart, y, juce::jmax (0, groupEnd - leftStart), h);
        a.common[Bars] = l.removeFromLeft (84); l.removeFromLeft (6);
        a.root = l.removeFromLeft (w (54)); l.removeFromLeft (6);
        a.scale = l.removeFromLeft (w (128)); l.removeFromLeft (6);
        a.common[Template] = l.removeFromLeft (w (112)); l.removeFromLeft (6);
        a.chords = l.removeFromLeft (w (118)); l.removeFromLeft (6);
        a.random = l.removeFromLeft (64); l.removeFromLeft (8);
        a.common[Previous] = l.removeFromLeft (26); l.removeFromLeft (4);
        a.common[PageLabel] = l.removeFromLeft (72); l.removeFromLeft (4);
        a.common[Next] = l.removeFromLeft (26);
        if (height (width, pages) > 42)
        {
            a.common[Bars] = { leftStart, y, 84, h };
            a.root = { leftStart + 90, y, 54, h };
            a.scale = { leftStart + 150, y, 128, h };
            const int px = r.getRight() - 138;
            a.common[Previous] = { px, y, 26, h };
            a.common[PageLabel] = { px + 30, y, 72, h };
            a.common[Next] = { px + 106, y, 26, h };
            const int y2 = y + 32;
            a.common[Template] = { 14, y2, 104, h };
            a.chords = { 124, y2, 104, h };
            a.random = { 234, y2, 64, h };
            x = r.getRight() - 4;
            auto compact = [&] (juce::Rectangle<int>& c, int cw, int gap)
            { x -= cw; c = { x, y2, cw, h }; x -= gap; };
            compact (a.common[Clear], 56, 6); compact (a.common[Solo], 30, 4); compact (a.common[Mute], 30, 6);
            x -= 32; a.common[Volume] = juce::Rectangle<int> (x, y2, 32, h).withSizeKeepingCentre (32, 32);
            x -= 8;
            compact (a.bake, 52, 4); compact (a.record, 44, 0);
            a.meter = {};
        }
        return a;
    }
};
