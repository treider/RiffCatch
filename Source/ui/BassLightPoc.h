#pragma once
#include "Theme.h"

// Deliberately local v0.80/81 prototype: two current snapshots, not a renderer/service
// or a cache indexed by every value. UI/message thread only. Alpha survives tint changes.
namespace BassLightPoc
{
    inline float smooth (float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return t * t * (3.0f - 2.0f * t);
    }

    struct Masks
    {
        enum class Status { unbuilt, ready, paddingFailure };
        Status status = Status::unbuilt;
        std::array<juce::Image, 3> layers;
        juce::Point<float> origin;
        float scale = 1.0f;
        int builds = 0, gaussianCalls = 0, imageAllocations = 0, borderAlpha = 0;
        double lastBuildMs = 0.0;

        template <typename Emitter, typename Receiver>
        bool build (juce::Rectangle<float> bounds, float physicalScale,
                    std::array<float, 3> radii, Emitter emitter, Receiver receiver)
        {
            jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
            const auto started = juce::Time::getMillisecondCounterHiRes();
            status = Status::unbuilt;
            scale = physicalScale;
            int pad = (int) std::ceil (3.0f * radii[0] * scale) + 2;
            // Validate actual support, including backend variation; never accept a cut mask.
            for (int attempt = 0; attempt < 3; ++attempt, pad *= 2)
            {
                origin = { std::floor (bounds.getX() * scale - pad) / scale,
                           std::floor (bounds.getY() * scale - pad) / scale };
                const int w = (int) std::ceil ((bounds.getRight() - origin.x) * scale) + pad;
                const int h = (int) std::ceil ((bounds.getBottom() - origin.y) * scale) + pad;
                juce::Image source (juce::Image::ARGB, w, h, true, juce::NativeImageType());
                ++imageAllocations;
                {
                    juce::Graphics mask (source);
                    mask.addTransform (juce::AffineTransform::translation (-origin.x, -origin.y).scaled (scale));
                    emitter (mask);
                }
                borderAlpha = 0;
                for (int i = 0; i < 3; ++i)
                {
                    layers[(size_t) i] = {};
                    juce::ImageEffects::applyGaussianBlurEffect (radii[(size_t) i] * scale, source, layers[(size_t) i]);
                    ++gaussianCalls; ++imageAllocations; // owned source/results, not backend-internal allocations
                    // Native blur is complete before CPU alpha inspection/modulation.
                    auto image = layers[(size_t) i].convertedToFormat (juce::Image::ARGB);
                    juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x)
                        {
                            auto pixel = pixels.getPixelColour (x, y);
                            if (x == 0 || y == 0 || x == w - 1 || y == h - 1)
                                borderAlpha = juce::jmax (borderAlpha, (int) pixel.getAlpha());
                            const float gain = receiver (origin.x + (x + 0.5f) / scale,
                                                         origin.y + (y + 0.5f) / scale);
                            if (gain < 1.0f)
                                pixels.setPixelColour (x, y, pixel.withMultipliedAlpha (gain));
                        }
                    layers[(size_t) i] = image;
                }
                if (borderAlpha <= 1)
                {
                    status = Status::ready;
                    break;
                }
                if (attempt == 2) // backend failure: omit light rather than display a cut edge or allocate unboundedly
                {
                    status = Status::paddingFailure;
                    juce::Logger::writeToLog ("BassLightPoc: mask padding failed after 3 attempts; border alpha="
                                             + juce::String (borderAlpha) + ", image="
                                             + juce::String (w) + "x" + juce::String (h));
                    jassertfalse;
                    layers = {};
                }
            }
            ++builds;
            lastBuildMs = juce::Time::getMillisecondCounterHiRes() - started;
            return status == Status::ready;
        }

