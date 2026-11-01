#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <tuple>

// Colour themes: six roles (Primary, Secondary, Accent, Background, Surface, Text) are mapped onto the app palette.
// Red (record), yellow/orange (warnings, info) and green are fixed and never follow the theme.
namespace Theme
{
    inline juce::Colour bg     { 0xff1a1b26 };
    inline juce::Colour panel  { 0xff1f2133 };
    inline juce::Colour deep   { 0xff14151d };
    inline juce::Colour raised { 0xff292e42 };
    inline juce::Colour border { 0xff2f334d };
    inline juce::Colour text   { 0xffc0caf5 };
    inline juce::Colour dim    { 0xff565f89 };
    inline juce::Colour blue   { 0xff7aa2f7 };     // Primary
    inline juce::Colour accent { 0xff7aa2f7 };     // the one UI colour for controls (= Primary)
    inline juce::Colour cyan   { 0xff7dcfff };     // Accent
    inline juce::Colour purple { 0xffbb9af7 };     // Secondary (master)
    inline juce::Colour onAccent { 0xff14151d };   // text/icons drawn on a filled accent
    inline juce::Colour frame  { 0xff14151d };     // dark ring around buttons and boxes
    inline const juce::Colour green  { 0xff9ece6a };
    inline const juce::Colour yellow { 0xffe0af68 };
    inline const juce::Colour orange { 0xffff9e64 };
    inline const juce::Colour red    { 0xfff7768e };
    inline const juce::Colour teal   { 0xff73daca };
    inline bool legacyTracks = true;               // Tokyo Night keeps its original eight clip colours

    inline constexpr float radius = 4.0f;     // controls: buttons, boxes, menus, lanes
    inline constexpr float radiusLg = 6.0f;   // panels and track cards
    // Soft drop shadow under cards - tweak these three to taste
    inline constexpr float shadowOpacity = 0.22f;   // 0 = off, ~0.4 = strong
    inline constexpr int   shadowRadius  = 8;       // blur size in px
    inline constexpr int   shadowOffsetY = 2;       // how far the shadow falls downwards

    struct Preset { juce::String name; juce::uint32 primary, secondary, accent, background, surface, text; };
    inline std::vector<Preset> presets =   // the nine built-ins first, then user themes loaded from disk
    {
        { "Tokyo Night", 0xff7aa2f7, 0xffbb9af7, 0xff7dcfff, 0xff1a1b26, 0xff1f2133, 0xffc0caf5 },
        { "Ocean",       0xff2563eb, 0xff0ea5e9, 0xff06b6d4, 0xff0f172a, 0xff1e293b, 0xfff1f5f9 },
        { "Forest",      0xff059669, 0xff10b981, 0xff34d399, 0xff022c22, 0xff064e3b, 0xffd1fae5 },
        { "Sunset",      0xffea580c, 0xfff59e0b, 0xfffbbf24, 0xff1c1917, 0xff292524, 0xfffef3c7 },
        { "Midnight",    0xff6366f1, 0xff8b5cf6, 0xffa78bfa, 0xff0c0a1d, 0xff1e1b4b, 0xffe0e7ff },
        { "Rose",        0xffbe123c, 0xffe11d48, 0xfffb7185, 0xff1c1917, 0xff3f1f1f, 0xffffe4e6 },
        { "Lavender",    0xff8b5cf6, 0xffa78bfa, 0xffc4b5fd, 0xff1e1b4b, 0xff312e81, 0xffe9d5ff },
        { "Coral",       0xfff43f5e, 0xfffb7185, 0xfffda4af, 0xff451a1a, 0xff7f1d1d, 0xffffe4e6 },
        { "Mint",        0xff14b8a6, 0xff2dd4bf, 0xff5eead4, 0xff134e4a, 0xff0f766e, 0xffccfbf1 },
    };
    inline constexpr int builtinCount = 9;
    inline int numPresets() { return (int) presets.size(); }
    inline int current = 3;   // Sunset

