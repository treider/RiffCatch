#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Tokyo Night Storm palette
namespace Theme
{
    inline const juce::Colour bg     { 0xff1a1b26 };
    inline const juce::Colour panel  { 0xff1f2133 };
    inline const juce::Colour deep   { 0xff14151d };
    inline const juce::Colour raised { 0xff292e42 };
    inline const juce::Colour border { 0xff2f334d };
    inline const juce::Colour text   { 0xffc0caf5 };
    inline const juce::Colour dim    { 0xff565f89 };
    inline const juce::Colour blue   { 0xff7aa2f7 };
    inline const juce::Colour cyan   { 0xff7dcfff };
    inline const juce::Colour green  { 0xff9ece6a };
    inline const juce::Colour yellow { 0xffe0af68 };
    inline const juce::Colour orange { 0xffff9e64 };
    inline const juce::Colour red    { 0xfff7768e };
    inline const juce::Colour purple { 0xffbb9af7 };
    inline const juce::Colour teal   { 0xff73daca };

    inline juce::Colour trackColour (int i)
    {
        const juce::Colour c[] = { blue, cyan, green, yellow, orange, purple, teal, red };
        return c[i % 8];
    }
}

class RiffLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RiffLookAndFeel()
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
        setColour (juce::TextButton::textColourOnId, deep);
        setColour (juce::ComboBox::backgroundColourId, deep);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::ComboBox::focusedOutlineColourId, blue);
        setColour (juce::PopupMenu::backgroundColourId, panel);
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

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                               bool over, bool down) override
    {
        auto c = bgc;
        if (down) c = c.darker (0.15f); else if (over) c = c.brighter (0.12f);
        g.setColour (c);
        g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (0.5f), 6.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
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

            juce::Rectangle<float> thumb (cx - 12.0f, pos - 5.0f, 24.0f, 10.0f);
            g.setColour (Theme::text);
            g.fillRoundedRectangle (thumb, 1.5f);
            g.setColour (Theme::deep);
            g.fillRect (thumb.withSizeKeepingCentre (14.0f, 1.0f));
        }
        else
        {
            const float cy = y + h * 0.5f;
            juce::Rectangle<float> groove ((float) x, cy - 2.0f, (float) w, 4.0f);
            g.setColour (Theme::deep);
            g.fillRoundedRectangle (groove, 1.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (groove.withRight (pos), 1.0f);
            juce::Rectangle<float> thumb (pos - 5.0f, cy - 11.0f, 10.0f, 22.0f);
            g.setColour (Theme::text);
            g.fillRoundedRectangle (thumb, 1.5f);
            g.setColour (Theme::deep);
            g.fillRect (thumb.withSizeKeepingCentre (1.0f, 14.0f));
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

        g.setColour (Theme::deep);
        g.fillEllipse (c.x - r + 4, c.y - r + 4, 2 * r - 8, 2 * r - 8);

        juce::Path bgArc, valArc;
        const float ar = r - 2.0f;
        bgArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, endA, true);
        g.setColour (Theme::raised);
        g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        valArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, mid, ang, true);
        g.setColour (accent);
        g.strokePath (valArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path tick;
        tick.addRoundedRectangle (-1.25f, -(r - 8.0f), 2.5f, r * 0.42f, 1.0f);
        g.setColour (Theme::text);
        g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

// Small flat icon button (transport etc.)
class IconButton : public juce::Button
{
public:
    enum Icon { Play, Stop, Record, Plus };
    IconButton (Icon i, juce::Colour c) : juce::Button ({}), icon (i), colour (c)
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (5.0f, 4.0f);
        const bool on = getToggleState();
        if (on) juce::DropShadow (colour.withAlpha (0.55f), 9, { 0, 0 }).drawForRectangle (g, b.toNearestInt());
        auto bgc = on ? colour : Theme::raised;
        if (on && icon == Record)   // gentle pulse while recording
            bgc = bgc.interpolatedWith (juce::Colours::white, 0.12f + 0.12f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f));
        if (down) bgc = bgc.darker (0.2f); else if (over) bgc = bgc.brighter (0.12f);
        g.setColour (bgc);
        g.fillRoundedRectangle (b, 10.0f);

        g.setColour (on ? Theme::deep : colour);
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
            case Plus:
                g.fillRoundedRectangle (ic.withSizeKeepingCentre (ic.getWidth(), 3.0f), 1.5f);
                g.fillRoundedRectangle (ic.withSizeKeepingCentre (3.0f, ic.getHeight()), 1.5f);
                break;
        }
    }

private:
    Icon icon;
    juce::Colour colour;
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
        // flat colour zones: green up to -18 dB, yellow up to -6 dB, red above
        const float zoneY = (-18.0f + 60.0f) / 66.0f, zoneR = (-6.0f + 60.0f) / 66.0f;
        auto zone = [&] (float from, float to, juce::Colour c)
        {
            const float a = juce::jmin (p, to);
            if (a <= from) return;
            g.setColour (c);
            if (horizontal) g.fillRect (r.getX() + r.getWidth() * from, r.getY(), r.getWidth() * (a - from), r.getHeight());
            else            g.fillRect (r.getX(), r.getBottom() - r.getHeight() * a, r.getWidth(), r.getHeight() * (a - from));
        };
        zone (0.0f, zoneY, Theme::green);
        zone (zoneY, zoneR, Theme::yellow);
        zone (zoneR, 1.0f, Theme::red);
        if (hp > 0.02f)
        {
            g.setColour (Theme::text.withAlpha (0.8f));
            if (horizontal) g.fillRect (r.getX() + r.getWidth() * hp - 1.5f, r.getY(), 1.5f, r.getHeight());
            else            g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hp, r.getWidth(), 1.5f);
        }        segments (g, r, horizontal);
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