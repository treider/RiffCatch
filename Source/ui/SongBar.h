#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../audio/AudioEngine.h"

// The song: one row of pattern blocks in play order. A block is as wide as the bars it plays (loop length x repeats).
// Click opens the pattern for editing, drag moves it, the wheel changes the repeats, right-click gives the block menu,
// "+" adds a pattern at the end. While the song plays a line shows where it is.
class SongBar : public juce::Component
{
public:
    explicit SongBar (AudioEngine& e) : engine (e) { setWantsKeyboardFocus (false); }

    std::function<void()> onChanged;                 // the song was edited
    std::function<void (int)> onOpenPattern;         // a block was clicked

    static constexpr int barH = 38;

    // -1 when the song is not playing
    void setPlayhead (juce::int64 samples)
    {
        if (samples != playhead) { playhead = samples; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& bank = engine.patterns;
        auto well = getLocalBounds().toFloat().reduced (3.0f);
        Theme::wellRing (g, well, 3.0f);
        g.setColour (Theme::deep);
        g.fillRoundedRectangle (well, Theme::radius);

        const auto layout = blocks();
        if (bank.song.empty())
        {
            g.setColour (Theme::dim);
            g.setFont (juce::Font (juce::FontOptions (12.5f)));
            g.drawText ("The song is empty. Add patterns with +, then press Play to hear them one after the other.",
                        getLocalBounds().withTrimmedLeft (12).withTrimmedRight (plusW + 10), juce::Justification::centredLeft);
        }
        const juce::int64 barLen = juce::jmax<juce::int64> (1, engine.getBarLength());
        for (size_t i = 0; i < layout.size(); ++i)
        {
            const auto& c = bank.song[i];
            const auto& p = bank.pats[(size_t) c.pattern];
            const auto r = layout[i].toFloat();
            const auto col = Theme::trackColour (p.colour);
            const bool lifted = (int) i == dragging;
            g.setColour (col.withAlpha (lifted ? 0.7f : (hover == (int) i ? 1.0f : 0.88f)));
            g.fillRoundedRectangle (r, Theme::radius);
            g.setColour (col.contrasting (0.85f).withAlpha (0.9f));
            g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
            auto label = p.name;
            if (c.repeats > 1) label << "  " << juce::String::fromUTF8 ("\xc3\x97") << c.repeats;
            g.drawText (label, layout[i].reduced (7, 0).withTrimmedRight (13), juce::Justification::centredLeft, true);
            drawClose (g, closeRect (layout[i]).toFloat(), hover == (int) i && closeHover);
        }
        if (bank.song.size() < 256)
        {
            const auto r = plusRect().toFloat();
            Theme::framedTile (g, r, hover == plusHover ? Theme::raised.brighter (0.1f) : Theme::raised, 2.0f);
            g.setColour (Theme::accent);
            g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
            g.drawText ("+", r.toNearestInt(), juce::Justification::centred);
        }
        if (playhead >= 0)    // where the song is now
        {
            double bars = (double) playhead / (double) barLen, acc = 0.0;
            for (size_t i = 0; i < layout.size(); ++i)
            {
                const auto& c = bank.song[i];
                const double n = (double) bank.pats[(size_t) c.pattern].loopBars * c.repeats;
                if (bars < acc + n || i + 1 == layout.size())
                {
                    const double f = juce::jlimit (0.0, 1.0, (bars - acc) / juce::jmax (0.001, n));
                    const float x = layout[i].getX() + (float) (f * layout[i].getWidth());
                    g.setColour (Theme::text);
                    g.fillRect (x - 1.0f, (float) layout[i].getY() - 3.0f, 2.0f, (float) layout[i].getHeight() + 6.0f);
                    break;
                }
                acc += n;
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hit (e.getPosition());
        const bool ch = closeBlockAt (e.getPosition()) >= 0;
        setHover (h, ch);
    }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1, false); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (closeBlockAt (e.getPosition()) >= 0) { pressed = -1; return; }
        const int h = hit (e.getPosition());
        if (h >= 0 && h != plusHover) { dragging = -1; pressed = h; }
        else pressed = -1;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pressed < 0 || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6) return;
        auto& song = engine.patterns.song;
        dragging = pressed;
        const auto layout = blocks();
        const int x = e.x;
        int target = pressed;                       // the block that would be at the mouse
        for (size_t i = 0; i < layout.size(); ++i) if (x >= layout[i].getX() && x < layout[i].getRight()) target = (int) i;
        if (target != pressed && target >= 0 && target < (int) song.size())
        {
            const auto moved = song[(size_t) pressed];
            song.erase (song.begin() + pressed);
            song.insert (song.begin() + target, moved);
            pressed = dragging = target;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const int h = hit (e.getPosition());
        if (! e.mods.isPopupMenu() && dragging < 0)   // a drag must never turn into a delete
        {
            const int cb = closeBlockAt (e.getPosition());
            if (cb >= 0)
            {
                dragging = -1; pressed = -1;
                if (e.mouseWasClicked()) removeBlock (cb);
                return;
            }
        }
        const bool moved = dragging >= 0;
        dragging = -1;
        if (moved) { pressed = -1; changed(); repaint(); return; }
        pressed = -1;
        if (h == plusHover && ! e.mods.isPopupMenu()) { showAddMenu(); return; }
        if (h < 0 || h == plusHover) return;
        if (e.mods.isPopupMenu()) showBlockMenu (h);
        else if (e.mouseWasClicked() && onOpenPattern) onOpenPattern (engine.patterns.song[(size_t) h].pattern);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hit (e.getPosition());
        if (h < 0 || w.deltaY == 0.0f) return;
        auto& c = engine.patterns.song[(size_t) h];
        c.repeats = juce::jlimit (1, 64, c.repeats + (w.deltaY > 0.0f ? 1 : -1));
        changed(); repaint();
    }

private:
    static constexpr int plusW = 36, gap = 4, minBlockW = 34, plusHover = 1000;
    AudioEngine& engine;
    int hover = -1, pressed = -1, dragging = -1;
    bool closeHover = false;
    juce::int64 playhead = -1;

    void changed() { if (onChanged) onChanged(); }
    void setHover (int h, bool ch) { if (h != hover || ch != closeHover) { hover = h; closeHover = ch; repaint(); } }

    static juce::Rectangle<int> closeRect (juce::Rectangle<int> block)
    {
        return { block.getRight() - 15, block.getY() + 3, 12, 12 };
    }
    static void drawClose (juce::Graphics& g, juce::Rectangle<float> r, bool hot)
    {
        g.setColour (Theme::deep.withAlpha (hot ? 0.55f : 0.35f));
        g.fillRoundedRectangle (r.expanded (1.0f), 3.0f);
        g.setColour (hot ? Theme::red : Theme::red.withAlpha (0.85f));
        const float in = 3.5f;
        g.drawLine (r.getX() + in, r.getY() + in, r.getRight() - in, r.getBottom() - in, 1.6f);
        g.drawLine (r.getRight() - in, r.getY() + in, r.getX() + in, r.getBottom() - in, 1.6f);
    }
    int closeBlockAt (juce::Point<int> p) const
    {
        const auto b = blocks();
        for (size_t i = 0; i < b.size(); ++i) if (closeRect (b[i]).contains (p)) return (int) i;
        return -1;
    }
    // removing a block only takes the pattern out of the song, the pattern itself stays
    void removeBlock (int i)
    {
        auto& song = engine.patterns.song;
        if (i < 0 || i >= (int) song.size()) return;
        song.erase (song.begin() + i);
        hover = -1; closeHover = false;
        changed(); repaint();
    }
    juce::Rectangle<int> plusRect() const
    {
        const auto b = blocks();
        const int x = b.empty() ? 8 : b.back().getRight() + gap;
        return { juce::jmin (x, getWidth() - plusW - 6), 6, plusW, getHeight() - 12 };
    }

    // the blocks' rectangles: as wide as the bars they play, shrunk to fit
    std::vector<juce::Rectangle<int>> blocks() const
    {
        const auto& bank = engine.patterns;
        std::vector<juce::Rectangle<int>> r;
        const int avail = getWidth() - 16 - plusW - gap;
        double ppb = 40.0;
        auto totalFor = [&] (double pp)
        {
            double t = 0.0;
            for (auto& c : bank.song) t += juce::jmax ((double) minBlockW, bank.pats[(size_t) c.pattern].loopBars * c.repeats * pp) + gap;
            return t;
        };
        while (ppb > 1.0 && totalFor (ppb) > avail) ppb -= 0.5;
        int x = 8;
        for (auto& c : bank.song)
        {
            const int w = (int) juce::jmax ((double) minBlockW, bank.pats[(size_t) c.pattern].loopBars * c.repeats * ppb);
            r.push_back ({ x, 6, w, getHeight() - 12 });
            x += w + gap;
        }
        return r;
    }
    int hit (juce::Point<int> p) const
    {
        const auto b = blocks();
        for (size_t i = 0; i < b.size(); ++i) if (b[i].contains (p)) return (int) i;
        if (engine.patterns.song.size() < 256 && plusRect().contains (p)) return plusHover;
        return -1;
    }

    void showAddMenu()
    {
        juce::PopupMenu m;
        for (int i = 0; i < PatternBank::maxPatterns; ++i)
            if (engine.patterns.isUsed (i))
                m.addColouredItem (100 + i, engine.patterns.pats[(size_t) i].name, Theme::trackColour (engine.patterns.pats[(size_t) i].colour));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r >= 100 && engine.patterns.isUsed (r - 100)) { engine.patterns.song.push_back ({ r - 100, 1 }); changed(); repaint(); }
        });
    }

    void showBlockMenu (int h)
    {
        auto& bank = engine.patterns;
        juce::PopupMenu reps, change;
        for (int n : { 1, 2, 3, 4, 6, 8, 12, 16 }) reps.addItem (200 + n, juce::String (n) + (n == 1 ? " time" : " times"), true, bank.song[(size_t) h].repeats == n);
        for (int i = 0; i < PatternBank::maxPatterns; ++i)
            if (bank.isUsed (i)) change.addColouredItem (300 + i, bank.pats[(size_t) i].name, Theme::trackColour (bank.pats[(size_t) i].colour), true, bank.song[(size_t) h].pattern == i);
        juce::PopupMenu m;
        m.addSubMenu ("Play", reps);
        m.addSubMenu ("Change pattern", change);
        m.addItem (1, "Duplicate", bank.song.size() < 256);
        m.addItem (2, "Remove");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, h] (int r)
        {
            auto& song = engine.patterns.song;
            if (h >= (int) song.size()) return;
            if (r >= 200 && r < 300) song[(size_t) h].repeats = r - 200;
            else if (r >= 300 && engine.patterns.isUsed (r - 300)) song[(size_t) h].pattern = r - 300;
            else if (r == 1) song.insert (song.begin() + h + 1, song[(size_t) h]);
            else if (r == 2) song.erase (song.begin() + h);
            else return;
            changed(); repaint();
        });
    }
};