        void draw (juce::Graphics& g, juce::Colour tint, std::array<float, 3> alpha) const
        {
            if (status != Status::ready) return;
            juce::Graphics::ScopedSaveState save (g);
            const auto transform = juce::AffineTransform::scale (1.0f / scale).translated (origin.x, origin.y);
            for (int i = 0; i < 3; ++i) // broad -> medium -> tight; sharp source belongs to original owner
            {
                g.setColour (tint.withMultipliedAlpha (alpha[(size_t) i]));
                g.drawImageTransformed (layers[(size_t) i], transform, true);
            }
        }
    };

    struct Frame
    {
        Masks masks;
        juce::Rectangle<float> bounds;
        float scale = 0.0f;
        void ensure (juce::Rectangle<float> b, float s)
        {
            if (bounds == b && scale == s) return;
            bounds = b; scale = s;
            const auto edge = b.reduced (1.0f);
            // JUCE Windows native radius = 3 sigma, not sigma. Software fallback
            // also remains bounded; production tests measure the actual result.
            masks.build (b, s, { 16.5f, 7.5f, 2.1f }, [&] (juce::Graphics& g)
            {
                juce::Path shape;
                shape.addRoundedRectangle (edge, Theme::radiusLg);
                g.reduceClipRegion (shape);
                juce::Path contour;
                contour.addRoundedRectangle (edge.reduced (0.75f), Theme::radiusLg - 0.75f);
                juce::ColourGradient weight (juce::Colours::white.withAlpha (0.7f), edge.getX(), edge.getY(),
                    juce::Colours::white.withAlpha (0.06f), edge.getRight(), edge.getBottom(), false);
                g.setGradientFill (weight);
                g.strokePath (contour, juce::PathStrokeType (1.0f));
                g.setColour (juce::Colours::white);
                g.fillPath (Theme::bassStripePath (edge)); // exact 4px source, not the panel face
            }, [=] (float x, float y)
            {
                // No emission into either tab's transparent corners or their gap.
                // Fade near the joined top-left corner instead of cutting a blur at y=0.
                const float pastTabs = smooth ((x - (b.getX() + 258.0f)) / 10.0f);
                return pastTabs + (1.0f - pastTabs) * smooth ((y - edge.getY()) / 7.0f);
            });
        }
        void drawOuter (juce::Graphics& g) const
        {
            masks.draw (g, Theme::bassLeftRibbon(), { 0.65f, 0.50f, 0.34f });
        }
        void drawInner (juce::Graphics& g) const
        {
            juce::Graphics::ScopedSaveState save (g);
            const auto edge = bounds.reduced (1.0f);
            g.reduceClipRegion (Theme::bassEnclosureBand (edge, 3.0f, false));
            // Solid stripe is not tinted; nor is functional content or the recessed face.
            g.excludeClipRegion (juce::Rectangle<float> (edge.getX(), edge.getY(), 4.0f, edge.getHeight()).getSmallestIntegerContainer());
            masks.draw (g, Theme::bassFocusLight(), { 0.0f, 0.14f, 0.22f });
        }
    };

    struct Sub
    {
        static constexpr int repaintOutset = 16; // includes broad support outside the unchanged 38px slider
        Masks masks;
        juce::Rectangle<float> bounds;
        float proportion = -1.0f, scale = 0.0f, start = 0.0f, end = 0.0f;
        bool uni = true;
        Theme::BassSubGeometry geometry { {}, 0.0f, {} };
        void ensure (juce::Rectangle<float> b, float p, float s, float a, float z, bool unipolar)
        {
            if (bounds == b && proportion == p && scale == s && start == a && end == z && uni == unipolar) return;
            bounds = b; proportion = p; scale = s; start = a; end = z; uni = unipolar;
            geometry = Theme::BassSubGeometry::make (b, p, a, z, uni);
            masks.build (b, s, { 12.0f, 7.5f, 2.0f }, [&] (juce::Graphics& g)
            {
                g.setColour (juce::Colours::white);
                g.strokePath (geometry.valueArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved,
                                                                    juce::PathStrokeType::rounded));
            }, [=] (float, float y)
            {
                // Caption starts at slider.bottom, only 4px below ring: extinguish
                // smoothly before it, rather than painting text on a golden haze.
                return 1.0f - smooth ((y - (b.getBottom() - 5.0f)) / 4.0f);
            });
        }
        static float strength (bool enabled, bool over, bool down)
        {
            return ! enabled ? 0.28f : down ? 0.60f : over ? 1.18f : 1.0f;
        }
        void drawRing (juce::Graphics& g, bool enabled, bool down) const
        {
            // Only the ring/shadow base moves to the parent. Otherwise the child's
            // opaque radius15 disk buries the near Gaussian between arc and receiver.
            // Face, bevels, track, arc and pointer remain sharp in the original LAF.
            const auto c = geometry.centre;
            const float r = geometry.radius, gain = ! enabled ? 0.35f : down ? 0.5f : 1.0f;
            for (int layer = 3; layer >= 0; --layer)
            {
                const float spread = layer * 0.6f;
                g.setColour (Theme::bassShadow().withAlpha (0.055f * gain));
                g.fillEllipse (c.x - r - spread, c.y - r * 0.84f + 1.5f - spread,
                               2.0f * (r + spread), 2.0f * (r * 0.84f + spread));
            }
            g.setColour (Theme::frame);
            g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        }
        void draw (juce::Graphics& g, juce::Colour tint, bool enabled, bool over, bool down) const
        {
            const float gain = strength (enabled, over, down);
            // Multiply layer opacity, not the opaque tint (which would clamp hover to1).
            masks.draw (g, tint, { 0.72f * gain, 0.65f * gain, 0.44f * gain });
        }
    };

    // Only Sub installs the hook. No parameter/callback interception; base input
    // implementations remain authoritative. Paint catches arbitrary silent setters.
    struct Slider : juce::Slider
    {
        std::function<void()> lightChanged;
        void notifyLight() { if (lightChanged) lightChanged(); }
        void valueChanged() override { juce::Slider::valueChanged(); notifyLight(); }
        void paint (juce::Graphics& g) override { notifyLight(); juce::Slider::paint (g); }
        void resized() override { juce::Slider::resized(); notifyLight(); }
        void enablementChanged() override { juce::Slider::enablementChanged(); notifyLight(); }
        void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); notifyLight(); }
        void mouseExit (const juce::MouseEvent& e) override { juce::Slider::mouseExit (e); notifyLight(); }
        void mouseDown (const juce::MouseEvent& e) override { juce::Slider::mouseDown (e); notifyLight(); }
        void mouseUp (const juce::MouseEvent& e) override { juce::Slider::mouseUp (e); notifyLight(); }
    };
}
