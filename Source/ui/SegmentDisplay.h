#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <map>
#include <tuple>

// Seven-segment display drawing, like an old countdown timer: lit segments in the display colour, unlit ones as a faint ghost.
namespace Seg
{
    // ---- the look of all seven-segment displays (counter in the cards, clock): adjust here ----
    inline juce::Colour digitColour   { 0xff8fd0ff };   // lit segments: ice blue / electric cyan (the base of the shading)
    inline juce::Colour bezelTop      { 0xff0a1c3e };   // the recessed bezel: dark blue, top ...
    inline juce::Colour bezelMid      { 0xff06122c };   // ... middle ...
    inline juce::Colour bezelBottom   { 0xff020814 };   // ... and bottom
    inline juce::Colour rimLight      { 0xff3f8cff };   // blue reflected light on the lower lip of the recess
    inline juce::Colour wellTop       { 0xff010610 };   // the recessed display: midnight blue, almost black
    inline juce::Colour wellBottom    { 0xff04163a };
    inline juce::Colour shadowColour  { 0xff000410 };   // inner shadow along all edges of the well
    inline float shadowAlpha = 0.95f;                   // how strong the inner shadow is
    inline float shadowBlur  = 6.0f;                    // how soft / deep it is, in pixels
    inline float shadowInset = 1.0f;                    // how far in from the edge it starts, in pixels
    inline float glowAlpha   = 0.11f;                   // "dodge": soft blue glow around lit segments
    inline float burnAlpha   = 0.30f;                   // "burn": unlit segments, deep blue burned into the well
    inline float slant       = 0.10f;                   // italic lean of the digits
    inline float ringWidth   = 4.5f;                    // thickness of the casing around the well