    inline void applyPreset (const Preset& p, bool legacy)
    {
        const juce::Colour B (p.background), S (p.surface), T (p.text);
        blue = accent = juce::Colour (p.primary);
        purple = juce::Colour (p.secondary);
        cyan = juce::Colour (p.accent);
        text = T;
        legacyTracks = legacy;
        if (legacy)   // the original palette, exactly
        {
            bg = B; panel = S;
            deep = juce::Colour (0xff14151d); raised = juce::Colour (0xff292e42); border = juce::Colour (0xff2f334d); dim = juce::Colour (0xff565f89);
        }
        else
        {
            bg = B; panel = S;
            deep = B.darker (0.35f);
            raised = S.interpolatedWith (T, 0.10f);
            border = S.interpolatedWith (T, 0.18f);
            dim = B.interpolatedWith (T, 0.50f);
        }
        frame = deep;
        onAccent = accent.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff14151d) : juce::Colour (0xfff8fafc);
    }
    inline void apply (int index)
    {
        index = juce::jlimit (0, numPresets() - 1, index);
        current = index;
        applyPreset (presets[(size_t) index], index == 0);
    }

    // Build a full theme from just a primary and an accent colour. The base is tinted with the primary hue.
    inline Preset generate (const juce::String& name, juce::Colour primary, juce::Colour accentCol, bool dark)
    {
        const float h = primary.getHue();
        const float s = juce::jlimit (0.12f, 0.45f, primary.getSaturation() * 0.5f);
        const juce::Colour bgC   = dark ? juce::Colour::fromHSV (h, s, 0.10f, 1.0f) : juce::Colour::fromHSV (h, s * 0.25f, 0.96f, 1.0f);
        const juce::Colour surfC = dark ? juce::Colour::fromHSV (h, s, 0.17f, 1.0f) : juce::Colour::fromHSV (h, s * 0.35f, 0.89f, 1.0f);
        const juce::Colour textC = dark ? juce::Colour::fromHSV (h, 0.12f, 0.95f, 1.0f) : juce::Colour::fromHSV (h, 0.30f, 0.14f, 1.0f);
        return { name, primary.getARGB(), primary.withRotatedHue (0.08f).getARGB(), accentCol.getARGB(),
                 bgC.getARGB(), surfC.getARGB(), textC.getARGB() };
    }

    inline juce::Colour trackColour (int i)
    {
        juce::Colour c[8];
        if (legacyTracks)
        {
            const juce::Colour l[] = { juce::Colour (0xff7aa2f7), juce::Colour (0xff7dcfff), juce::Colour (0xff9ece6a), juce::Colour (0xffe0af68),
                                       juce::Colour (0xffff9e64), juce::Colour (0xffbb9af7), juce::Colour (0xff73daca), juce::Colour (0xfff7768e) };
            for (int k = 0; k < 8; ++k) c[k] = l[k];
        }
        else   // hues around the theme's own colours
        {
            const juce::Colour base[3] = { blue, cyan, purple };
            for (int k = 0; k < 8; ++k)
            {
                const float shift = k < 3 ? 0.0f : (k < 6 ? 0.09f : -0.09f);
                c[k] = base[k % 3].withRotatedHue (shift);
            }
        }
        return i % 16 < 8 ? c[i % 8] : c[i % 8].darker (0.3f);   // second set of 8: same hues, deeper
    }

    // Flat button tile inside a dark ring (like the reference: a darker frame around every control).
    inline void framedTile (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float ring = 2.5f)
    {
        const float ro = juce::jmin (7.0f, r.getHeight() * 0.3f);
        g.setColour (frame);
        g.fillRoundedRectangle (r, ro);
        g.setColour (fill);
        g.fillRoundedRectangle (r.reduced (ring), juce::jmax (2.0f, ro - ring * 0.6f));
    }
    // Soft card shadow. The blur is expensive, so it is rendered once per card size and then drawn as an image
    // (cardShadow runs on every repaint of a parent, which happens many times a second while playing).
    inline void cardShadow (juce::Graphics& g, juce::Rectangle<float> r)
    {
        if (shadowOpacity <= 0.0f || r.isEmpty()) return;
        static std::map<std::tuple<int, int, int>, juce::Image> cache;
        const int w = (int) std::ceil (r.getWidth()), h = (int) std::ceil (r.getHeight());
        const std::tuple<int, int, int> key { w, h, (int) std::lround (radiusLg * 10.0f) };
        const int pad = shadowRadius * 2 + std::abs (shadowOffsetY) + 2;
        auto it = cache.find (key);
        if (it == cache.end())
        {
            if (cache.size() > 48) cache.clear();
            juce::Image img (juce::Image::ARGB, w + 2 * pad, h + 2 * pad, true);
            juce::Graphics ig (img);
            juce::Path p;
            p.addRoundedRectangle ((float) pad, (float) pad, (float) w, (float) h, radiusLg);
            juce::DropShadow (juce::Colours::black.withAlpha (shadowOpacity), shadowRadius, { 0, shadowOffsetY }).drawForPath (ig, p);
            it = cache.emplace (key, img).first;
        }
        g.drawImageAt (it->second, (int) std::floor (r.getX()) - pad, (int) std::floor (r.getY()) - pad);
    }
    // Ring around a dark well (lanes, meters): lighter than the dark inside it
    inline void wellRing (juce::Graphics& g, juce::Rectangle<float> r, float grow = 3.0f)
    {
        g.setColour (border);
        g.fillRoundedRectangle (r.expanded (grow), radiusLg + grow * 0.5f);
    }
}
class RiffLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RiffLookAndFeel() { applyColours(); }

    void applyColours()
    {
        using namespace Theme;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::DocumentWindow::textColourId, text);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Label::textWhenEditingColourId, text);
        setColour (juce::Label::backgroundWhenEditingColourId, deep);
        setColour (juce::Label::outlineWhenEditingColourId, blue);
        setColour (juce::TextEditor::backgroundColourId, deep);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, blue.withAlpha (0.35f));
        setColour (juce::TextEditor::outlineColourId, border);
        setColour (juce::TextEditor::focusedOutlineColourId, blue);
        setColour (juce::CaretComponent::caretColourId, blue);
        setColour (juce::TextButton::buttonColourId, raised);
        setColour (juce::TextButton::buttonOnColourId, blue);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, onAccent);
        setColour (juce::ComboBox::backgroundColourId, raised);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::ComboBox::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::PopupMenu::backgroundColourId, panel.withAlpha (0.99f));   // not fully opaque: lets the menu window have rounded corners
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, blue.withAlpha (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::ScrollBar::thumbColourId, border);
        setColour (juce::Slider::textBoxTextColourId, dim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, border);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ListBox::textColourId, text);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, border);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, blue);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 8; }

    // Same flat, rounded look as the buttons (no outline)
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto b = juce::Rectangle<float> ((float) w, (float) h);
        Theme::framedTile (g, b, box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f), 2.0f);
        juce::Path arrow;
        const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
        arrow.lineTo (cx, cy + 2.0f);
        arrow.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (arrow, juce::PathStrokeType (1.6f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                               bool over, bool down) override
    {
        auto c = bgc;
        if (down) c = c.darker (0.15f); else if (over) c = c.brighter (0.12f);
        const auto r = b.getLocalBounds().toFloat();
        Theme::framedTile (g, r, c, r.getHeight() < 26.0f ? 2.0f : 2.5f);
        if (b.getProperties().contains ("outline"))
        {
            g.setColour (juce::Colour ((juce::uint32) (int) b.getProperties()["outline"]));
            g.drawRoundedRectangle (r.reduced (2.0f), Theme::radius, 1.0f);
        }
    }

    // ---- popup menus / dropdowns: rounded like everything else, same border as the bars
    int getPopupMenuBorderSize() override { return 4; }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        const auto b = juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (b, Theme::radiusLg);
        g.setColour (Theme::border.withAlpha (0.55f));
        g.drawRoundedRectangle (b, Theme::radiusLg, 1.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (Theme::border);
            g.fillRect (area.getX() + 8, area.getCentreY(), area.getWidth() - 16, 1);
            return;
        }
        auto r = area.reduced (1, 0);
        if (isHighlighted && isActive)
        {
            g.setColour (Theme::blue.withAlpha (0.28f));
            g.fillRoundedRectangle (r.toFloat(), Theme::radius);
        }
        g.setColour (isActive ? Theme::text : Theme::dim.withAlpha (0.6f));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        r.reduce (12, 0);
        if (isTicked)
            g.fillEllipse ((float) r.getX() - 8.0f, (float) r.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.drawText (text, r, juce::Justification::centredLeft);
        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (Theme::dim);
            g.drawText (shortcutKeyText, r, juce::Justification::centredRight);
        }
        if (hasSubMenu)
        {
            juce::Path p;
            const float cx = (float) r.getRight() - 4.0f, cy = (float) r.getCentreY();
            p.addTriangle (cx - 3.0f, cy - 4.0f, cx - 3.0f, cy + 4.0f, cx + 2.0f, cy);
            g.fillPath (p);
        }
    }

    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& t) override
    {
        g.setColour (t.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, Theme::radius);
    }
    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& t) override
    {
        if (! t.isEnabled()) return;
        g.setColour (t.findColour (t.hasKeyboardFocus (true) ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, Theme::radius, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style == juce::Slider::LinearBar)   // value box: flat rounded, like the buttons
        {
            g.setColour (Theme::raised);
            g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), Theme::radius);
            return;
        }
        if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
            return;
        }
        const auto accent = s.findColour (juce::Slider::trackColourId);
        if (style == juce::Slider::LinearVertical)
        {
            const float cx = x + w * 0.5f;
            juce::Rectangle<float> groove (cx - 2.0f, (float) y, 4.0f, (float) h);
            g.setColour (Theme::deep);
            g.fillRoundedRectangle (groove, 1.0f);
            g.setColour (accent.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
            g.fillRoundedRectangle (groove.withTop (pos), 1.0f);

            g.setColour (Theme::text);
            g.fillEllipse (cx - 7.0f, pos - 7.0f, 14.0f, 14.0f);
        }
        else
        {
            const float cy = y + h * 0.5f;
            juce::Rectangle<float> groove ((float) x, cy - 2.0f, (float) w, 4.0f);
            g.setColour (Theme::deep);
            g.fillRoundedRectangle (groove, 1.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (groove.withRight (pos), 1.0f);
            g.setColour (Theme::text);
            g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        }
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float prop,
                           float startA, float endA, juce::Slider& s) override
    {
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = startA + prop * (endA - startA);
        const float mid = 0.5f * (startA + endA);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

        g.setColour (Theme::frame);                               // dark ring around every knob, like the buttons
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (Theme::raised);                              // lighter face so the knob stands out from the panel
        g.fillEllipse (c.x - r + 5.5f, c.y - r + 5.5f, 2 * r - 11.0f, 2 * r - 11.0f);

        juce::Path bgArc, valArc;
        const float ar = r - 2.6f;
        bgArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, endA, true);
        g.setColour (Theme::border.brighter (0.25f));
        g.strokePath (bgArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        valArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, (bool) s.getProperties().getWithDefault ("uni", false) ? startA : mid, ang, true);
        g.setColour (accent);
        g.strokePath (valArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path tick;
        tick.addRoundedRectangle (-1.25f, -(r - 7.0f), 2.5f, r * 0.36f, 1.0f);
        g.setColour (Theme::text);
        g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

// Small flat icon button (transport etc.)
class IconButton : public juce::Button
{
public:
    enum Icon { Play, Stop, Record, Plus, Loop, SkipStart, SkipEnd };
    void setIconColour (juce::Colour c) { colour = c; repaint(); }
    IconButton (Icon i, juce::Colour c) : juce::Button ({}), icon (i), colour (c)
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        auto bgc = on ? colour : Theme::raised;
        if (on && icon == Record)   // gentle pulse while recording
            bgc = bgc.interpolatedWith (juce::Colours::white, 0.12f + 0.12f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f));
        if (down) bgc = bgc.darker (0.2f); else if (over) bgc = bgc.brighter (0.12f);
        Theme::framedTile (g, getLocalBounds().toFloat(), bgc, 3.0f);

        g.setColour (on ? Theme::onAccent : colour);
        auto ic = b.withSizeKeepingCentre (b.getHeight() * 0.38f, b.getHeight() * 0.38f);
        switch (icon)
        {
            case Play:
            {
                juce::Path p;
                p.addTriangle (ic.getX() + 2, ic.getY(), ic.getX() + 2, ic.getBottom(), ic.getRight() + 2, ic.getCentreY());
                g.fillPath (p);
                break;
            }
            case Stop:   g.fillRoundedRectangle (ic, 2.0f); break;
            case Record: g.fillEllipse (ic); break;
            case SkipStart: case SkipEnd:
            {
                juce::Graphics::ScopedSaveState ss (g);
                if (icon == SkipEnd) g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, b.getCentreX(), b.getCentreY()));
                const auto r = b.withSizeKeepingCentre (b.getHeight() * 0.42f, b.getHeight() * 0.42f);
                g.fillRect (r.getX() - 1.0f, r.getY(), 2.5f, r.getHeight());
                juce::Path p;
                p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + 3.0f, r.getCentreY());
                g.fillPath (p);
                break;
            }
            case Loop:
            {
                const auto c = b.getCentre();
                const float hw = b.getHeight() * 0.27f, hh = b.getHeight() * 0.17f;
                const float x0 = c.x - hw, x1 = c.x + hw, y0 = c.y - hh, y1 = c.y + hh;
                juce::Path top, bottom;
                top.startNewSubPath (x0, y0 + 5.0f); top.lineTo (x0, y0); top.lineTo (x1 - 4.0f, y0);
                bottom.startNewSubPath (x1, y1 - 5.0f); bottom.lineTo (x1, y1); bottom.lineTo (x0 + 4.0f, y1);
                const juce::PathStrokeType st (2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
                g.strokePath (top.createPathWithRoundedCorners (3.0f), st);
                g.strokePath (bottom.createPathWithRoundedCorners (3.0f), st);
                juce::Path heads;
                heads.addTriangle (x1 - 4.0f, y0 - 4.0f, x1 - 4.0f, y0 + 4.0f, x1 + 2.5f, y0);
                heads.addTriangle (x0 + 4.0f, y1 - 4.0f, x0 + 4.0f, y1 + 4.0f, x0 - 2.5f, y1);
                g.fillPath (heads);
                break;
            }            case Plus:
                g.fillRoundedRectangle (ic.withSizeKeepingCentre (ic.getWidth(), 3.0f), 1.5f);
                g.fillRoundedRectangle (ic.withSizeKeepingCentre (3.0f, ic.getHeight()), 1.5f);
                break;
        }
    }

