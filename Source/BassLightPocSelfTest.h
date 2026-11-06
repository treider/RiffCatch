#pragma once
#include "ui/BassPanel.h"

// Real production masks/components, offscreen only. No devices/settings/input automation.
struct BassLightPocTest
{
    using Check = std::function<void (bool, const juce::String&)>;
    static float luminance (juce::Colour c)
    {
        return 0.2126f * c.getRed() + 0.7152f * c.getGreen() + 0.0722f * c.getBlue();
    }
    template <typename Paint>
    static juce::Image render (int w, int h, float scale, Paint paint)
    {
        juce::Image image (juce::Image::ARGB, juce::roundToInt (w * scale), juce::roundToInt (h * scale), true);
        { juce::Graphics g (image); g.addTransform (juce::AffineTransform::scale (scale)); paint (g); }
        return image;
    }
    static void artifact (const juce::Image& image, const juce::String& name)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rc81-evidence");
        dir.createDirectory();
        if (auto out = dir.getChildFile ("synthetic-" + name + ".png").createOutputStream())
        {
            out->setPosition (0); out->truncate();
            juce::PNGImageFormat().writeImageToStream (image, *out);
        }
    }
    static void run (const Check& check)
    {
        // Exercise the bounded backend-support failure, not invalid production bounds.
        struct Diagnostic : juce::Logger
        {
            juce::Logger* previous = juce::Logger::getCurrentLogger();
            juce::String message;
            Diagnostic() { juce::Logger::setCurrentLogger (this); }
            ~Diagnostic() override { juce::Logger::setCurrentLogger (previous); }
            void logMessage (const juce::String& text) override { message += text; }
        };
        {
            Diagnostic diagnostic;
            BassLightPoc::Masks masks;
            const bool ok = masks.build ({ 0, 0, 2, 2 }, 1.0f, { 0.3f, 0.2f, 0.1f },
                [] (juce::Graphics& g) { g.fillAll (juce::Colours::white); },
                [] (float, float) { return 1.0f; });
            check (! ok && masks.status == BassLightPoc::Masks::Status::paddingFailure
                   && masks.builds == 1 && masks.gaussianCalls == 9 && masks.imageAllocations == 12
                   && masks.layers[0].isNull() && masks.layers[1].isNull() && masks.layers[2].isNull()
                   && diagnostic.message.contains ("mask padding failed after 3 attempts"),
                   "v81 bounded mask failure returns false, retains failure status and logs diagnostic");
        }
        const Theme::Preset saved { "live", Theme::blue.getARGB(), Theme::purple.getARGB(), Theme::cyan.getARGB(),
                                   Theme::bg.getARGB(), Theme::panel.getARGB(), Theme::text.getARGB() };
        const bool legacy = Theme::legacyTracks;
        Theme::applyPreset ({ "Neo-Retro", 0xffff780a, 0xfff2ab24, 0xfff6c343, 0xff0b1018, 0xff202630, 0xffdce3ec }, false);
        auto engine = std::make_unique<AudioEngine>();
        engine->prepareForTest (44100.0, 2);
        RiffLookAndFeel laf;
        BassPanel panel (*engine);
        panel.setLookAndFeel (&laf);
        panel.setBounds (20, 40, 1008, 208);
        auto& sub = panel.knobs[0]->s;
        int optins = 0;
        bool plainNeighbours = true;
        for (auto* k : panel.knobs)
        {
            optins += (bool) k->s.getProperties().getWithDefault ("bassSubLightPoc", false) ? 1 : 0;
            plainNeighbours &= (dynamic_cast<BassLightPoc::Slider*> (&k->s) != nullptr) == (k->name == "Sub");
        }
        check (optins == 1 && plainNeighbours && sub.getTooltip() == "Sub"
               && sub.getBounds() == juce::Rectangle<int> (40, panel.headerHeight() + 34, 38, 38),
               "v80 POC: exactly Sub opted in; native 38px bounds and shared header untouched");
        const auto geom = Theme::BassSubGeometry::make (panel.subSliderArea(), 0.5f,
            sub.getRotaryParameters().startAngleRadians, sub.getRotaryParameters().endAngleRadians, true);
        check (geom.centre == juce::Point<float> (59.0f, 95.0f) && geom.radius == 15.0f,
               "v80 POC: LAF and parent use actual SliderLayout, radius15/face9.5/arc12.4");
        const float a = sub.getRotaryParameters().startAngleRadians, z = sub.getRotaryParameters().endAngleRadians;
        for (float scale : { 1.0f, 1.5f })
        {
            const auto tag = " " + juce::String (scale * 96.0f, 0) + "dpi synthetic";
            BassLightPoc::Frame frame;
            const juce::Rectangle<float> b (20, 20, 608, 240);
            frame.ensure (b, scale);
            auto receiver = [&] (bool light)
            {
                return render (660, 285, scale, [&] (juce::Graphics& g)
                {
                    g.fillAll (Theme::bg);
                    Theme::cardShadow (g, b);
                    if (light) frame.drawOuter (g);
                    Theme::bassEnclosure (g, b, false, true);
                });
            };
            auto baseline = receiver (false), lit = receiver (true);
            auto difference = [&] (float x, float y)
            {
                const int px = juce::roundToInt (x * scale), py = juce::roundToInt (y * scale);
                return luminance (lit.getPixelAt (px, py)) - luminance (baseline.getPixelAt (px, py));
            };
            // Pixel positions relative to the actual x=21 emitting stripe, not
            // the component rectangle (which has the preserved 1px inset).
            const float f2 = difference (19, 120), f6 = difference (15, 120), f10 = difference (11, 120);
            // v81 visible bloom request supersedes the rejected subtle v80 upper limits.
            // Keep a bounded budget, positive wide tail and independent decay checks.
            check (f2 >= 30.0f && f2 <= 60.0f && f6 >= 8.0f && f6 <= 23.0f && f10 >= 1.0f && f10 <= 8.0f,
                "v81 frame outside crisp silhouette Y' d2/d6/d10=" + juce::String (f2, 2) + "/"
                + juce::String (f6, 2) + "/" + juce::String (f10, 2) + tag);
            bool monotonic = true;
            float previous = 255.0f;
            for (int d = 1; d <= 12; ++d)
            {
                const auto delta = difference (21.0f - d, 120);
                monotonic &= delta <= previous + 1.0f;
                previous = delta;
            }
            check (monotonic && frame.masks.borderAlpha <= 1, "v80 frame smooth outside tail and transparent padded border" + tag);
            check (std::abs (difference (15, 210) - f6) <= 1.0f && difference (3, 120) <= 1.0f,
                   "v81 frame broad tail sustained at mid/lower left and fades away by d18" + tag);
            bool stripe = true, tabGap = true;
            for (int y = 30; y < 250; ++y)
                for (int x = 21; x < 25; ++x)
                    stripe &= lit.getPixelAt (juce::roundToInt (x * scale), juce::roundToInt (y * scale)) == Theme::bassLeftRibbon();
            for (int y = 0; y < 20; ++y)
                for (int x = 10; x < 250; ++x)
                    tabGap &= lit.getPixelAt (juce::roundToInt (x * scale), juce::roundToInt (y * scale))
                           == baseline.getPixelAt (juce::roundToInt (x * scale), juce::roundToInt (y * scale));
            check (stripe && tabGap, "v80 frame solid4px unchanged and no light through tab corners/gap" + tag);
            artifact (lit, "frame" + tag);
            const auto count = frame.masks.builds, calls = frame.masks.gaussianCalls, allocations = frame.masks.imageAllocations;
            for (int i = 0; i < 30; ++i) { frame.ensure (b, scale); receiver (true); }
            check (frame.masks.builds == count && frame.masks.gaussianCalls == calls && frame.masks.imageAllocations == allocations,
                   "v80 frame warm 30 repaints: 0 Gaussian calls / 0 owned-mask allocations" + tag);

            BassLightPoc::Sub light;
            const juce::Rectangle<float> knob (20, 20, 38, 38);
            for (float value : { 0.0f, 0.5f, 1.0f })
            {
                light.ensure (knob, value, scale, a, z, true);
                auto draw = [&] (bool enabled, bool over, bool down)
                {
                    return render (90, 85, scale, [&] (juce::Graphics& g)
                    { g.fillAll (Theme::panel); light.draw (g, Theme::cyan, enabled, over, down); });
                };
                const auto normal = draw (true, false, false);
                const auto hover = draw (true, true, false), down = draw (true, true, true), disabled = draw (false, false, false);
                auto delta = [&] (float x, float y)
                {
                    return luminance (normal.getPixelAt (juce::roundToInt (x * scale), juce::roundToInt (y * scale)))
                         - luminance (Theme::panel);
                };
                const float k2 = delta (22, 39), k4 = delta (20, 39), k7 = delta (17, 39);
                if (value > 0.0f)
                {
                    check (k2 >= 14.0f && k2 <= 32.0f && k4 >= 6.0f && k4 <= 18.0f && k7 >= 1.0f && k7 <= 6.0f,
                        "v81 Sub outside ring Y' d2/d4/d7=" + juce::String (k2, 2) + "/" + juce::String (k4, 2)
                        + "/" + juce::String (k7, 2) + " value=" + juce::String (value) + tag);
                    const int px = juce::roundToInt (22 * scale), py = juce::roundToInt (39 * scale);
                    check (luminance (hover.getPixelAt (px, py)) > luminance (normal.getPixelAt (px, py))
                           && luminance (normal.getPixelAt (px, py)) > luminance (down.getPixelAt (px, py))
                           && luminance (down.getPixelAt (px, py)) > luminance (disabled.getPixelAt (px, py)),
                           "v80 isolated production halo paints hover > normal > down > disabled" + tag);
                    check (delta (19, 39) > 0.0f, "v80 Sub light survives beyond unchanged Slider component boundary" + tag);
                    bool decay = true;
                    float previousSub = 255.0f;
                    for (int d = 1; d <= 14; ++d)
                    {
                        const float current = delta (24.0f - d, 39);
                        decay &= current <= previousSub + 1.0f;
                        previousSub = current;
                    }
                    check (decay && previousSub <= 1.0f, "v81 Sub broad tail decays to receiver, no extra ring" + tag);
                    artifact (hover, "Sub-halo-hover-" + juce::String (value) + tag);
                    artifact (down, "Sub-halo-down-" + juce::String (value) + tag);
                    artifact (disabled, "Sub-halo-disabled-" + juce::String (value) + tag);
                }
                else
                {
                    bool empty = true;
                    for (int y = 0; y < normal.getHeight(); ++y)
                        for (int x = 0; x < normal.getWidth(); ++x) empty &= normal.getPixelAt (x, y) == Theme::panel;
                    check (empty, "v80 Sub0 has no fabricated emitting arc or halo" + tag);
                }
                bool caption = true, contained = true;
                const auto dirty = knob.expanded ((float) BassLightPoc::Sub::repaintOutset).withBottom (knob.getBottom() - 1);
                for (int y = 0; y < hover.getHeight(); ++y)
                    for (int x = 0; x < hover.getWidth(); ++x)
                        if (! dirty.contains ((x + 0.5f) / scale, (y + 0.5f) / scale))
                            contained &= hover.getPixelAt (x, y) == Theme::panel;
                check (contained && light.masks.status == BassLightPoc::Masks::Status::ready,
                       "v81 strongest halo fits parent repaint union including caption cutoff" + tag);
                for (int y = juce::roundToInt (58 * scale); y < normal.getHeight(); ++y)
                    for (int x = 0; x < normal.getWidth(); ++x) caption &= normal.getPixelAt (x, y) == Theme::panel;
                check (caption && light.masks.borderAlpha <= 1, "v80 Sub caption receiver untinted / mask border transparent" + tag);
                const int warm = light.masks.builds, warmCalls = light.masks.gaussianCalls, warmAlloc = light.masks.imageAllocations;
                for (int i = 0; i < 30; ++i)
                {
                    light.ensure (knob, value, scale, a, z, true);
                    draw (true, true, false); draw (true, true, true); draw (false, false, false);
                }
                check (light.masks.builds == warm && light.masks.gaussianCalls == warmCalls && light.masks.imageAllocations == warmAlloc,
                       "v80 Sub warm 90 hover/down/disabled paints: 0 Gaussian / 0 owned-mask allocations" + tag);
                artifact (normal, "Sub-halo-" + juce::String (value) + tag);
            }
            check (light.masks.builds == 3 && light.masks.gaussianCalls == 9,
                   "v80 Sub current-snapshot sequence 0/.5/1: exactly3 builds/9 Gaussian" + tag);
            check (true, "v81 timings last small Sub=" + juce::String (light.masks.lastBuildMs, 3)
                   + "ms; static frame=" + juce::String (frame.masks.lastBuildMs, 3)
                   + "ms; frame mask=" + juce::String (frame.masks.layers[0].getWidth()) + "x"
                   + juce::String (frame.masks.layers[0].getHeight()) + tag);
            const int built = light.masks.builds;
            light.ensure (knob.translated (1, 0), 1, scale, a, z, true);
            light.ensure (knob.translated (1, 0), 1, scale + 0.25f, a, z, true);
            check (light.masks.builds == built + 2, "v80 Sub geometry and physical DPI invalidate snapshot" + tag);
            std::vector<double> timings;
            for (int i = 0; i <= 24; ++i)
            {
                light.ensure (knob, i / 24.0f, scale, a, z, true);
                timings.push_back (light.masks.lastBuildMs);
            }
            std::sort (timings.begin(), timings.end());
            const auto image = light.masks.layers[0];
            check (light.masks.builds == built + 27 && light.masks.layers.size() == 3,
                   "v80 25-value sweep retains only3 current masks, no per-value map" + tag);
            check (true, "v80 25-value Sub rebuild median=" + juce::String (timings[12], 3)
                   + "ms max=" + juce::String (timings.back(), 3) + "ms; mask=" + juce::String (image.getWidth())
                   + "x" + juce::String (image.getHeight()) + "; borderAlpha=" + juce::String (light.masks.borderAlpha) + tag);
            frame.ensure (b.withWidth (609), scale);
            frame.ensure (b.withWidth (609), scale + 0.25f);
            check (frame.masks.builds == count + 2, "v80 frame geometry and physical DPI invalidate snapshot" + tag);
        }

        auto paintPanel = [&] (float scale = 1.0f)
        {
            return render (panel.getWidth(), panel.getHeight(), scale, [&] (juce::Graphics& g)
            { panel.paintEntireComponent (g, true); });
        };
        for (int width : { 1008, 608 })
        {
            panel.setSize (width, width == 1008 ? 208 : 240);
            for (float value : { 0.0f, 0.5f, 1.0f })
            {
                engine->bass.sub = value;
                const auto old = panel.lastSubLightBounds;
                const int invalidations = panel.subInvalidations;
                panel.refreshFromEngine();
                check (sub.getValue() == value && panel.subInvalidations > invalidations
                       && panel.lastSubInvalidation.contains (old),
                       "v80 silent engine refresh preserves mapping; parent invalidates old/new halo union");
                auto image = paintPanel();
                artifact (image, "Bass-" + juce::String (width) + "-Sub" + juce::String (value));
                const int f = panel.frameLight.masks.builds, s = panel.subLight.masks.builds;
                for (int i = 0; i < 20; ++i)
                {
                    engine->setCursor (i * 2048);
                    float silent[64] {}, left[64] {}, right[64] {};
                    const float* inputs[] { silent, silent };
                    float* outputs[] { left, right };
                    engine->audioDeviceIOCallbackWithContext (inputs, 2, outputs, 2, 64, {});
                    panel.tick(); paintPanel();
                }
                check (panel.frameLight.masks.builds == f && panel.subLight.masks.builds == s,
                       "v80 production Bass tick/repaint: frame and Sub Gaussian builds unchanged");
            }
        }
        const int fs = panel.frameLight.masks.builds, ss = panel.subLight.masks.builds;
        for (const auto& preset : Theme::presets)
        {
            Theme::applyPreset (preset, false);
            panel.lookAndFeelChanged();
            const auto painted = paintPanel();
            if (preset.name == "Sunset" || preset.name == "Tokyo Night")
                artifact (painted, "Bass-theme-" + preset.name);
        }
        check (panel.frameLight.masks.builds == fs && panel.subLight.masks.builds == ss,
               "v80 live built-in theme tint changes reuse both alpha caches");
        int callbacks = 0;
        panel.onInfo = [&] (const juce::String&) { ++callbacks; };
        sub.setValue (0.25, juce::sendNotificationSync);
        check (engine->bass.sub.load() == 0.25f && callbacks == 1,
               "v80 real Sub setter retains original engine/info callback exactly once");
        sub.setEnabled (false);
        const int beforeDisabledPaint = panel.subLight.masks.builds;
        paintPanel();
        paintPanel();
        check ((panel.lastSubLightState & 1) == 0 && panel.subLight.masks.builds == beforeDisabledPaint + 1,
               "v80 production disable invalidates parent; value builds once, state reuses alpha");
        artifact (paintPanel(), "Bass-disabled-Sub");
        sub.setEnabled (true);
        paintPanel();
        const int beforeSilent = panel.subInvalidations;
        sub.setValue (0.66, juce::dontSendNotification);
        // Exercise the child-only silent setter safety hook, not refreshFromEngine.
        render (38, 38, 1.0f, [&] (juce::Graphics& g) { sub.paint (g); });
        check (panel.subInvalidations == beforeSilent + 1 && engine->bass.sub.load() == 0.25f,
               "v80 arbitrary silent child setter schedules parent halo refresh without invoking audio callback");
        auto partial = paintPanel();
        sub.setValue (0.0, juce::dontSendNotification);
        render (38, 38, 1.0f, [&] (juce::Graphics& g) { sub.paint (g); });
        {
            juce::Graphics g (partial);
            g.reduceClipRegion (panel.lastSubInvalidation);
            panel.paintEntireComponent (g, true);
        }
        const auto clean = paintPanel();
        bool noStaleHalo = true;
        for (int y = 0; y < clean.getHeight(); ++y)
            for (int x = 0; x < clean.getWidth(); ++x)
                noStaleHalo &= clean.getPixelAt (x, y) == partial.getPixelAt (x, y);
        check (noStaleHalo && engine->bass.sub.load() == 0.25f,
               "v81 actual silent nonzero-to-zero clipped production repaint equals clean full paint; no stale halo");
        const auto oldArea = panel.lastSubLightBounds;
        sub.setTopLeftPosition (sub.getX() + 2, sub.getY());
        render (38, 38, 1.0f, [&] (juce::Graphics& g) { sub.paint (g); });
        check (panel.lastSubInvalidation.contains (oldArea) && panel.lastSubInvalidation.contains (panel.lastSubLightBounds),
               "v80 moved source invalidation contains old and new parent extents");
        panel.setLookAndFeel (nullptr);
        Theme::applyPreset (saved, legacy);
    }
};
