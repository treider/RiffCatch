#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// "DRUMS | BASS" tabs in the header of the step-sequencer panels (the two panels share one slot).
// Drawn like the Notes button: a framed tile, filled with the primary colour when that sequencer holds data.
// The open tab gets a light inner outline.
class SeqTabs : public juce::Component
{
public:
    SeqTabs (int selectedTab, juce::Colour) : selected (selectedTab) { setWantsKeyboardFocus (false); }
    std::function<void (int)> onSelect;
    std::function<bool (int)> hasData;                     // is there a pattern in tab 0 (drums) / 1 (bass)?

    void refresh()
    {
        const int s = (hasData && hasData (0) ? 1 : 0) | (hasData && hasData (1) ? 2 : 0);
        if (s != lastState) { lastState = s; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const char* const names[2] = { "Drums", "Bass" };
        const float w = (float) (getWidth() - 4) * 0.5f;
        for (int i = 0; i < 2; ++i)
        {
            const auto r = juce::Rectangle<float> ((float) i * (w + 4.0f), 0.0f, w, (float) getHeight());
            const bool on = i == selected, data = hasData && hasData (i);
            auto fill = data ? Theme::accent : Theme::raised;
            if (hover == i) fill = fill.brighter (0.12f);
            Theme::framedTile (g, r, fill, 2.5f);
            if (on)
            {
                g.setColour ((data ? Theme::onAccent : Theme::text).withAlpha (0.75f));
                g.drawRoundedRectangle (r.reduced (4.5f), Theme::radius, 1.0f);
            }
            g.setColour (data ? Theme::onAccent : Theme::text);
            g.setFont (juce::Font (juce::FontOptions (13.0f, on ? juce::Font::bold : juce::Font::plain)));
            g.drawText (names[i], r.toNearestInt(), juce::Justification::centred);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = tabAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = tabAt (e.x);
        if (e.mouseWasClicked() && i != selected && onSelect) onSelect (i);
    }

private:
    int tabAt (int x) const { return x < getWidth() / 2 ? 0 : 1; }
    int selected = 0, hover = -1, lastState = -1;
};