    // The well: midnight blue with a strong inner shadow that follows the rounded shape. Rendered once per size and
    // parameter set, then drawn as an image (the clock repaints about 30 times a second).
    inline void paintDisplayBackground (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        if (r.isEmpty()) return;
        static std::map<std::tuple<int, int, int, juce::uint32, juce::uint32, int>, juce::Image> cache;
        const int w = (int) std::ceil (r.getWidth()), h = (int) std::ceil (r.getHeight());
        const int params = (int) (shadowAlpha * 1000.0f) * 31 + (int) (shadowBlur * 100.0f) * 7 + (int) (shadowInset * 100.0f) + (int) wellBottom.getARGB();
        const std::tuple<int, int, int, juce::uint32, juce::uint32, int> key { w, h, (int) std::lround (radius * 10.0f), wellTop.getARGB(), shadowColour.getARGB(), params };
        auto it = cache.find (key);
        if (it == cache.end())
        {
            if (cache.size() > 24) cache.clear();
            juce::Image img (juce::Image::ARGB, w, h, true);
            juce::Graphics ig (img);
            juce::Path shape;
            shape.addRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, radius);
            ig.reduceClipRegion (shape);
            ig.setGradientFill (juce::ColourGradient (wellTop, 0.0f, 0.0f, wellBottom, 0.0f, (float) h, false));
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

    // The whole display housing inside `outer`: a matte dark-blue bezel sunk into
    // the panel, with a shadowed upper lip and blue reflected light on the lower
    // lip around the recessed well. Returns the well, where the digits go.
    inline juce::Rectangle<float> paintDisplayHousing (juce::Graphics& g, juce::Rectangle<float> outer, float radius)
    {
        if (outer.isEmpty()) return outer;
        g.setColour (Theme::frame);                                           // thin black line against the card
        g.fillRoundedRectangle (outer, radius);
        const auto c = outer.reduced (1.0f);
        const float cr = radius - 1.0f;
        {
            juce::ColourGradient bezel (bezelTop, 0.0f, c.getY(), bezelBottom, 0.0f, c.getBottom(), false);
            bezel.addColour (0.55f, bezelMid);
            g.setGradientFill (bezel);
            g.fillRoundedRectangle (c, cr);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, c.getY(),
                                                    juce::Colours::transparentBlack, 0.0f, c.getY() + c.getHeight() * 0.55f, false));
            g.fillRoundedRectangle (c, cr);                                   // upper lip in shadow: light comes from above
        }
        {
            juce::ColourGradient edge (juce::Colours::black.withAlpha (0.70f), 0.0f, c.getY(),
                                       rimLight.withAlpha (0.55f), 0.0f, c.getBottom(), false);
            edge.addColour (0.62f, juce::Colours::black.withAlpha (0.10f));
            g.setGradientFill (edge);                                         // dark upper contour, lit lower lip
            g.drawRoundedRectangle (c.reduced (0.5f), cr - 0.5f, 1.0f);
        }
        const auto well = outer.reduced (ringWidth);
        const float wr = juce::jmax (2.5f, radius - ringWidth * 0.7f);
        {
            juce::ColourGradient lip (juce::Colours::black.withAlpha (0.85f), 0.0f, well.getY(),
                                      rimLight.withAlpha (0.42f), 0.0f, well.getBottom(), false);
            lip.addColour (0.70f, juce::Colours::black.withAlpha (0.30f));
            g.setGradientFill (lip);                                          // the bezel rolls into the hole
            g.drawRoundedRectangle (well.expanded (0.9f), wr + 0.9f, 1.6f);
        }
        paintDisplayBackground (g, well, wr);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (well, wr);
            g.reduceClipRegion (clip);
            g.setGradientFill (juce::ColourGradient (rimLight.withAlpha (0.0f), 0.0f, well.getBottom() - 5.0f,
                                                    rimLight.withAlpha (0.14f), 0.0f, well.getBottom(), false));
            g.fillRect (well);                                                // blue light caught inside the lower lip
        }
        return well;
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

    // One lit segment: soft blue glow around it (dodge), a bevelled body shaded from near-white at the top to deep blue at
    // the bottom of the digit, and a thin edge that is bright on top and dark underneath.
    inline void glowPath (juce::Graphics& g, const juce::Path& p, float t, juce::Colour lit)
    {
        const float a = lit.getFloatAlpha(), e = juce::jmax (1.5f, t * 0.8f);
        g.setColour (lit.withAlpha (glowAlpha * 0.5f * a));
        g.strokePath (p, juce::PathStrokeType (e * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lit.withAlpha (glowAlpha * a));
        g.strokePath (p, juce::PathStrokeType (e * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    inline void litPath (juce::Graphics& g, const juce::Path& p, float t, juce::Colour lit, float y0, float y1)
    {
        const float a = lit.getFloatAlpha();
        const auto top = lit.interpolatedWith (juce::Colours::white, 0.8f).withAlpha (a);
        const auto bot = lit.interpolatedWith (juce::Colour (0xff1a56d0), 0.45f).withAlpha (a);
        g.setGradientFill (juce::ColourGradient (top, 0.0f, y0, bot, 0.0f, y1, false));
        g.fillPath (p);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f * a), 0.0f, y0, juce::Colour (0xff1448b8).withAlpha (0.45f * a), 0.0f, y1, false));
        g.strokePath (p, juce::PathStrokeType (0.6f));
    }
    inline void burnPath (juce::Graphics& g, const juce::Path& p)
    {
        g.setColour (juce::Colour (0xff0b2a5c).withAlpha (burnAlpha));
        g.fillPath (p);
    }

    inline float glyphWidth (float h) { return h * 0.52f; }
    inline juce::AffineTransform leanFor (float y, float h) { return juce::AffineTransform::shear (-slant, 0.0f).translated (slant * (y + h), 0.0f); }

    inline void glyph (juce::Graphics& g, float x, float y, float h, int mask, juce::Colour lit)
    {
        const float w = glyphWidth (h);
        const float t = juce::jmax (2.2f, h * 0.15f), hh = h * 0.5f, k = 0.6f;     // k: hairline gap between segments
        auto horiz = [&] (float sx, float sy, float sw)                              // hexagon with pointed ends
        {
            juce::Path p;
            p.startNewSubPath (sx + k, sy + t * 0.5f);
            p.lineTo (sx + t * 0.5f + k, sy);                p.lineTo (sx + sw - t * 0.5f - k, sy);
            p.lineTo (sx + sw - k, sy + t * 0.5f);
            p.lineTo (sx + sw - t * 0.5f - k, sy + t);       p.lineTo (sx + t * 0.5f + k, sy + t);
            p.closeSubPath();
            return p;
        };
        auto vert = [&] (float sx, float sy, float sh)
        {
            juce::Path p;
            p.startNewSubPath (sx + t * 0.5f, sy + k);
            p.lineTo (sx + t, sy + t * 0.5f + k);            p.lineTo (sx + t, sy + sh - t * 0.5f - k);
            p.lineTo (sx + t * 0.5f, sy + sh - k);
            p.lineTo (sx, sy + sh - t * 0.5f - k);           p.lineTo (sx, sy + t * 0.5f + k);
            p.closeSubPath();
            return p;
        };
        juce::Path seg[7] = {
            horiz (x + t * 0.5f, y, w - t),                         // a
            vert (x + w - t, y + t * 0.5f, hh),                     // b
            vert (x + w - t, y + hh - t * 0.5f, hh),                // c
            horiz (x + t * 0.5f, y + h - t, w - t),                 // d
            vert (x, y + hh - t * 0.5f, hh),                        // e
            vert (x, y + t * 0.5f, hh),                             // f
            horiz (x + t * 0.5f, y + hh - t * 0.5f, w - t) };       // g
        const auto lean = leanFor (y, h);
        for (int i = 0; i < 7; ++i)                                  // unlit ones first so glows of lit ones lie on top
            if ((mask & (1 << i)) == 0) { seg[i].applyTransform (lean); burnPath (g, seg[i]); }
        for (int i = 0; i < 7; ++i)
            if ((mask & (1 << i)) != 0) { seg[i].applyTransform (lean); glowPath (g, seg[i], t, lit); }
        for (int i = 0; i < 7; ++i)                                  // bodies last, so no glow dims a neighbouring segment
            if ((mask & (1 << i)) != 0) litPath (g, seg[i], t, lit, y, y + h);
    }

    inline float colonWidth (float h) { return h * 0.22f; }
    inline void litDot (juce::Graphics& g, float cx, float cy, float d, float y, float h, juce::Colour lit)
    {
        juce::Path p;
        p.addEllipse (cx + slant * (y + h - cy) - d * 0.5f, cy - d * 0.5f, d, d);
        glowPath (g, p, d * 0.8f, lit);
        litPath (g, p, d * 0.8f, lit, cy - d * 0.5f, cy + d * 0.5f);
    }
    inline void colon (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.6f, h * 0.15f), cx = x + colonWidth (h) * 0.5f;
        litDot (g, cx, y + h * 0.30f + d * 0.5f, d, y, h, lit);
        litDot (g, cx, y + h * 0.68f + d * 0.5f, d, y, h, lit);
    }
    inline void dot (juce::Graphics& g, float x, float y, float h, juce::Colour lit)
    {
        const float d = juce::jmax (2.6f, h * 0.15f);
        litDot (g, x + colonWidth (h) * 0.5f, y + h - d * 0.5f, d, y, h, lit);
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
        float h = juce::jmin (getHeight() - 14.0f, 30.0f);       // some air above and below the digits
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