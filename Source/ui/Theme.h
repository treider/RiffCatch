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
    // VU meters keep the Tokyo Night lilac/blue pair in every theme.
    inline juce::Colour meterFill() { return juce::Colour (0xffbb9af7); }
    inline juce::Colour meterPeak() { return juce::Colour (0xff7aa2f7); }
    inline bool legacyTracks = true;               // Tokyo Night keeps its original eight clip colours

    inline constexpr float radius = 4.0f;     // controls: buttons, boxes, menus, lanes
    inline constexpr float radiusLg = 6.0f;   // panels and track cards
    // Soft drop shadow under cards - tweak these three to taste
    inline constexpr float shadowOpacity = 0.22f;   // 0 = off, ~0.4 = strong
    inline constexpr int   shadowRadius  = 8;       // blur size in px
    inline constexpr int   shadowOffsetY = 2;       // how far the shadow falls downwards

    struct Preset { juce::String name; juce::uint32 primary, secondary, accent, background, surface, text; };
    inline std::vector<Preset> presets =   // the eight built-ins first, then user themes loaded from disk
    {
        { "Tokyo Night", 0xff7aa2f7, 0xffbb9af7, 0xff7dcfff, 0xff1a1b26, 0xff1f2133, 0xffc0caf5 },
        { "Forest",      0xff059669, 0xff10b981, 0xff34d399, 0xff022c22, 0xff064e3b, 0xffd1fae5 },
        { "Sunset",      0xffea580c, 0xfff59e0b, 0xfffbbf24, 0xff1c1917, 0xff292524, 0xfffef3c7 },
        { "Rose",        0xffbe123c, 0xffe11d48, 0xfffb7185, 0xff1c1917, 0xff3f1f1f, 0xffffe4e6 },
        { "Lavender",    0xff8b5cf6, 0xffa78bfa, 0xffc4b5fd, 0xff1e1b4b, 0xff312e81, 0xffe9d5ff },
        { "Coral",       0xfff43f5e, 0xfffb7185, 0xfffda4af, 0xff451a1a, 0xff7f1d1d, 0xffffe4e6 },
        { "Mint",        0xff14b8a6, 0xff2dd4bf, 0xff5eead4, 0xff134e4a, 0xff0f766e, 0xffccfbf1 },
        { "Neo-Retro",   0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec },
    };
    inline constexpr int builtinCount = 8;
    inline int numPresets() { return (int) presets.size(); }
    inline int current = 7;   // Neo-Retro

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

    // Track cards only: a localized rim in the live track colour, and a raised
    // nameplate. Armed cards pass Theme::red, so the red state is preserved.
    inline juce::Colour trackRimLight (juce::Colour family) { return family.interpolatedWith (juce::Colours::white, 0.22f); }
    inline juce::Colour trackRimShade() { return deep.darker (0.55f); }
    inline juce::Colour trackPlateHigh (juce::Colour family) { return family.interpolatedWith (juce::Colours::white, 0.18f); }
    inline juce::Colour trackPlateLow (juce::Colour family) { return family.withMultipliedBrightness (0.78f); }
    inline juce::Colour trackPlateContact() { return deep.darker (0.62f); }
    inline juce::Colour trackFaceHigh() { return panel.brighter (0.035f); }
    inline juce::Colour trackFaceLow() { return panel.darker (0.025f); }
    inline juce::Colour trackHeaderHigh() { return panel.brighter (0.080f); }
    inline juce::Colour trackHeaderLow() { return panel.brighter (0.020f); }

    // Bass sound-knob material: live roles also work in custom light/dark previews.
    // Lighting uses surface brightness, not text (which is dark in light themes).
    inline juce::Colour bassKnobLight()   { return raised.brighter (0.22f); }
    inline juce::Colour bassKnobShade()   { return raised.darker (0.32f); }
    inline juce::Colour bassKnobContact() { return deep.darker (0.50f); }
    inline juce::Colour bassKnobGlint()   { return raised.brighter (0.65f); }
    inline juce::Colour bassKnobLabel()   { return dim.interpolatedWith (text, 0.22f); }

    // Exposed metal is a light source, not merely a brighter dark face.
    inline juce::Colour bassControlLight() { return juce::Colour::fromHSV (raised.getHue(), 0.06f, 0.90f, 1.0f); }
    inline juce::Colour bassControlContact() { return deep.darker (0.50f); }
    inline juce::Colour bassNoteLabel() { return dim.interpolatedWith (text, 0.32f); }
    inline juce::Colour bassShadow() { return deep.darker (0.85f); }
    inline juce::Colour bassGlyphContour() { return juce::Colours::black; }
    inline juce::Colour bassArcCore (juce::Colour family) { return family.interpolatedWith (bassControlLight(), 0.28f); }

    // Tuner/master only. Accent/cyan may be gold: cool reflected light has its
    // own hue, with live surface/text influence rather than a cached preset colour.
    inline juce::Colour mixerCoolLight() { return juce::Colour (0xffa9c9ed).interpolatedWith (text, 0.15f); }
    inline juce::Colour mixerContact() { return deep.darker (0.65f); }
    inline juce::Colour masterFaceLight() { return text.interpolatedWith (juce::Colours::white, 0.28f); }
    inline juce::Colour masterFaceShade() { return text.interpolatedWith (raised, 0.32f); }

    // Tuner only: a small real outside shadow within the existing 4px enclosure gap.
    // Four coverage layers avoid a blur/cache allocation in the repeated paint path.
    inline void tunerShadow (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (int layer = 3; layer >= 0; --layer)
        {
            const float spread = layer * 0.75f;
            g.setColour (juce::Colours::black.withAlpha (0.055f));
            g.fillRoundedRectangle (r.translated (0.0f, 1.0f).expanded (spread), radiusLg + spread);
        }
    }

    // Only the existing tuner/master well surrounds use this local material.
    // The caller paints the unchanged deep interior afterwards.
    inline void mixerWellSurround (juce::Graphics& g, juce::Rectangle<float> outer, float outerRadius,
                                   juce::Rectangle<float> well, float innerRadius,
                                   juce::Colour light = juce::Colour())
    {
        g.setColour (border.darker (0.28f));
        g.fillRoundedRectangle (outer, outerRadius);
        if (light.isTransparent()) light = mixerCoolLight();
        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.65f), outer.getX(), outer.getY(),
                                                light.withAlpha (0.0f), outer.getX() + outer.getWidth() * 0.65f,
                                                outer.getBottom(), false));
        g.drawRoundedRectangle (outer.reduced (0.65f), outerRadius - 0.65f, 1.1f);
        g.setColour (mixerContact());
        g.fillRoundedRectangle (well.expanded (1.0f), innerRadius + 0.5f);
    }

    // Cheap bounded coverage layers, not a blurred image allocated on each repaint.
    // Call from the parent, before children, so unchanged hit bounds do not clip it.
    inline void bassControlShadow (juce::Graphics& g, juce::Rectangle<float> r, bool enabled, bool down)
    {
        const float strength = ! enabled ? 0.35f : down ? 0.50f : 1.0f;
        for (int layer = 3; layer >= 0; --layer)
        {
            const float spread = layer * 0.75f;
            g.setColour (bassShadow().withAlpha (0.055f * strength));
            g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 1.5f).expanded (spread),
                                    juce::jmin (7.0f, r.getHeight() * 0.3f) + spread);
        }
    }

    // Child-only JUCE invalidation otherwise leaves the outside shadow stale.
    // Schedule a bounded parent repaint only on a state transition, not a timer.
    inline void bassShadowState (juce::Component& c, bool down)
    {
        const int state = (c.isEnabled() ? 1 : 0) | (down ? 2 : 0);
        auto& props = c.getProperties();
        if ((int) props.getWithDefault ("bassShadowState", -1) == state) return;
        props.set ("bassShadowState", state);
        if (auto* parent = c.getParentComponent())
            parent->repaint (parent->getLocalArea (&c, c.getLocalBounds()).expanded (4));
    }

    // Explicit Bass opt-in only. Keep the dark frame and semantic active fill;
    // directional light is clipped to the face, never an accent-coloured outline.
    inline void bassControlTile (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill,
                                 float ring, bool enabled, bool over, bool down, bool focus)
    {
        juce::Graphics::ScopedSaveState ss (g);
        if (! enabled) fill = fill.interpolatedWith (deep, 0.55f);
        else if (down) fill = fill.darker (0.20f);
        else if (over) fill = fill.brighter (0.12f);
        framedTile (g, r, fill, ring);
        const auto face = r.reduced (ring);
        if (face.isEmpty()) return;
        juce::Path clip;
        const float ro = juce::jmax (2.0f, juce::jmin (7.0f, r.getHeight() * 0.3f) - ring * 0.6f);
        clip.addRoundedRectangle (face, ro);
        g.reduceClipRegion (clip);
        const float strength = ! enabled ? 0.25f : down ? 0.50f : over ? 1.15f : 1.0f;
        const auto light = bassControlLight();
        // The small library arrows get edge light only, not a face wash.
        if (r.getHeight() >= 18.0f)
        {
            g.setGradientFill (juce::ColourGradient (light.withAlpha (0.11f * strength), face.getX(), face.getY(),
                                                    light.withAlpha (0.0f), face.getX(), face.getY() + face.getHeight() * 0.30f, false));
            g.fillPath (clip);
        }
        juce::Path bevel;
        bevel.addRoundedRectangle (face.reduced (0.65f), juce::jmax (1.0f, ro - 0.65f));
        g.setGradientFill (juce::ColourGradient (bassControlContact().withAlpha (0.0f), face.getX(), face.getCentreY(),
                                                bassControlContact().withAlpha (0.85f), face.getX(), face.getBottom(), false));
        g.strokePath (bevel, juce::PathStrokeType (1.3f));
        // A closed contour has no bright end-cap. Only the light field fades,
        // along the top edge and down the left side of the same rounded face.
        {
            juce::Graphics::ScopedSaveState highlightState (g);
            juce::Path contour, highlight;
            contour.addRoundedRectangle (face.reduced (0.5f), juce::jmax (1.0f, ro - 0.5f));
            juce::PathStrokeType (1.0f).createStrokedPath (highlight, contour);
            const auto origin = juce::Point<float> (face.getX() + ro, face.getY() + 0.5f);
            const float reach = juce::jmax (1.0f, face.getWidth() - ro);
            const auto stretch = juce::AffineTransform::scale (1.0f, face.getHeight() * 0.65f / reach, origin.x, origin.y);
            highlight.applyTransform (stretch.inverted());
            g.addTransform (stretch);
            g.setGradientFill (juce::ColourGradient (light.withAlpha ((focus && enabled ? 0.85f : 0.75f) * strength),
                                                    origin.x, origin.y, light.withAlpha (0.0f), origin.x + reach, origin.y, true));
            g.fillPath (highlight);
        }
        juce::ColourGradient reflection (light.withAlpha (0.0f), face.getX() + ro, face.getBottom(),
                                         light.withAlpha (0.0f), face.getCentreX(), face.getBottom(), false);
        reflection.addColour (0.20, light.withAlpha (0.20f * strength));
        g.setGradientFill (reflection);
        g.fillRect (face.getX() + ro, face.getBottom() - 2.0f, juce::jmax (0.0f, face.getWidth() * 0.5f - ro), 0.7f);
    }

    // Panel identities follow the live theme roles: Drums = Primary, Bass =
    // Secondary. Both the tab and the panel/ribbon use the same colour.
    inline juce::Colour drumAccent() { return blue; }
    inline juce::Colour bassAccent() { return purple; }

    // P1 Bass material only: derive every layer from the live theme, not a fixed orange palette.
    inline juce::Colour bassRim() { return bassAccent().interpolatedWith (bassControlLight(), 0.20f); }
    inline juce::Colour bassFocusLight() { return bassAccent().interpolatedWith (bassControlLight(), 0.35f); }
    inline juce::Colour bassFocusCore() { return bassFocusLight().interpolatedWith (bassControlLight(), 0.30f).withBrightness (1.0f); }
    inline juce::Colour bassLeftRibbon() { return bassAccent(); }
    inline juce::Colour bassTabRibbon() { return bassAccent(); }

    // v0.80 two-source lighting proof only. No palette or other knob changes.
    inline juce::Colour bassSubSpecular() { return bassKnobGlint().interpolatedWith (text, 0.35f); }
    inline juce::Colour bassSubCore() { return bassArcCore (cyan).interpolatedWith (text, 0.20f); }
    struct BassSubGeometry
    {
        juce::Point<float> centre;
        float radius;
        juce::Path valueArc;
        static BassSubGeometry make (juce::Rectangle<float> slider, float proportion,
                                     float start, float end, bool unipolar)
        {
            const auto b = slider.reduced (4.0f);
            BassSubGeometry result { b.getCentre(), juce::jmin (b.getWidth(), b.getHeight()) * 0.5f, {} };
            const float from = unipolar ? start : 0.5f * (start + end);
            const float to = start + proportion * (end - start);
            if (from != to)
                result.valueArc.addCentredArc (result.centre.x, result.centre.y, result.radius - 2.6f,
                                              result.radius - 2.6f, 0.0f, from, to, true);
            return result;
        }
    };

    // Local material contours, not layout radii. A 12px centred stroke on a
    // radius-6 path collapses its inner radius to zero. Keep a real curved hole
    // even at the deepest halo inset; neither the outer edge nor bounds move.
    inline juce::Path bassEnclosureBand (juce::Rectangle<float> edge, float depth, bool tab)
    {
        juce::Path band;
        band.setUsingNonZeroWinding (false);
        band.addRoundedRectangle (edge.getX(), edge.getY(), edge.getWidth(), edge.getHeight(),
                                  radiusLg, radiusLg, true, true, ! tab, ! tab);
        const auto inner = edge.reduced (depth);
        const float innerRadius = juce::jmax (3.0f, radiusLg - depth);
        band.addRoundedRectangle (inner.getX(), inner.getY(), inner.getWidth(), inner.getHeight(),
                                  innerRadius, innerRadius, true, true, ! tab, ! tab);
        return band;
    }
    // The straight 4px ribbon stays opaque; only its inner endpoint corners
    // turn through radius 2. The enclosure supplies the outer radius-6 clip.
    inline juce::Path bassStripePath (juce::Rectangle<float> edge)
    {
        juce::Path p;
        p.addRoundedRectangle (edge.getX(), edge.getY(), 4.0f, edge.getHeight(),
                               2.0f, 2.0f, false, true, false, true);
        return p;
    }
    inline void bassEnclosure (juce::Graphics& g, juce::Rectangle<float> r, bool tab = false, bool lightPoc = false)
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path shape;
        // Extend the active tab below its clip so there is no closing bottom border.
        const auto edge = r.reduced (1.0f).withTrimmedBottom (tab ? -6.0f : 0.0f);
        shape.addRoundedRectangle (edge.getX(), edge.getY(), edge.getWidth(), edge.getHeight(),
                                   radiusLg, radiusLg, true, true, ! tab, ! tab);
        g.reduceClipRegion (shape);
        const auto rim = bassRim();
        const auto dark = deep.darker (0.45f);
        juce::ColourGradient face (panel.interpolatedWith (raised, 0.18f), r.getX(), r.getY(),
                                   panel.interpolatedWith (deep, 0.65f), r.getRight(), r.getBottom(), false);
        face.addColour (0.45, panel.interpolatedWith (deep, 0.35f));
        g.setGradientFill (face);
        g.fillPath (shape);

        // Explicit rounded outer/inner edges for the 2.5px contact bevel.
        g.setColour (dark.withAlpha (0.85f));
        g.fillPath (bassEnclosureBand (edge, 2.5f, tab));
        juce::Path core;
        const auto inset = edge.reduced (0.75f);
        core.addRoundedRectangle (inset.getX(), inset.getY(), inset.getWidth(), inset.getHeight(),
                                  radiusLg - 0.75f, radiusLg - 0.75f, true, true, ! tab, ! tab);
        // Keep the live theme contour, but let far edges recede.
        g.setColour (rim.withAlpha (0.30f));
        g.strokePath (core, juce::PathStrokeType (1.0f));

        const float reach = tab ? r.getWidth() * 0.70f : 180.0f;
        const float vertical = tab ? 18.0f : 70.0f;
        const auto origin = edge.getTopLeft().translated (6.0f, 2.0f);
        // Elliptical, bounded radial light. Transform the filled geometry inversely
        // so its physical width stays constant while only the gradient stretches.
        auto localLight = [&] (juce::Path band, juce::Colour colour)
        {
            juce::Graphics::ScopedSaveState local (g);
            if (tab)
            {
                // Full-width top ribbon; the rounded band carries its ends down
                // into the transparent field instead of ending in bright caps.
                juce::ColourGradient top (colour, edge.getX(), edge.getY(),
                                          colour.withAlpha (0.0f), edge.getX(), edge.getY() + 9.0f, false);
                top.addColour (0.15, colour);
                g.setGradientFill (top);
                g.fillPath (band);
                return;
            }
            const auto stretch = juce::AffineTransform::scale (1.0f, vertical / reach, origin.x, origin.y);
            band.applyTransform (stretch.inverted());
            g.addTransform (stretch);
            juce::ColourGradient glow (colour, origin.x, origin.y,
                                       colour.withAlpha (0.0f), origin.x + reach, origin.y, true);
            glow.addColour (0.20, colour);
            glow.addColour (0.55, colour.withMultipliedAlpha (0.55f));
            g.setGradientFill (glow);
            g.fillPath (band);
        };
        // Nested curved bands taper the last 3.5px of the halo in half-pixel
        // steps. Combined peak opacity stays ~0.32, with the same spatial light.
        if (tab)
        {
            // One continuous light field: nested band coverage multiplied by
            // a second fade used to extinguish the halo just below the core.
            juce::ColourGradient top (bassTabRibbon().withAlpha (0.58f), edge.getX(), edge.getY(),
                                      bassTabRibbon().withAlpha (0.0f), edge.getX(), edge.getY() + 10.0f, false);
            top.addColour (0.25, bassTabRibbon().withAlpha (0.42f));
            top.addColour (0.60, bassTabRibbon().withAlpha (0.12f));
            g.setGradientFill (top);
            g.fillPath (shape);
        }
        else if (! lightPoc)
            for (int layer = 0; layer < 8; ++layer)
                localLight (bassEnclosureBand (edge, 2.5f + layer * 0.5f, false),
                            bassFocusLight().withAlpha (0.047f));
        // Full 1.5px exposed core, immediately beside the dark contact stripe.
        juce::Path coreStroke;
        juce::PathStrokeType (1.5f).createStrokedPath (coreStroke, core);
        localLight (coreStroke, (tab ? bassTabRibbon() : bassFocusCore()).withAlpha (0.95f));
        if (! tab)
        {
            // Solid 4px fill with softened inner ends and rounded-card clipping.
            // Keep Bass's existing inset and live Accent role (Neo-Retro yellow).
            // Paint last over the local rim; no separate attenuated left halo.
            g.setColour (bassLeftRibbon());
            g.fillPath (bassStripePath (edge));
            juce::ColourGradient reflection (bassFocusLight().withAlpha (0.0f), edge.getX() + 12.0f, edge.getBottom(),
                                             bassFocusLight().withAlpha (0.0f), edge.getX() + r.getWidth() * 0.23f, edge.getBottom(), false);
            reflection.addColour (0.04, bassFocusLight().withAlpha (0.07f));
            reflection.addColour (0.08, bassFocusLight().withAlpha (0.21f));
            reflection.addColour (0.12, bassFocusLight().withAlpha (0.28f));
            g.setGradientFill (reflection);
            g.fillRect (edge.getX() + 12.0f, edge.getBottom() - 3.3f, r.getWidth() * 0.23f - 12.0f, 0.85f);
        }
    }

    // Bass only; exactly the existing 3px well extent, never a grid overlay.
    inline void bassWellRim (juce::Graphics& g, juce::Rectangle<float> well)
    {
        juce::Graphics::ScopedSaveState ss (g);
        wellRing (g, well, 3.0f);
        g.setColour (deep.darker (0.65f));
        g.fillRoundedRectangle (well.expanded (1.5f), radiusLg);
        const auto light = bassControlLight();
        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.78f), well.getX(), well.getY(),
                                                light.withAlpha (0.0f), well.getX(), well.getBottom(), false));
        g.drawRoundedRectangle (well.expanded (2.5f), radiusLg + 1.0f, 1.0f);
        g.setColour (bassControlContact());
        g.drawRoundedRectangle (well.expanded (1.0f), radius, 1.0f);
        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.28f), well.getX() + 8.0f, well.getBottom(),
                                                light.withAlpha (0.0f), well.getX() + well.getWidth() * 0.35f, well.getBottom(), false));
        g.fillRect (well.getX() + 8.0f, well.getBottom() + 1.7f, juce::jmax (0.0f, well.getWidth() * 0.35f - 8.0f), 0.8f);
    }

    // v0.77 local material roles. Accent/cyan is gold in Neo-Retro, not a
    // cool light source. Keep these live, independent of the frozen mixer roles.
    inline juce::Colour materialCoolLight() { return juce::Colour (0xffa9c9ed).interpolatedWith (text, 0.12f); }
    inline juce::Colour materialContact() { return deep.darker (0.65f); }
    inline bool materialControl (const juce::Component& c)
    {
        const auto& p = c.getProperties();
        return (bool) p.getWithDefault ("bassControl", false)
            || (bool) p.getWithDefault ("drumControl", false)
            || (bool) p.getWithDefault ("trackControl", false)
            || (bool) p.getWithDefault ("toolbarControl", false);
    }

    // Only explicit button/combo opt-ins, once in their actual parent before
    // children. LibraryCounter owns its arrow shadows (not its blue housing).
    inline void materialControlShadows (juce::Graphics& g, juce::Component& parent)
    {
        for (auto* c : parent.getChildren())
        {
            if (! c->isVisible() || ! materialControl (*c)) continue;
            auto* button = dynamic_cast<juce::Button*> (c);
            auto* combo = dynamic_cast<juce::ComboBox*> (c);
            if (button != nullptr || combo != nullptr)
                bassControlShadow (g, parent.getLocalArea (c, c->getLocalBounds()).toFloat(), c->isEnabled(),
                                   button != nullptr ? button->isDown() : combo->isPopupActive());
        }
    }

    // v0.85 toolbar: pale exposure light on the exposed upper edge, a dark
    // neighbour inside it and a darker settings row. Roles are live so every
    // built-in/custom palette keeps working.
    inline juce::Colour toolbarFaceHigh() { return panel.brighter (0.050f); }
    inline juce::Colour toolbarFaceLow() { return panel.darker (0.040f); }
    inline juce::Colour toolbarRowTop() { return panel.darker (0.220f); }
    inline juce::Colour toolbarRowBottom() { return panel.darker (0.300f); }
    inline juce::Colour toolbarRim() { return materialCoolLight().interpolatedWith (juce::Colours::white, 0.15f); }
    inline juce::Colour toolbarContact() { return deep.darker (0.620f); }

    // v0.86: the toolbar's localized exposed-top glints, reused by the panel
    // frames. The caller keeps its own rim colour and state strength.
    inline void panelTopGlints (juce::Graphics& g, juce::Rectangle<float> r, float cornerRadius,
                                juce::Colour rim, juce::Colour contact, float strength = 1.0f,
                                int maxGlints = 8)
    {
        if (r.isEmpty() || r.getWidth() < 24.0f || strength <= 0.0f) return;
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, cornerRadius);
        g.reduceClipRegion (clip);
        auto glint = juce::Rectangle<float> (r.getX() + 8.0f, r.getY() + 0.6f,
                                            r.getWidth() - 16.0f, 1.8f);
        // Glints are spaced by panel width, not a fixed count: a narrow card such
        // as the mixer gets 2-3, while the toolbar can use all 8. Wide sequencer
        // panels cap the count so they do not read as a dashed strip.
        const int count = juce::jlimit (1, juce::jmax (1, maxGlints),
                                        (int) std::lround (r.getWidth() / 140.0f));
        const float half = count == 1 ? 0.18f : 0.35f / (float) count;
        const float start = count == 1 ? 0.5f : half;
        const float step = count > 1 ? (1.0f - 2.0f * half) / (float) (count - 1) : 0.0f;
        juce::ColourGradient top (contact.withAlpha (0.30f * strength), glint.getX(), 0.0f,
                                  contact.withAlpha (0.25f * strength), glint.getRight(), 0.0f, false);
        for (int i = 0; i < count; ++i)
        {
            const float p = start + (float) i * step;
            if (p - half > 0.001f) top.addColour (p - half, contact.withAlpha (0.22f * strength));
            top.addColour (p, rim.withAlpha (0.95f * strength));
            if (p + half < 0.999f) top.addColour (p + half, contact.withAlpha (0.22f * strength));
        }
        g.setGradientFill (top);
        g.fillRect (glint);
        g.setGradientFill (juce::ColourGradient (rim.withAlpha (0.14f * strength), 0.0f, r.getY() + 1.5f,
                                                rim.withAlpha (0.0f), 0.0f, r.getY() + 7.5f, false));
        g.fillRect (r.getX() + 4.0f, r.getY() + 1.5f, r.getWidth() - 8.0f, 6.0f);
    }

    // v0.97: pattern-rail / song-block material. A plain browser-style pill:
    // one solid face, no rim, gradient or glint. The pattern colour lives in a
    // small chip (like the reference logo square); the close glyph is an x in
    // a filled circle and the "+" tile is flat with no outline.
    inline void flatTabFace (juce::Graphics& g, juce::Rectangle<float> r, bool active, bool hovered)
    {
        if (r.isEmpty()) return;
        auto fill = active ? raised.brighter (0.10f) : raised.darker (0.16f);
        if (hovered) fill = fill.brighter (0.08f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, radiusLg);
    }
    inline void tabColourChip (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col)
    {
        g.setColour (col);
        g.fillRoundedRectangle (r, 2.5f);
    }
    inline void circleClose (juce::Graphics& g, juce::Rectangle<float> r, bool hot)
    {
        const auto c = r.getCentre();
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        g.setColour (hot ? red : deep.withAlpha (0.38f));
        g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        g.setColour (hot ? deep : text.withAlpha (0.72f));
        const float in = d * 0.30f;
        g.drawLine (c.x - d * 0.5f + in, c.y - d * 0.5f + in,
                    c.x + d * 0.5f - in, c.y + d * 0.5f - in, 1.6f);
        g.drawLine (c.x + d * 0.5f - in, c.y - d * 0.5f + in,
                    c.x - d * 0.5f + in, c.y + d * 0.5f - in, 1.6f);
    }
    inline void flatPlusTile (juce::Graphics& g, juce::Rectangle<float> r, bool hot)
    {
        g.setColour (hot ? raised.brighter (0.18f) : raised.darker (0.06f));
        g.fillRoundedRectangle (r, radiusLg);
        const auto c = r.getCentre();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
        g.setColour (hot ? text : text.withAlpha (0.72f));
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.6f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.6f);
    }

    // Shared tab material for the Drums/Bass tabs: one flat face and dark
    // contact band for every tab, a crisp rim in the tab's colour (grey when
    // inactive) and one localized glint on the active tab only.
    inline void tabFace (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour rim,
                         bool active, bool hovered = false)
    {
        if (r.isEmpty()) return;
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path tab;
        tab.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() + 6.0f,
                                 radiusLg, radiusLg, true, true, false, false);
        g.reduceClipRegion (tab);
        g.setGradientFill (juce::ColourGradient (hovered ? panel.brighter (0.04f) : panel, r.getTopLeft(),
                                                 panel.interpolatedWith (deep, 0.45f),
                                                 r.getBottomRight(), false));
        g.fillPath (tab);

        // No closing bottom border: the tab runs into the rail/panel body.
        const auto edge = r.reduced (1.0f).withTrimmedBottom (-6.0f);
        g.setColour (deep.darker (0.45f).withAlpha (0.85f));
        g.fillPath (bassEnclosureBand (edge, 2.5f, true));
        juce::Path core;
        const auto inset = edge.reduced (0.75f);
        core.addRoundedRectangle (inset.getX(), inset.getY(), inset.getWidth(), inset.getHeight(),
                                  radiusLg - 0.75f, radiusLg - 0.75f, true, true, false, false);
        g.setColour (rim.withAlpha (active ? 0.95f : 0.80f));
        g.strokePath (core, juce::PathStrokeType (1.5f));

        if (active)
            panelTopGlints (g, r, radiusLg, rim.brighter (0.25f), deep, 0.85f, 1);
    }

    // v0.86: full exposed-edge treatment for the outer panel frames. Same
    // pale upper contour, dark inner neighbour and localized glints as the
    // v0.85 toolbar, in the panel's own live rim/contact roles. No geometry,
    // control or hit-area changes.
    inline void panelExposedEdge (juce::Graphics& g, juce::Rectangle<float> r, float cornerRadius,
                                  juce::Colour rim, juce::Colour contact, float strength = 1.0f,
                                  int maxGlints = 8)
    {
        if (r.isEmpty() || r.getWidth() < 24.0f || strength <= 0.0f) return;
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, cornerRadius);
        g.reduceClipRegion (clip);
        // Exposed upper rim; the far edges recede instead of forming a uniform outline.
        g.setGradientFill (juce::ColourGradient (rim.withAlpha (0.80f * strength), r.getX(), r.getY(),
                                                rim.withAlpha (0.10f * strength), r.getX(),
                                                r.getY() + r.getHeight() * 0.62f, false));
        g.drawRoundedRectangle (r.reduced (0.6f), juce::jmax (1.0f, cornerRadius - 0.6f), 1.2f);
        // Dark neighbour inside the pale edge.
        g.setGradientFill (juce::ColourGradient (contact.withAlpha (0.52f * strength), r.getX(), r.getY() + 2.0f,
                                                contact.withAlpha (0.28f * strength), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (1.7f), juce::jmax (1.0f, cornerRadius - 1.7f), 1.0f);
        panelTopGlints (g, r, cornerRadius, rim, contact, strength, maxGlints);
    }

    inline void toolbarEnclosure (juce::Graphics& g, juce::Rectangle<float> outer,
                                  juce::Rectangle<float> settingsRow)
    {
        if (outer.isEmpty()) return;
        const float r = radiusLg;
        const auto rim = toolbarRim();
        const auto contact = toolbarContact();
        juce::Path shape;
        shape.addRoundedRectangle (outer, r);
        {   // Bounded cool bloom: the contour fades out before the lower half,
            // so this reads as exposure light, not a neon outline.
            juce::Graphics::ScopedSaveState bloom (g);
            g.setGradientFill (juce::ColourGradient (rim.withAlpha (0.20f), outer.getX(), outer.getY() - 4.0f,
                                                    rim.withAlpha (0.0f), outer.getX(), outer.getY() + outer.getHeight() * 0.48f, false));
            g.drawRoundedRectangle (outer.expanded (2.5f), r + 2.5f, 4.0f);
            g.setGradientFill (juce::ColourGradient (rim.withAlpha (0.09f), outer.getX(), outer.getY() - 5.0f,
                                                    rim.withAlpha (0.0f), outer.getX(), outer.getY() + outer.getHeight() * 0.40f, false));
            g.drawRoundedRectangle (outer.expanded (5.0f), r + 5.0f, 5.0f);
        }
        g.setGradientFill (juce::ColourGradient (toolbarFaceHigh(), 0.0f, outer.getY(),
                                                toolbarFaceLow(), 0.0f, outer.getBottom(), false));
        g.fillPath (shape);
        {   // darker settings row inside the same rounded enclosure
            juce::Graphics::ScopedSaveState rowState (g);
            g.reduceClipRegion (shape);
            g.setGradientFill (juce::ColourGradient (toolbarRowTop(), 0.0f, settingsRow.getY(),
                                                    toolbarRowBottom(), 0.0f, settingsRow.getBottom(), false));
            g.fillRect (settingsRow);
            g.setColour (contact.withAlpha (0.45f));
            g.fillRect (settingsRow.getX() + 2.0f, settingsRow.getY(), settingsRow.getWidth() - 4.0f, 1.0f);
        }
        {   // exposed pale rim with a dark neighbour inside: no uniform accent outline
            g.setGradientFill (juce::ColourGradient (rim.withAlpha (0.82f), 0.0f, outer.getY(),
                                                    rim.withAlpha (0.12f), 0.0f, outer.getY() + outer.getHeight() * 0.62f, false));
            g.drawRoundedRectangle (outer.reduced (0.6f), r - 0.6f, 1.2f);
            g.setGradientFill (juce::ColourGradient (contact.withAlpha (0.55f), 0.0f, outer.getY() + 2.0f,
                                                    contact.withAlpha (0.22f), 0.0f, outer.getBottom(), false));
            g.drawRoundedRectangle (outer.reduced (1.7f), r - 1.7f, 1.0f);
        }
        panelTopGlints (g, outer, r, rim, contact);
    }

    // BPM readout: a recessed dark field inside a warm rim. Text drawing is unchanged.
    inline void toolbarReadoutWell (juce::Graphics& g, juce::Rectangle<float> r, float cornerRadius)
    {
        if (r.isEmpty()) return;
        g.setColour (toolbarContact().withAlpha (0.55f));
        g.fillRoundedRectangle (r.expanded (0.8f), cornerRadius + 0.8f);
        g.setGradientFill (juce::ColourGradient (deep.darker (0.35f), 0.0f, r.getY(),
                                                deep.brighter (0.05f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, cornerRadius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), 0.0f, r.getY(),
                                                juce::Colours::black.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.55f, false));
        g.fillRoundedRectangle (r, cornerRadius);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), 0.0f, r.getY(),
                                                accent.withAlpha (0.20f), 0.0f, r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (0.5f), cornerRadius - 0.5f, 1.0f);
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (r.getX() + 4.0f, r.getY() + 1.0f, r.getWidth() - 8.0f, 1.0f);
    }

    // Narrow exposed bevel; no extra filled face, and no changed hit geometry.
    inline void materialBevel (juce::Graphics& g, juce::Rectangle<float> r, float cornerRadius,
                               juce::Colour light, float strength = 1.0f)
    {
        g.setGradientFill (juce::ColourGradient (materialContact().withAlpha (0.0f), r.getX(), r.getCentreY(),
                                                materialContact().withAlpha (0.85f * strength), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (0.6f), cornerRadius, 1.2f);
        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.65f * strength), r.getX(), r.getY(),
                                                light.withAlpha (0.0f), r.getX() + r.getWidth() * 0.60f,
                                                r.getY() + juce::jmin (18.0f, r.getHeight()), false));
        g.drawRoundedRectangle (r.reduced (0.5f), cornerRadius, 1.0f);
    }

    // Drums keeps the Primary role, so the exposed top glints follow the tab
    // colour instead of the cool material light.
    inline juce::Colour drumRim() { return drumAccent().interpolatedWith (juce::Colours::white, 0.18f); }

    inline void drumEnclosure (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path shape;
        shape.addRoundedRectangle (r, radiusLg);
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (panel.interpolatedWith (raised, 0.18f), r.getX(), r.getY(),
                                                panel.interpolatedWith (deep, 0.48f), r.getRight(), r.getBottom(), false));
        g.fillPath (shape);
        // v0.86: directional exposed edge replaces the uniform orange outline;
        // the orange left stripe and tabs keep the panel's identity. The glints
        // follow the Drums tab colour, capped at 5 across the wide panel.
        panelExposedEdge (g, r, radiusLg, drumRim(), materialContact(), 1.0f, 5);
        // Drums remains edge-painted, not Bass's 1px inset. The same solid
        // width and mild inner end rounding keep mixer alignment unchanged.
        g.setColour (drumAccent());
        g.fillPath (bassStripePath (r));
    }

    inline void drumTab (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path tab;
        tab.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() + 6.0f,
                                 radiusLg, radiusLg, true, true, false, false);
        g.reduceClipRegion (tab);
        g.setGradientFill (juce::ColourGradient (panel, r.getTopLeft(), panel.interpolatedWith (deep, 0.45f),
                                                r.getBottomRight(), false));
        g.fillPath (tab);
        g.setGradientFill (juce::ColourGradient (drumAccent().withAlpha (0.42f), r.getX(), r.getY(),
                                                drumAccent().withAlpha (0.0f), r.getX(), r.getY() + 9.0f, false));
        g.fillPath (tab);
        g.setGradientFill (juce::ColourGradient (drumAccent(), r.getX(), r.getY() + 1.0f,
                                                drumAccent().withAlpha (0.0f), r.getX(), r.getY() + 8.0f, false));
        g.drawRoundedRectangle (r.withTrimmedBottom (-6.0f).reduced (0.75f), radiusLg, 1.5f);
        // No reflection beneath either tab caption.
    }

    // Only the existing 3px Drums surround. Clip the new layers to the
    // outside: the original cells and inter-lane gutters survive.
    inline void materialWellRim (juce::Graphics& g, juce::Rectangle<float> well)
    {
        wellRing (g, well, 3.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.excludeClipRegion (well.toNearestInt());
        const auto light = materialCoolLight();
        g.setColour (materialContact());
        g.fillRoundedRectangle (well.expanded (1.4f), radiusLg);
        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.72f), well.getX(), well.getY(),
                                                light.withAlpha (0.08f), well.getRight(), well.getBottom(), false));
        g.drawRoundedRectangle (well.expanded (2.5f), radiusLg + 1.0f, 1.0f);
        juce::ColourGradient reflection (light.withAlpha (0.0f), well.getX() + 8.0f, well.getBottom(),
                                         light.withAlpha (0.0f), well.getX() + well.getWidth() * 0.4f, well.getBottom(), false);
        reflection.addColour (0.16, light.withAlpha (0.24f));
        g.setGradientFill (reflection);
        g.fillRect (well.getX() + 8.0f, well.getBottom() + 1.7f, juce::jmax (0.0f, well.getWidth() * 0.4f - 8.0f), 0.8f);
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
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        const auto b = juce::Rectangle<float> ((float) w, (float) h);
        const bool bass = Theme::materialControl (box);
        if (bass)
        {
            Theme::bassShadowState (box, down);
            Theme::bassControlTile (g, b, box.findColour (juce::ComboBox::backgroundColourId), 2.0f,
                                    box.isEnabled(), box.isMouseOver (true), down, box.hasKeyboardFocus (true));
        }
        else
            Theme::framedTile (g, b, box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f), 2.0f);
        juce::Path arrow;
        const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
        arrow.lineTo (cx, cy + 2.0f);
        arrow.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (bass && ! box.isEnabled() ? 0.45f : 1.0f));
        g.strokePath (arrow, juce::PathStrokeType (1.6f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                               bool over, bool down) override
    {
        auto c = bgc;
        if (down) c = c.darker (0.15f); else if (over) c = c.brighter (0.12f);
        const auto r = b.getLocalBounds().toFloat();
        const float ring = r.getHeight() < 26.0f ? 2.0f : 2.5f;
        if (Theme::materialControl (b))
        {
            Theme::bassShadowState (b, down);
            Theme::bassControlTile (g, r, bgc, ring, b.isEnabled(), over, down, b.hasKeyboardFocus (true));
        }
        else
            Theme::framedTile (g, r, c, ring);
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

    // Stock JUCE message-box surface, with the compact themed alert symbol kept.
    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert,
                       const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout) override
    {
        juce::ignoreUnused (textArea);

        const auto cornerSize = 4.0f;
        g.setColour (alert.findColour (juce::AlertWindow::outlineColourId));
        g.drawRoundedRectangle (alert.getLocalBounds().toFloat(), cornerSize, 2.0f);

        const auto bounds = alert.getLocalBounds().reduced (1);
        g.reduceClipRegion (bounds);

        g.setColour (alert.findColour (juce::AlertWindow::backgroundColourId));
        g.fillRoundedRectangle (bounds.toFloat(), cornerSize);

        int iconSpaceUsed = 0;
        const auto type = alert.getAlertType();

        if (type != juce::MessageBoxIconType::NoIcon)
        {
            const auto accent = type == juce::MessageBoxIconType::WarningIcon ? Theme::orange
                              : type == juce::MessageBoxIconType::InfoIcon    ? Theme::blue
                                                                              : Theme::cyan;
            const juce::Rectangle<float> badge ((float) bounds.getX() + 20.0f,
                                                (float) bounds.getY() + 18.0f, 30.0f, 30.0f);
            g.setColour (accent.withAlpha (0.14f));
            g.fillEllipse (badge.expanded (4.0f));
            g.setColour (accent.withAlpha (0.85f));
            g.drawEllipse (badge, 1.5f);
            g.setColour (accent);
            g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
            g.drawText (type == juce::MessageBoxIconType::WarningIcon ? "!"
                      : type == juce::MessageBoxIconType::InfoIcon    ? "i" : "?",
                        badge, juce::Justification::centred);
            iconSpaceUsed = 80;
        }

        g.setColour (alert.findColour (juce::AlertWindow::textColourId));
        const juce::Rectangle<int> alertBounds (bounds.getX() + iconSpaceUsed, 30,
                                                bounds.getWidth(),
                                                bounds.getHeight() - getAlertWindowButtonHeight() - 20);
        textLayout.draw (g, alertBounds.toFloat());
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

            if ((bool) s.getProperties().getWithDefault ("masterOutputFader", false))
            {
                // Same tactile metal face as the material knobs, kept inside the
                // existing 14px cap so travel and hit area are unchanged.
                const juce::Rectangle<float> cap (cx - 7.0f, pos - 7.0f, 14.0f, 14.0f);
                g.setGradientFill (juce::ColourGradient (Theme::bassKnobLight(), cap.getX() + 3.0f, cap.getY() + 2.0f,
                                                        Theme::bassKnobShade(), cap.getRight() - 2.0f, cap.getBottom() - 1.0f, false));
                g.fillEllipse (cap);
                g.setColour (Theme::bassKnobContact().withAlpha (0.90f));
                g.drawEllipse (cap.reduced (0.5f), 1.0f);
                g.setColour (accent.withAlpha (s.isEnabled() ? 0.55f : 0.20f));
                g.drawEllipse (cap.reduced (1.5f), 1.0f);
                g.setGradientFill (juce::ColourGradient (Theme::bassKnobGlint().withAlpha (0.75f), cap.getX() + 3.0f, cap.getY() + 2.0f,
                                                        Theme::bassKnobGlint().withAlpha (0.0f), cap.getRight() - 2.0f, cap.getCentreY(), true));
                g.drawEllipse (cap.reduced (2.0f), 0.8f);
            }
            else
            {
                g.setColour (Theme::text);
                g.fillEllipse (cx - 7.0f, pos - 7.0f, 14.0f, 14.0f);
            }
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
        const bool bass = (bool) s.getProperties().getWithDefault ("bassSoundKnob", false)
                       || (bool) s.getProperties().getWithDefault ("bassVolumeKnob", false);
        const bool track = (bool) s.getProperties().getWithDefault ("trackKnob", false);
        const bool material = bass || track || (bool) s.getProperties().getWithDefault ("drumKnob", false);
        const bool smallDrum = (bool) s.getProperties().getWithDefault ("drumKnob", false) && juce::jmin (w, h) <= 26;
        const bool subPoc = bass && (bool) s.getProperties().getWithDefault ("bassSubLightPoc", false);

        if (material && ! subPoc)
        {
            const float strength = ! s.isEnabled() ? 0.35f : s.isMouseButtonDown() ? 0.5f : 1.0f;
            for (int layer = 3; layer >= 0; --layer)
            {
                const float spread = layer * 0.6f;
                g.setColour (Theme::bassShadow().withAlpha (0.055f * strength));
                g.fillEllipse (c.x - r - spread, c.y - r * 0.84f + 1.5f - spread,
                               2.0f * (r + spread), 2.0f * (r * 0.84f + spread));
            }
        }
        if (! subPoc) // Sub's parent supplies this base before its Gaussian near halo.
        {
            g.setColour (Theme::frame);                           // unchanged ring for all other knobs
            g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        }
        if (material)
        {
            // Only the existing face is restyled; ring, geometry and input stay untouched.
            juce::Graphics::ScopedSaveState ss (g);
            const juce::Rectangle<float> face (c.x - r + 5.5f, c.y - r + 5.5f, 2 * r - 11.0f, 2 * r - 11.0f);
            juce::Path faceClip;
            faceClip.addEllipse (face);
            g.reduceClipRegion (faceClip);
            const bool enabled = s.isEnabled();
            const bool down = enabled && s.isMouseButtonDown();
            const bool over = enabled && s.isMouseOverOrDragging();
            auto stateColour = [&] (juce::Colour col)
            {
                if (! enabled) return col.interpolatedWith (Theme::raised, 0.65f);
                return down ? col.darker (0.10f) : over ? col.brighter (0.08f) : col;
            };
            juce::ColourGradient surface (stateColour (Theme::bassKnobLight()), face.getX(), face.getY(),
                                          stateColour (Theme::bassKnobShade()), face.getRight(), face.getBottom(), false);
            surface.addColour (0.45, stateColour (Theme::raised));
            g.setGradientFill (surface);
            g.fillRect (face);

            // Narrow lower contact bevel, then a short upper-left reflection tapering to nothing.
            g.setGradientFill (juce::ColourGradient (Theme::bassKnobContact().withAlpha (0.0f), c.x, c.y,
                                                    stateColour (Theme::bassKnobContact()), c.x, face.getBottom(), false));
            g.drawEllipse (face.reduced (0.45f), 0.9f);
            const auto glint = stateColour (Theme::bassKnobGlint());
            const float strength = enabled && s.hasKeyboardFocus (true) ? 0.95f : 0.70f;
            g.setGradientFill (juce::ColourGradient (glint.withAlpha (strength), face.getX() + 3.0f, face.getY() + 1.0f,
                                                    glint.withAlpha (0.0f), face.getRight() - 1.0f, c.y, true));
            juce::Path reflection;
            const float edgeR = r - 6.2f;
            reflection.addCentredArc (c.x, c.y, edgeR, edgeR, 0.0f, -1.15f, 0.95f, true);
            g.strokePath (reflection, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (track)
            {
                // Track volume/pan: give the drag circle a defined inner grip
                // without changing the face, ring or hit area.
                const auto grip = face.reduced (face.getWidth() * 0.22f);
                g.setGradientFill (juce::ColourGradient (stateColour (Theme::bassKnobLight()).brighter (0.06f), grip.getX() + 2.0f, grip.getY() + 1.0f,
                                                        stateColour (Theme::bassKnobShade()), grip.getRight() - 1.0f, grip.getBottom() - 1.0f, false));
                g.fillEllipse (grip);
                g.setColour (stateColour (Theme::bassKnobContact()).withAlpha (0.85f));
                g.drawEllipse (grip.reduced (0.5f), 0.8f);
                g.setColour (accent.withAlpha (enabled ? 0.30f : 0.12f));
                g.drawEllipse (grip.reduced (1.4f), 0.8f);
            }
            if (subPoc)
            {
                // Narrow raised metal bevel and inner reflection, never a centre wash.
                const float light = ! enabled ? 0.22f : down ? 0.42f : over ? 1.0f : 0.85f;
                g.setGradientFill (juce::ColourGradient (Theme::bassSubSpecular().withAlpha (light),
                    face.getX() + 2.0f, face.getY(), Theme::bassSubSpecular().withAlpha (0.0f),
                    face.getRight(), face.getBottom() - 2.0f, false));
                g.drawEllipse (face.reduced (0.5f), 0.8f);
                g.setGradientFill (juce::ColourGradient (Theme::bassKnobContact().withAlpha (0.0f),
                    c.x, c.y, Theme::bassKnobContact().withAlpha (0.90f), c.x, face.getBottom(), false));
                g.drawEllipse (face.reduced (1.15f), 0.7f);
                g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.13f * light),
                    face.getX(), c.y, accent.withAlpha (0.0f), c.x, c.y, false));
                g.drawEllipse (face.reduced (1.9f), 0.7f);
            }
        }
        else
        {
            g.setColour (Theme::raised);                          // unchanged legacy face for every unmarked slider
            g.fillEllipse (c.x - r + 5.5f, c.y - r + 5.5f, 2 * r - 11.0f, 2 * r - 11.0f);
        }

        // Functional signals are always painted last, above all material layers.
        juce::Path bgArc, valArc;
        const float ar = r - 2.6f;
        bgArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, endA, true);
        g.setColour (Theme::border.brighter (0.25f));
        g.strokePath (bgArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float valueStart = (bool) s.getProperties().getWithDefault ("uni", false) ? startA : mid;
        // A zero-length JUCE arc still contains a line with coincident endpoints;
        // rounded strokes can turn that into a dot. Bass zero must stay empty.
        if (! material || valueStart != ang)
            valArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, valueStart, ang, true);
        if (subPoc)
            valArc = Theme::BassSubGeometry::make (juce::Rectangle<int> (x, y, w, h).toFloat(),
                prop, startA, endA, (bool) s.getProperties().getWithDefault ("uni", false)).valueArc;
        if (material && ! subPoc)
        {
            juce::Graphics::ScopedSaveState haloState (g);
            // Exclude the unchanged metallic face, not the surrounding ring.
            // Most of the old 4/6px halo was covered by the 2.6px value stroke.
            juce::Path haloClip;
            haloClip.setUsingNonZeroWinding (false);
            haloClip.addRectangle (juce::Rectangle<int> (x, y, w, h).toFloat());
            haloClip.addEllipse (c.x - r + 5.5f, c.y - r + 5.5f, 2 * r - 11.0f, 2 * r - 11.0f);
            g.reduceClipRegion (haloClip);
            const float strength = (s.isEnabled() ? (! bass && s.isMouseButtonDown() ? 0.5f : 1.0f) : 0.35f)
                                 * (smallDrum ? 0.45f : 1.0f);
            for (int layer = 0; layer < 6; ++layer)
            {
                g.setColour (accent.withAlpha ((0.045f + layer * 0.026f) * strength));
                g.strokePath (valArc, juce::PathStrokeType (smallDrum ? 6.0f - layer * 0.5f : 9.0f - layer,
                                                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        g.setColour (accent);
        g.strokePath (valArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (material)
        {
            g.setColour ((subPoc ? Theme::bassSubCore() : Theme::bassArcCore (accent))
                         .withAlpha (s.isEnabled() ? (subPoc ? 0.70f : 0.48f) : 0.17f));
            g.strokePath (valArc, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::Path tick;
        tick.addRoundedRectangle (-1.25f, -(r - 7.0f), 2.5f, r * 0.36f, 1.0f);
        g.setColour (subPoc && s.isEnabled() ? Theme::text.interpolatedWith (juce::Colours::white, 0.55f)
                                            : Theme::text);
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
        getProperties().set ("toolbarControl", true);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        auto bgc = on ? colour : Theme::raised;
        if (on && icon == Record)   // gentle pulse while recording
            bgc = bgc.interpolatedWith (juce::Colours::white, 0.12f + 0.12f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f));
        Theme::bassShadowState (*this, down);
        Theme::bassControlTile (g, getLocalBounds().toFloat(), bgc, 2.5f,
                                isEnabled(), over, down, hasKeyboardFocus (true));

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
    explicit ToolButton (Icon i) : juce::Button ({}), icon (i)
    {
        setWantsKeyboardFocus (false);
        getProperties().set ("toolbarControl", true);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool en = isEnabled();
        Theme::bassShadowState (*this, down);
        Theme::bassControlTile (g, getLocalBounds().toFloat(), Theme::raised, 2.0f,
                                en, over, down, hasKeyboardFocus (true));
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
        if (r.getWidth() < 1.0f || r.getHeight() < 1.0f) return;
        const float rad = juce::jlimit (1.0f, 3.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.32f);
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path well;
        well.addRoundedRectangle (r, rad);
        g.reduceClipRegion (well);

        // Flat well: one dark fill, no gradients or burned-in scale.
        g.setColour (Theme::deep.darker (0.55f));
        g.fillRect (r);

        const float p = pos (level), hp = pos (hold);
        const auto fill = Theme::meterFill();
        if (p > 0.01f)
        {
            const juce::Rectangle<float> bar = horizontal
                ? juce::Rectangle<float> (r.getX(), r.getY(), r.getWidth() * p, r.getHeight())
                : juce::Rectangle<float> (r.getX(), r.getBottom() - r.getHeight() * p, r.getWidth(), r.getHeight() * p);
            g.setColour (fill);
            g.fillRect (bar);

            // Small glow at the leading edge only; still clipped to the well.
            if (horizontal)
            {
                const float x = r.getX() + r.getWidth() * p;
                g.setGradientFill (juce::ColourGradient (fill.withAlpha (0.30f), x, 0.0f,
                                                        fill.withAlpha (0.0f), x + 4.0f, 0.0f, false));
                g.fillRect (x, r.getY(), 4.0f, r.getHeight());
            }
            else
            {
                const float y = r.getBottom() - r.getHeight() * p;
                g.setGradientFill (juce::ColourGradient (fill.withAlpha (0.30f), 0.0f, y,
                                                        fill.withAlpha (0.0f), 0.0f, y - 4.0f, false));
                g.fillRect (r.getX(), y - 4.0f, r.getWidth(), 4.0f);
            }
        }

        if (hp > 0.02f)
        {
            // Peak-hold stays Tokyo blue in every theme. Snap to whole pixels so
            // the 2px marker stays crisp at any meter size.
            g.setColour (Theme::meterPeak());
            if (horizontal)
            {
                const float x = std::floor (r.getX() + r.getWidth() * hp);
                g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
            }
            else
            {
                const float y = std::floor (r.getBottom() - r.getHeight() * hp);
                g.fillRect (r.getX(), y - 1.0f, r.getWidth(), 2.0f);
            }
        }

        // Flat contact outline; no directional highlight.
        g.setColour (Theme::border.withAlpha (0.40f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
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
