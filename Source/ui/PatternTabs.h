#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../audio/AudioEngine.h"

// The pattern tabs above the tracks: one tab per used pattern (colour + name) and a "+" tab. Click opens a pattern,
// double-click renames, right-click gives the pattern menu. Reads names and colours from engine.patterns.
class PatternTabs : public juce::Component
{
public:
    explicit PatternTabs (AudioEngine& e) : engine (e) { setWantsKeyboardFocus (false); }

    std::function<void (int)> onSelect, onDuplicate, onClear, onDelete;
    std::function<void (int, const juce::String&)> onRenamed;
    std::function<void (int, int)> onColour;
    std::function<void()> onAdd;

    static constexpr int tabH = 28;

    void refresh() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& bank = engine.patterns;
        const int used = bank.numUsed();
        const bool plus = used < PatternBank::maxPatterns;
        const int slots = used + (plus ? 1 : 0);

        // The rail hugs its tabs and ends in a rounded cap, so it no longer runs
        // as an empty strip into the Song button with hard 90-degree corners.
        const float barW = slots > 0 ? (float) juce::jmin (getWidth(), tabRect (slots - 1).getRight()) : 0.0f;
        if (barW > 0.0f)
        {
            juce::Path bar;
            bar.addRoundedRectangle (0.0f, 0.0f, barW, (float) getHeight(),
                                     Theme::radiusLg, Theme::radiusLg, false, false, true, true);
            juce::Graphics::ScopedSaveState barState (g);
            g.reduceClipRegion (bar);
            g.setColour (Theme::panel.darker (0.30f));
            g.fillRect (getLocalBounds());
            g.setColour (Theme::panel.darker (0.45f));
            g.fillRect (0, getHeight() - 3, (int) std::ceil (barW), 3);
        }