private:
    Icon icon;
    juce::Colour colour;
};

// Small flat icon button for edit operations (cut/copy/paste/fades/...). Icons are drawn as vector paths.
class ToolButton : public juce::Button
{
public:
    enum Icon { Cut, Copy, Paste, Silence, FadeIn, FadeOut, Duplicate, Undo, Redo, Menu, ZoomIn, ZoomOut, ZoomFit, SelectAll, Reverse, Crop, Folder };
    explicit ToolButton (Icon i) : juce::Button ({}), icon (i) { setWantsKeyboardFocus (false); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool en = isEnabled();
        Theme::framedTile (g, getLocalBounds().toFloat(), en ? (down ? Theme::raised.darker (0.2f) : over ? Theme::raised.brighter (0.15f) : Theme::raised)
                                                              : Theme::raised.withAlpha (0.5f), 2.5f);
        const auto col = en ? Theme::text : Theme::dim.withAlpha (0.45f);
        g.setColour (col);

        const auto r = b.withSizeKeepingCentre (16.0f, 16.0f);
        const float x = r.getX(), y = r.getY();
        const juce::PathStrokeType st (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Graphics::ScopedSaveState ss (g);
        if (icon == Redo) g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, x + 8.0f, y + 8.0f));
        juce::Path p;
        switch (icon)
        {
            case Cut:
                g.drawEllipse (x + 1.5f, y + 9.5f, 5.0f, 5.0f, 1.5f);
                g.drawEllipse (x + 9.5f, y + 9.5f, 5.0f, 5.0f, 1.5f);
                p.startNewSubPath (x + 4.8f, y + 9.6f);  p.lineTo (x + 11.8f, y + 1.0f);
                p.startNewSubPath (x + 11.2f, y + 9.6f); p.lineTo (x + 4.2f, y + 1.0f);
                g.strokePath (p, st);
                break;
            case Copy:
                g.drawRoundedRectangle (x + 5.0f, y + 1.0f, 9.5f, 10.0f, 2.0f, 1.5f);
                g.drawRoundedRectangle (x + 1.5f, y + 5.0f, 9.5f, 10.0f, 2.0f, 1.5f);
                break;
            case Paste:
                g.drawRoundedRectangle (x + 2.0f, y + 3.0f, 12.0f, 12.0f, 2.0f, 1.5f);
                g.fillRoundedRectangle (x + 5.0f, y + 1.0f, 6.0f, 4.0f, 1.2f);
                g.fillRect (x + 5.0f, y + 8.0f, 6.0f, 1.4f);
                g.fillRect (x + 5.0f, y + 11.0f, 4.0f, 1.4f);
                break;
            case Silence:
                p.startNewSubPath (x + 1.0f, y + 6.0f); p.lineTo (x + 4.5f, y + 6.0f); p.lineTo (x + 8.5f, y + 2.5f);
                p.lineTo (x + 8.5f, y + 13.5f); p.lineTo (x + 4.5f, y + 10.0f); p.lineTo (x + 1.0f, y + 10.0f); p.closeSubPath();
                g.fillPath (p);
                p.clear();
                p.startNewSubPath (x + 10.5f, y + 5.5f); p.lineTo (x + 15.0f, y + 10.5f);
                p.startNewSubPath (x + 15.0f, y + 5.5f); p.lineTo (x + 10.5f, y + 10.5f);
                g.strokePath (p, st);
                break;
            case FadeIn:
                p.addTriangle (x + 1.0f, y + 14.0f, x + 15.0f, y + 14.0f, x + 15.0f, y + 2.0f);
                g.setColour (col.withAlpha (0.35f)); g.fillPath (p);
                g.setColour (col);
                g.drawLine (x + 1.0f, y + 14.0f, x + 15.0f, y + 2.0f, 1.6f);
                break;
            case FadeOut:
                p.addTriangle (x + 1.0f, y + 14.0f, x + 15.0f, y + 14.0f, x + 1.0f, y + 2.0f);
                g.setColour (col.withAlpha (0.35f)); g.fillPath (p);
                g.setColour (col);
                g.drawLine (x + 1.0f, y + 2.0f, x + 15.0f, y + 14.0f, 1.6f);
                break;
            case Duplicate:
                g.drawRoundedRectangle (x + 1.5f, y + 2.0f, 13.0f, 5.0f, 1.5f, 1.5f);
                g.setColour (col.withAlpha (0.45f));
                g.fillRoundedRectangle (x + 1.5f, y + 9.0f, 13.0f, 5.0f, 1.5f);
                break;
            case SelectAll:
            {
                juce::Path rp, dashed;
                rp.addRoundedRectangle (x + 1.0f, y + 1.5f, 14.0f, 13.0f, 1.5f);
                const float dl[] = { 2.6f, 2.0f };
                juce::PathStrokeType (1.5f).createDashedStroke (dashed, rp, dl, 2);
                g.fillPath (dashed);
                g.setColour (col.withAlpha (0.35f));
                g.fillRect (x + 4.0f, y + 5.0f, 8.0f, 6.0f);
                break;
            }
            case Reverse:
                p.addTriangle (x + 8.0f, y + 3.0f, x + 8.0f, y + 13.0f, x + 1.0f, y + 8.0f);
                p.addTriangle (x + 15.0f, y + 3.0f, x + 15.0f, y + 13.0f, x + 8.0f, y + 8.0f);
                g.fillPath (p);
                break;
            case ZoomIn: case ZoomOut: case ZoomFit:
                g.drawEllipse (x + 1.0f, y + 1.0f, 10.0f, 10.0f, 1.6f);
                g.drawLine (x + 9.5f, y + 9.5f, x + 15.0f, y + 15.0f, 2.0f);
                if (icon != ZoomFit) g.fillRect (x + 3.5f, y + 5.2f, 5.0f, 1.6f);
                if (icon == ZoomIn) g.fillRect (x + 5.2f, y + 3.5f, 1.6f, 5.0f);
                if (icon == ZoomFit)
                {
                    g.drawLine (x + 3.0f, y + 6.0f, x + 9.0f, y + 6.0f, 1.4f);
                    g.drawLine (x + 3.0f, y + 4.0f, x + 3.0f, y + 8.0f, 1.4f);
                    g.drawLine (x + 9.0f, y + 4.0f, x + 9.0f, y + 8.0f, 1.4f);
                }
                break;
            case Crop:   // two crop marks
                g.drawLine (x + 4.5f, y + 1.0f, x + 4.5f, y + 11.5f, 1.8f);
                g.drawLine (x + 4.5f, y + 11.5f, x + 15.0f, y + 11.5f, 1.8f);
                g.drawLine (x + 1.0f, y + 4.5f, x + 11.5f, y + 4.5f, 1.8f);
                g.drawLine (x + 11.5f, y + 4.5f, x + 11.5f, y + 15.0f, 1.8f);
                break;
            case Folder:   // an open-folder outline
                p.startNewSubPath (x + 1.5f, y + 13.5f);
                p.lineTo (x + 1.5f, y + 3.0f);  p.lineTo (x + 6.2f, y + 3.0f);  p.lineTo (x + 8.0f, y + 5.2f);
                p.lineTo (x + 14.5f, y + 5.2f); p.lineTo (x + 14.5f, y + 13.5f); p.closeSubPath();
                g.strokePath (p, st);
                break;
            case Menu:
                for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (x + 1.0f, y + 2.5f + 5.0f * (float) i, 14.0f, 2.0f, 1.0f);
                break;
            case Undo: case Redo:
                p.startNewSubPath (x + 4.5f, y + 7.0f);
                p.cubicTo (x + 7.5f, y + 3.0f, x + 14.0f, y + 4.5f, x + 13.5f, y + 11.0f);
                g.strokePath (p, st);
                p.clear();
                p.addTriangle (x + 1.0f, y + 7.0f, x + 6.5f, y + 2.5f, x + 6.5f, y + 11.5f);
                g.fillPath (p);
                break;
        }
    }

