#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Seven-segment display drawing, like an old countdown timer: lit segments in the display colour, unlit ones as a faint ghost.
namespace Seg
{
    inline juce::Colour digitColour { 0xff9fd8ff };    // lit segments: light blue (counter and clock)

    // segments a (top), b (upper right), c (lower right), d (bottom), e (lower left), f (upper left), g (middle)
    constexpr int sa = 1, sb = 2, sc = 4, sd = 8, se = 16, sf = 32, sg = 64;
    constexpr int maskD = sb | sc | sd | se | sg, maskB = sc | sd | se | sf | sg;

    inline int digitMask (int n)
    {
        static const int m[10] = { sa | sb | sc | sd | se | sf, sb | sc, sa | sb | sd | se | sg, sa | sb | sc | sd | sg, sb | sc | sf | sg,
                                   sa | sc | sd | sf | sg, sa | sc | sd | se | sf | sg, sa | sb | sc, 127, sa | sb | sc | sd | sf | sg };
        return m[juce::jlimit (0, 9, n)];
    }

    inline float glyphWidth (float h) { return h * 0.52f; }

    inline void glyph (juce::Graphics& g, float x, float y, float h, int mask, juce::Colour lit)
    {
        const float w = glyphWidth (h);
        const float t = juce::jmax (1.8f, h * 0.13f), hh = h * 0.5f;
        const juce::Rectangle<float> seg[7] = {
            { x + t * 0.5f, y, w - t, t },                              // a
            { x + w - t, y + t * 0.5f, t, hh - t },                     // b
            { x + w - t, y + hh, t, hh - t * 0.5f },                    // c
            { x + t * 0.5f, y + h - t, w - t, t },                      // d
            { x, y + hh, t, hh - t * 0.5f },                            // e
            { x, y + t * 0.5f, t, hh - t },                             // f
            { x + t * 0.5f, y + hh - t * 0.5f, w - t, t } };            // g
        for (int i = 0; i < 7; ++i)
        {
            g.setColour ((mask & (1 << i)) != 0 ? lit : lit.withAlpha (0.09f));
            g.fillRoundedRectangle (seg[i], t * 0.4f);
        }
    }

    inline float colonWidth (float h) { return h * 0.22f; }
    inline void colon (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.0f, h * 0.13f), cx = x + colonWidth (h) * 0.5f - d * 0.5f;
        g.setColour (lit);
        g.fillRoundedRectangle (cx, y + h * 0.28f, d, d, d * 0.3f);
        g.fillRoundedRectangle (cx, y + h * 0.66f, d, d, d * 0.3f);
    }
    inline void dot (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.0f, h * 0.13f);
        g.setColour (lit);
        g.fillRoundedRectangle (x + colonWidth (h) * 0.5f - d * 0.5f, y + h - d, d, d, d * 0.3f);
    }
}

// A clock text ("00:03.512") drawn in seven segments, centred in the component and scaled to fit.
class SegmentClock : public juce::Component
{
public:
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setColour (juce::Colour) {}                     // the digits always use Seg::digitColour

    void paint (juce::Graphics& g) override
    {
        const float gap = 0.12f;
        auto widthFor = [&] (float h)
        {
            float w = 0.0f;
            for (auto c : text) w += (juce::CharacterFunctions::isDigit (c) ? Seg::glyphWidth (h) : Seg::colonWidth (h)) + h * gap;
            return w - h * gap;
        };
        float h = juce::jmin (getHeight() - 18.0f, 30.0f);       // some air above and below the digits
        while (h > 8.0f && widthFor (h) > getWidth() - 14.0f) h -= 1.0f;
        float x = ((float) getWidth() - widthFor (h)) * 0.5f;
        const float y = ((float) getHeight() - h) * 0.5f;
        for (auto c : text)
        {
            if (juce::CharacterFunctions::isDigit (c)) { Seg::glyph (g, x, y, h, Seg::digitMask (c - '0'), Seg::digitColour); x += Seg::glyphWidth (h); }
            else if (c == ':') { Seg::colon (g, x, y, h, Seg::digitColour); x += Seg::colonWidth (h); }
            else { Seg::dot (g, x, y, h, Seg::digitColour); x += Seg::colonWidth (h); }
            x += h * gap;
        }
    }

private:
    juce::String text = "00:00.000";
    juce::Colour colour { Theme::accent };
};