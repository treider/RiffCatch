#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "SegmentDisplay.h"
#include "../audio/AudioEngine.h"

// Which drum (D01) or bass (B01) pattern of the global library the open pattern plays, shown on a seven-segment display like
// an old countdown timer, with an up and a down arrow. The arrows (and the mouse wheel) only count up and down: a number that
// does not exist yet starts as a new empty pattern, and an empty one nobody plays disappears again when you leave it.
// The library is global: several song patterns may play the same item, and the counter shows just that item's number.
class LibraryCounter : public juce::Component, public juce::SettableTooltipClient
{
public:
    LibraryCounter (AudioEngine& e, PatternBank::Kind k, juce::Colour accentColour) : engine (e), kind (k), accent (accentColour) { setWantsKeyboardFocus (false); }

    std::function<void (int)> onStep;          // +1 / -1
    std::function<void()> onCopy;              // copy what plays now to the next free number

    static constexpr int panelX = 46;          // x of the counter in the header of both the drum and the bass panel
    static constexpr int displayW = 64, arrowW = 22, margin = 3, gapToArrows = 5, totalW = margin + displayW + gapToArrows + arrowW;

    void refresh() { repaint(); updateTip(); }

    void paint (juce::Graphics& g) override
    {
        const auto& bank = engine.patterns;
        const int cur = bank.currentRef (kind);
        const auto d = displayRect().toFloat();
        Seg::paintDisplayHousing (g, d.expanded (3.0f), Theme::radiusLg + 1.5f);


        // Same brightness for Drums and Bass: the number is the state, not the glow.
        const auto lit = Seg::digitColour;
        const float dh = d.getHeight() - 8.0f, gapX = dh * 0.3f;
        const float y = d.getY() + 4.0f, dw = Seg::glyphWidth (dh);
        const float blockW = dw * 3.0f + gapX * 2.4f;      // "D01" / "B16", centred in the display
        float x = d.getX() + (d.getWidth() - blockW) * 0.5f;
        Seg::glyph (g, x, y, dh, kind == PatternBank::Drums ? Seg::maskD : Seg::maskB, lit); x += dw + gapX * 1.4f;
        Seg::glyph (g, x, y, dh, Seg::digitMask ((cur + 1) / 10), lit); x += dw + gapX;
        Seg::glyph (g, x, y, dh, Seg::digitMask ((cur + 1) % 10), lit);
        for (int i = 0; i < 2; ++i)                       // up arrow on top, down arrow below
        {
            const auto r = arrowRect (i).toFloat();
            const bool over = hover == i;
            if (Theme::materialControl (*this))
            {
                {
                    juce::Graphics::ScopedSaveState shadowState (g);
                    // Reserve the blue display housing.
                    g.reduceClipRegion (getLocalBounds().withTrimmedLeft (displayRect().getRight() + 3));
                    Theme::bassControlShadow (g, r, isEnabled(), pressed == i);
                }
                Theme::bassControlTile (g, r, Theme::raised, 2.0f, isEnabled(), over, pressed == i, hasKeyboardFocus (true));
            }
            else
                Theme::framedTile (g, r, over ? Theme::raised.brighter (0.12f) : Theme::raised, 2.0f);
            juce::Path p;
            const auto c = r.getCentre();
            if (i == 0) p.addTriangle (c.x - 4.5f, c.y + 2.0f, c.x + 4.5f, c.y + 2.0f, c.x, c.y - 3.0f);
            else        p.addTriangle (c.x - 4.5f, c.y - 2.0f, c.x + 4.5f, c.y - 2.0f, c.x, c.y + 3.0f);
            g.setColour (over ? themeColour() : Theme::text);
            g.fillPath (p);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { setHover (arrowAt (e.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        pressed = arrowAt (e.getPosition());
        if (Theme::materialControl (*this)) repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        pressed = -1;
        if (Theme::materialControl (*this)) repaint();
        if (e.mods.isPopupMenu() && displayRect().contains (e.getPosition()))
        {
            juce::PopupMenu m;
            m.addItem (1, "Copy to a new pattern", engine.patterns.numItems (kind) < PatternBank::maxItems);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r == 1 && onCopy) onCopy(); });
            return;
        }
        const int a = arrowAt (e.getPosition());
        if (a >= 0 && e.mouseWasClicked() && onStep) onStep (a == 0 ? 1 : -1);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        wheelAcc += w.deltaY * 4.0f;
        const int n = (int) wheelAcc;
        if (n == 0) return;
        wheelAcc -= (float) n;
        if (onStep) onStep (n > 0 ? 1 : -1);
    }

private:
    AudioEngine& engine;
    PatternBank::Kind kind;
    juce::Colour themeColour() const { return kind == PatternBank::Drums ? Theme::orange : Theme::cyan; }     // read at paint time so theme changes apply
    juce::Colour accent;
    int hover = -1;
    int pressed = -1;
    float wheelAcc = 0.0f;

    // the display keeps its full width so the arrows never move; the ring around it needs a few pixels of margin
    juce::Rectangle<int> displayRect() const
    {
        return { margin, margin, displayW, getHeight() - 2 * margin };
    }
    juce::Rectangle<int> arrowRect (int i) const        // the two arrows together are as tall as the controls next to the counter
    {
        const auto d = displayRect();
        const int h = (getHeight() - 2) / 2;
        return { d.getRight() + gapToArrows, i * (h + 2), arrowW, h };
    }    int arrowAt (juce::Point<int> p) const { for (int i = 0; i < 2; ++i) if (arrowRect (i).contains (p)) return i; return -1; }
    void setHover (int h) { if (h != hover) { hover = h; repaint(); } }
    void updateTip()
    {
        const int cur = engine.patterns.currentRef (kind);
        setTooltip ((kind == PatternBank::Drums ? "Drum pattern D" : "Bass pattern B") + juce::String (cur + 1)
                    + ". Arrows or wheel: next / previous number; a new number starts empty. Right-click: copy");
    }
};