private:
    Icon icon;
};

// Peak meter, dB scaled -60..+6 to match fader. Vertical or horizontal, 1 or 2 bars.
class LevelMeter : public juce::Component
{
public:
    void setHorizontal (bool h) { horizontal = h; }
    void setBars (int n) { bars = n; }

    void setLevels (float l, float r)
    {
        dispL = l > dispL ? l : dispL * 0.86f;
        dispR = r > dispR ? r : dispR * 0.86f;
        if (l > holdL || --holdTicksL < 0) { holdL = l; holdTicksL = 30; }
        if (r > holdR || --holdTicksR < 0) { holdR = r; holdTicksR = 30; }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float gap = 2.0f;
        if (bars == 1) { drawBar (g, b, dispL, holdL); return; }
        if (horizontal)
        {
            const float bh = (b.getHeight() - gap) * 0.5f;
            drawBar (g, b.removeFromTop (bh), dispL, holdL);
            b.removeFromTop (gap);
            drawBar (g, b, dispR, holdR);
        }
        else
        {
            const float bw = (b.getWidth() - gap) * 0.5f;
            drawBar (g, b.removeFromLeft (bw), dispL, holdL);
            b.removeFromLeft (gap);
            drawBar (g, b, dispR, holdR);
        }
    }

private:
    static float pos (float gain)
    {
        return gain <= 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain) + 60.0f) / 66.0f);
    }

    void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float level, float hold)
    {
        g.setColour (Theme::deep);
        g.fillRect (r);
        const float p = pos (level), hp = pos (hold);
        // solid bar in the theme colours: accent up to -18 dB, secondary up to -3 dB, red only for clipping
        const float zoneY = (-18.0f + 60.0f) / 66.0f, zoneR = (-3.0f + 60.0f) / 66.0f;
        auto zone = [&] (float from, float to, juce::Colour c)
        {
            const float a = juce::jmin (p, to);
            if (a <= from) return;
            g.setColour (c);
            if (horizontal) g.fillRect (r.getX() + r.getWidth() * from, r.getY(), r.getWidth() * (a - from), r.getHeight());
            else            g.fillRect (r.getX(), r.getBottom() - r.getHeight() * a, r.getWidth(), r.getHeight() * (a - from));
        };
        zone (0.0f, zoneY, Theme::accent);
        zone (zoneY, zoneR, Theme::purple);
        zone (zoneR, 1.0f, Theme::red);
        if (hp > 0.02f)
        {
            g.setColour (Theme::text.withAlpha (0.8f));
            if (horizontal) g.fillRect (r.getX() + r.getWidth() * hp - 1.5f, r.getY(), 1.5f, r.getHeight());
            else            g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hp, r.getWidth(), 1.5f);
        }
    }

    static void segments (juce::Graphics& g, juce::Rectangle<float> r, bool horiz)
    {
        g.setColour (Theme::panel);
        if (horiz) for (float x = r.getX() + 3.0f; x < r.getRight(); x += 4.0f) g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        else       for (float y = r.getY() + 3.0f; y < r.getBottom(); y += 4.0f) g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    }

    bool horizontal = false;
    int bars = 2;
    float dispL = 0, dispR = 0, holdL = 0, holdR = 0;
    int holdTicksL = 0, holdTicksR = 0;
};
// Large dashed "+" under the last track
class AddTrackButton : public juce::Button
{
public:
    AddTrackButton() : juce::Button ({}) { setWantsKeyboardFocus (false); setTooltip ("Add track (Ctrl+T)"); setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        const bool en = isEnabled();
        if (en && (over || down))
        {
            g.setColour (Theme::raised.withAlpha (down ? 0.9f : 0.55f));
            g.fillRoundedRectangle (b, Theme::radius);
        }
        const auto col = ! en ? Theme::dim.withAlpha (0.4f) : (over || down) ? Theme::text : Theme::dim.brighter (0.25f);
        g.setColour (col);
        const juce::String label ("Add new track");
        const juce::Font f (juce::FontOptions (13.0f));
        g.setFont (f);
        const float tw = juce::GlyphArrangement::getStringWidth (f, label);
        const float total = 12.0f + 10.0f + tw;
        const float x0 = b.getCentreX() - total * 0.5f, cy = b.getCentreY();
        g.fillRoundedRectangle (x0, cy - 0.75f, 12.0f, 1.5f, 0.75f);
        g.fillRoundedRectangle (x0 + 5.25f, cy - 6.0f, 1.5f, 12.0f, 0.75f);
        g.drawText (label, juce::Rectangle<float> (x0 + 22.0f, b.getY(), tw + 4.0f, b.getHeight()), juce::Justification::centredLeft);
    }
};