        int pos = 0;
        for (int i = 0; i < PatternBank::maxPatterns; ++i)
        {
            if (! bank.isUsed (i)) continue;
            const auto r = tabRect (pos++).toFloat();
            const bool on = i == bank.current;
            const auto col = Theme::trackColour (bank.pats[i].colour);
            // Every tab shares one material; only the active tab carries its
            // pattern colour and the single glint.
            Theme::tabFace (g, r, on ? col : Theme::dim, on, hover == i);

            const bool data = bank.hasContent (engine, i);
            g.setFont (juce::Font (juce::FontOptions (13.0f, on ? juce::Font::bold : juce::Font::plain)));
            auto tr = r.toNearestInt().withTrimmedTop (3).reduced (8, 0);
            g.setColour (Theme::dim);
            g.drawText (juce::String (i + 1), tr.removeFromLeft (16), juce::Justification::centredLeft);
            g.setColour (on ? Theme::text : (data ? Theme::text.withAlpha (0.8f) : Theme::dim));
            g.drawText (bank.pats[i].name, tr, juce::Justification::centredLeft, true);
        }
        if (plus)
        {
            const auto r = tabRect (pos).toFloat();
            juce::Path tab;
            tab.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), Theme::radiusLg, Theme::radiusLg, true, true, false, false);
            // No solid fill and no colour in the outline: the rail shows through
            // and a grey outline gives the plus the same material as the tabs.
            g.setColour (hover == -2 ? Theme::text.withAlpha (0.75f) : Theme::dim);
            g.strokePath (tab, juce::PathStrokeType (1.0f));
            g.setColour (Theme::accent);
            g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
            g.drawText ("+", r.toNearestInt().withTrimmedTop (2), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (hit (e.x)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const int h = hit (e.x);
        if (h == -1) return;
        if (e.mods.isPopupMenu()) { if (h >= 0) showMenu (h); return; }
        if (! e.mouseWasClicked()) return;
        if (h == -2) { if (onAdd) onAdd(); }
        else if (e.getNumberOfClicks() >= 2) startRename (h);
        else if (onSelect) onSelect (h);
    }

private:
    AudioEngine& engine;
    int hover = -1;

    int tabWidth() const
    {
        const int n = engine.patterns.numUsed() + 1;
        return juce::jlimit (60, 150, (getWidth() - 4) / juce::jmax (1, n));
    }
    juce::Rectangle<int> tabRect (int pos) const
    {
        const int w = tabWidth();
        const bool plus = pos >= engine.patterns.numUsed();
        return { pos * (w + 3), 0, plus ? 34 : w, getHeight() - 3 };
    }
    // pattern index, -2 for "+", -1 for nothing
    int hit (int x) const
    {
        int pos = 0;
        for (int i = 0; i < PatternBank::maxPatterns; ++i)
        {
            if (! engine.patterns.isUsed (i)) continue;
            if (tabRect (pos++).contains (x, 1)) return i;
        }
        if (engine.patterns.numUsed() < PatternBank::maxPatterns && tabRect (pos).contains (x, 1)) return -2;
        return -1;
    }
    std::unique_ptr<juce::TextEditor> editor;
    int editing = -1;

    // type the new name straight into the tab
    void startRename (int i)
    {
        if (! engine.patterns.isUsed (i)) return;
        int pos = 0;
        for (int k = 0; k < i; ++k) if (engine.patterns.isUsed (k)) ++pos;
        const auto r = tabRect (pos);
        editing = i;
        editor = std::make_unique<juce::TextEditor>();
        editor->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        editor->setColour (juce::TextEditor::backgroundColourId, Theme::deep);
        editor->setColour (juce::TextEditor::textColourId, Theme::text);
        editor->setColour (juce::TextEditor::outlineColourId, Theme::accent);
        editor->setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
        editor->setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.4f));
        editor->setIndents (4, 3);
        editor->setInputRestrictions (24);
        editor->setText (engine.patterns.pats[(size_t) i].name, false);
        editor->onReturnKey = [this] { finishRename (true); };
        editor->onEscapeKey = [this] { finishRename (false); };
        editor->onFocusLost = [this] { finishRename (true); };
        addAndMakeVisible (*editor);
        editor->setBounds (r.withTrimmedLeft (22).reduced (2, 4));
        editor->grabKeyboardFocus();
        editor->selectAll();
    }
    void finishRename (bool commit)
    {
        if (editing < 0 || editor == nullptr) return;
        const int i = editing;
        const auto text = editor->getText().trim();
        editing = -1;
        juce::MessageManager::callAsync ([this, ed = std::shared_ptr<juce::TextEditor> (std::move (editor))] { (void) ed; repaint(); });
        if (commit && onRenamed) onRenamed (i, text);
    }

    void setHover (int h) { if (h != hover) { hover = h; repaint(); } }

    // a 4 x 4 grid of colour swatches inside the popup menu: the colour itself is the preview
    struct SwatchGrid : juce::PopupMenu::CustomComponent
    {
        SwatchGrid (int current, std::function<void (int)> pick) : juce::PopupMenu::CustomComponent (false), cur (current), onPick (std::move (pick)) {}
        void getIdealSize (int& w, int& h) override { w = 4 * cell + 2 * margin; h = 4 * cell + 2 * margin; }
        juce::Rectangle<float> swatch (int c) const { return juce::Rectangle<float> ((float) (margin + (c % 4) * cell), (float) (margin + (c / 4) * cell), (float) cell, (float) cell).reduced (3.0f); }
        int swatchAt (juce::Point<int> p) const { for (int c = 0; c < 16; ++c) if (swatch (c).expanded (3.0f).contains (p.toFloat())) return c; return -1; }
        void paint (juce::Graphics& g) override
        {
            for (int c = 0; c < 16; ++c)
            {
                const auto r = swatch (c);
                g.setColour (Theme::trackColour (c));
                g.fillRoundedRectangle (r, Theme::radius);
                if (c == cur) { g.setColour (Theme::text); g.drawRoundedRectangle (r.expanded (2.0f), Theme::radius + 1.0f, 2.0f); }
                else if (c == hover) { g.setColour (Theme::text.withAlpha (0.6f)); g.drawRoundedRectangle (r.expanded (2.0f), Theme::radius + 1.0f, 1.0f); }
            }
        }
        void mouseMove (const juce::MouseEvent& e) override { const int h = swatchAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const int c = swatchAt (e.getPosition());
            if (c < 0) return;
            if (onPick) onPick (c);
            triggerMenuItem();
        }
        static constexpr int cell = 28, margin = 6;
        int cur, hover = -1;
        std::function<void (int)> onPick;
    };
    void showMenu (int i)
    {
        juce::PopupMenu colours;
        {
            juce::PopupMenu::Item sw;
            sw.itemID = 199;
            sw.customComponent = new SwatchGrid (engine.patterns.pats[(size_t) i].colour, [this, i] (int c) { if (onColour) onColour (i, c); });
            colours.addItem (sw);
        }
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addSubMenu ("Colour", colours);
        m.addItem (2, "Duplicate", engine.patterns.numUsed() < PatternBank::maxPatterns);
        m.addItem (3, "Clear audio clips");
        m.addSeparator();
        m.addItem (4, "Delete pattern", engine.patterns.numUsed() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this, i] (int r)
                         {
                             if (r == 1) startRename (i);
                             else if (r == 2 && onDuplicate) onDuplicate (i);
                             else if (r == 3 && onClear) onClear (i);
                             else if (r == 4 && onDelete) onDelete (i);
                             
                         });
    }
};
