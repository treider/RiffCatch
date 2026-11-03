#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <map>
#include <tuple>

// Seven-segment display drawing, like an old countdown timer: lit segments in the display colour, unlit ones as a faint ghost.
namespace Seg
{
    // ---- the look of all seven-segment displays (counter in the cards, clock): adjust here ----
    inline juce::Colour digitColour  { 0xff9fd8ff };   // lit segments: light blue
    inline juce::Colour iceBlue      { 0xff0a3566 };   // background: deep ice blue
    inline juce::Colour shadowColour { 0xff000a1a };   // inner shadow along all edges
    inline float shadowAlpha = 0.75f;                  // how strong the inner shadow is
    inline float shadowBlur  = 6.0f;                   // how soft / deep it is, in pixels
    inline float shadowInset = 1.0f;                   // how far in from the edge it starts, in pixels
    inline float glowAlpha   = 0.16f;                  // "dodge": soft glow around lit segments
    inline float burnAlpha   = 0.28f;                  // "burn": unlit segments are burned darker than the background

    // A lit segment with a soft glow behind it, or an unlit one burned into the background.
    inline void litShape (juce::Graphics& g, juce::Rectangle<float> s, float cr, juce::Colour lit)
    {
        const float e = juce::jmax (1.5f, juce::jmin (s.getWidth(), s.getHeight()) * 0.9f);
        g.setColour (lit.withAlpha (glowAlpha * 0.5f));
        g.fillRoundedRectangle (s.expanded (e), cr + e);
        g.setColour (lit.withAlpha (glowAlpha));
        g.fillRoundedRectangle (s.expanded (e * 0.45f), cr + e * 0.45f);
        g.setColour (lit);
        g.fillRoundedRectangle (s, cr);
    }
    inline void burnShape (juce::Graphics& g, juce::Rectangle<float> s, float cr)
    {
        g.setColour (shadowColour.withAlpha (burnAlpha));
        g.fillRoundedRectangle (s, cr);
    }

    // Display background: flat ice blue with a soft inner shadow that follows the rounded shape of the buttons.
    // Rendered once per size and parameter set, then drawn as an image (the clock repaints about 30 times a second).
    inline void paintDisplayBackground (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        if (r.isEmpty()) return;
        static std::map<std::tuple<int, int, int, juce::uint32, juce::uint32, int>, juce::Image> cache;
        const int w = (int) std::ceil (r.getWidth()), h = (int) std::ceil (r.getHeight());
        const int params = (int) (shadowAlpha * 1000.0f) * 31 + (int) (shadowBlur * 100.0f) * 7 + (int) (shadowInset * 100.0f);
        const std::tuple<int, int, int, juce::uint32, juce::uint32, int> key { w, h, (int) std::lround (radius * 10.0f), iceBlue.getARGB(), shadowColour.getARGB(), params };
        auto it = cache.find (key);
        if (it == cache.end())
        {
            if (cache.size() > 24) cache.clear();
            juce::Image img (juce::Image::ARGB, w, h, true);
            juce::Graphics ig (img);
            juce::Path shape;
            shape.addRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, radius);
            ig.reduceClipRegion (shape);
            ig.setColour (iceBlue);
            ig.fillPath (shape);
            const float pad = shadowBlur * 3.0f + 4.0f;       // a frame: everything outside the shape, so its blur falls inwards
            juce::Path frame;
            frame.addRectangle (-pad, -pad, (float) w + 2.0f * pad, (float) h + 2.0f * pad);
            frame.addRoundedRectangle (shadowInset, shadowInset, (float) w - 2.0f * shadowInset, (float) h - 2.0f * shadowInset, juce::jmax (1.0f, radius - shadowInset));
            frame.setUsingNonZeroWinding (false);
            juce::DropShadow (shadowColour.withAlpha (shadowAlpha), juce::jmax (1, (int) std::lround (shadowBlur)), { 0, 0 }).drawForPath (ig, frame);
            it = cache.emplace (key, img).first;
        }
        g.drawImageAt (it->second, (int) std::floor (r.getX()), (int) std::floor (r.getY()));
    }

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
            if ((mask & (1 << i)) != 0) litShape (g, seg[i], t * 0.4f, lit);
            else                        burnShape (g, seg[i], t * 0.4f);
        }
    }

    inline float colonWidth (float h) { return h * 0.22f; }
    inline void colon (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.0f, h * 0.13f), cx = x + colonWidth (h) * 0.5f - d * 0.5f;
        litShape (g, { cx, y + h * 0.28f, d, d }, d * 0.3f, lit);
        litShape (g, { cx, y + h * 0.66f, d, d }, d * 0.3f, lit);
    }
    inline void dot (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.0f, h * 0.13f);
        litShape (g, { x + colonWidth (h) * 0.5f - d * 0.5f, y + h - d, d, d }, d * 0.3f, lit);
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