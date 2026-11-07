#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// "Drums | Bass" tabs in the header of the step-sequencer panels (the two panels share one slot), drawn like browser tabs:
// rounded tops, the open tab in the panel colour and joined to the panel body, the others darker on a darker tab bar.
// A tab whose sequencer holds a pattern gets its label in the primary colour and a dot.
class SeqTabs : public juce::Component
{
public:
    SeqTabs (int selectedTab, juce::Colour accentColour) : selected (selectedTab), accent (accentColour) { setWantsKeyboardFocus (false); }
    std::function<void (int)> onSelect;
    std::function<bool (int)> hasData;                     // is there a pattern in tab 0 (drums) / 1 (bass)?

    // the darker bar the tabs stand on (panels fill their header with this)
    static juce::Colour barColour() { return Theme::panel.darker (0.30f); }

    void refresh()
    {
        const int s = (hasData && hasData (0) ? 1 : 0) | (hasData && hasData (1) ? 2 : 0);
        if (s != lastState) { lastState = s; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const char* const names[2] = { "Drums", "Bass" };
        const float w = (float) (getWidth() - 3) * 0.5f;
        for (int i = 0; i < 2; ++i)
        {
            const auto r = juce::Rectangle<float> ((float) i * (w + 3.0f), 0.0f, w, (float) getHeight());
            const bool on = i == selected, data = hasData && hasData (i);
            // Same material as the pattern tabs: Drums uses the Primary role and
            // Bass the Secondary role when active; the inactive tab is grey.
            const auto rim = on ? (i == 0 ? Theme::drumAccent() : Theme::bassAccent()) : Theme::dim;
            Theme::tabFace (g, r, rim, on, hover == i);

            g.setFont (juce::Font (juce::FontOptions (13.0f, on ? juce::Font::bold : juce::Font::plain)));
            g.setColour (data ? Theme::accent : (on ? Theme::text : Theme::dim));
            auto textR = r.toNearestInt().withTrimmedTop (3);
            if (data) { g.fillEllipse (textR.getX() + 9.0f, (float) textR.getCentreY() - 3.0f, 6.0f, 6.0f); textR.removeFromLeft (10); }
            if (i == 1)
            {
                const auto fillColour = data ? Theme::accent : (on ? Theme::text : Theme::dim);
                juce::GlyphArrangement glyphs;
                // Match Graphics::drawText's arrangement exactly (not fitted text).
                glyphs.addCurtailedLineOfText (g.getCurrentFont(), names[i], 0.0f, 0.0f, (float) textR.getWidth(), true);
                glyphs.justifyGlyphs (0, glyphs.getNumGlyphs(), (float) textR.getX(), (float) textR.getY(),
                                     (float) textR.getWidth(), (float) textR.getHeight(), juce::Justification::centred);
                juce::Path glyphPath;
                glyphs.createPath (glyphPath);
                g.setColour (Theme::bassGlyphContour());
                g.strokePath (glyphPath, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (fillColour);
                g.drawText (names[i], textR, juce::Justification::centred);
            }
            else
                g.drawText (names[i], textR, juce::Justification::centred);
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
    juce::Colour accent;
};
