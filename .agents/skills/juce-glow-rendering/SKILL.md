---
name: juce-glow-rendering
description: "Implement and review premium Neo-Retro lighting in JUCE/C++: outer glow, bloom, inner glow, Gaussian blur, layered transparency, bevels, specular highlights and dodge/burn. Use for illuminated controls, panel ribbons, knobs and recessed displays, including clipping, caching, DPI and visual-quality debugging."
---

# JUCE Glow Rendering

## Purpose and boundaries

Use this skill to design, implement or diagnose hardware-style light on dark
metal: soft orange and gold emitters, cool blue reflections, crisp highlights,
deep recesses and restrained shadows. A bright outline alone is not glow.
JUCE can render these effects; investigate masks, compositing, clipping and
contrast before blaming its renderer.

Loading this skill does not authorize source edits, extra effects or a redesign.
For documentation-only work, do not build or modify the application.
Honor the requested scope, existing visual decisions and tool permissions.
Read-only agents specify or review; they do not capture windows or implement.

In RiffCatch, first read:

- `AGENTS.md`
- `docs/design/design-system.md`
- `docs/design/visual-checklist.md`
- Relevant `Source/ui/Theme.h` helpers, component paint/layout and event bindings.

Open `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png` and real current
screenshots for visual work. Treat reference observations, proposed values and
verified results separately. These techniques are reusable recipes, not changes
to the approved design system. In particular, do not restore a removed reflection
or replace an explicitly solid ribbon with a diffuse one without approval.

Outside RiffCatch, use the host project's equivalent rules, palette and component
architecture. Do not require RiffCatch classes for the generic techniques below.

## Start with geometry and a light budget

1. Identify the receiver surface, the emitting shape and the light direction.
   Separate the source silhouette from the component rectangle and hit area.
2. Specify three scales: a crisp source/core, a narrower colored halo and a
   broader, dimmer bloom. Include an adjacent dark bevel/contact edge.
3. Decide which light is reflected, rather than emitted. Metal highlights should
   be directional; a value arc emits only along its actual value.
4. Assign each layer an owner, clip, draw order and cache invalidation rule.
5. Compare at native viewing size before magnifying corners or measuring pixels.

Starting points at 100% scale, not fixed tokens or measured reference values:

| Element | Core | Near halo | Broad bloom |
| --- | --- | --- | --- |
| Panel/tab ribbon | 1-3 logical px | 2-5 px, alpha 0.20-0.45 | 6-12 px, alpha 0.05-0.18 |
| Colored knob arc | Existing arc width | 1-3 px beyond arc, alpha 0.15-0.30 | 3-6 px, alpha 0.04-0.12 |
| Small metal button | Thin exposed bevel | Local reflection, not an emissive border | Usually none |

These are perceptual extents, not interchangeable blur-radius or sigma values.
Shrink effects on small controls. Keep semantic red/green states and pointers
more legible than decorative light. Preserve the original color in the halo;
use a brighter, less saturated core sparingly, not a white wash.

## Multi-layer rendering order

For a raised, illuminated control:

1. Parent-owned outer shadow, outside the control silhouette.
2. Outer glow/bloom that spills onto the surrounding receiver.
3. Opaque control base, covering unwanted glow inside the body.
4. Dark lower/contact bevel and upper diffuse face shading.
5. Inner reflected light clipped to the face, if specified.
6. Colored source band or value arc, then its small bright core.
7. Local specular highlights.
8. Text, icons, pointers and state indicators, unobscured.

For a recessed well, keep the interior dark, place a shadow at the inner upper
lip and a weaker reflected highlight at the opposite lip. Do not paint a glow
film over grid cells, waveform data or tuning scales.

Draw a panel's receiver glow before its children; draw the source on top of
the receiver. If the receiver is the panel face itself, its opaque fill must
precede that glow. A correct blur drawn below a later opaque fill is invisible.

## Outer Glow and Bloom

**Outer glow** is colored light spreading beyond an emitting silhouette.
**Bloom** is a broader, lower-frequency spread from the brightest sources.
For this UI, build bloom from explicit emissive masks rather than blurring the
entire interface: otherwise text, white pointers and meters all become emitters.

Practical mask pipeline:

1. Fill the emitter into a transparent, padded image. Use a stroked path converted
   to a filled path for a ribbon or arc; do not fill the whole control rectangle.
2. Blur separate copies at a small and larger radius.
3. Tint the blurred alpha with the source's live Theme color.
4. Composite the broad layer first, near layer second, original source last.
5. Cover or exclude the body interior when the effect should be outer-only.

Use a continuous mask for a long ribbon. A corner-centered radial gradient
cannot maintain emission along an entire vertical edge. If a solid 4px ribbon
is specified, keep that source opaque; any permitted halo is a separate layer.

JUCE's ordinary Graphics drawing uses alpha compositing, not HDR light transport.
Layered source-over can produce convincing UI illumination but is not physically
additive bloom. If true additive/screen compositing is required, implement an
explicit image/shader operation with documented color space and clamping; do not
invent `Graphics::setBlendMode`, or introduce OpenGL just to add a soft edge.

### Padded Gaussian mask example

This JUCE 8.0.4 example builds a single reusable mask. Call it only on cache
invalidation, not for every knob repaint. The emitter is a non-empty **filled**
path in the paint owner's logical coordinates; callers provide finite positive
scale/radius values and a bounded image size. Apply the project's error policy
for invalid runtime inputs; the assertions below document example preconditions.

```cpp
#include <juce_graphics/juce_graphics.h>
#include <cmath>

struct SoftLightMask
{
    juce::Image alpha;
    juce::Point<float> origin;
    float pixelsPerUnit = 1.0f;
};

SoftLightMask buildSoftLight (const juce::Path& emitter,
                             float pixelsPerUnit,
                             float blurRadius)
{
    jassert (! emitter.getBounds().isEmpty());
    jassert (pixelsPerUnit > 0.0f && blurRadius > 0.0f);
    const float radiusPx = blurRadius * pixelsPerUnit;
    const float padPx = std::ceil (3.0f * radiusPx) + 2.0f;
    const auto padded = emitter.getBounds().expanded (padPx / pixelsPerUnit);
    const int w = juce::jmax (1, (int) std::ceil (padded.getWidth() * pixelsPerUnit));
    const int h = juce::jmax (1, (int) std::ceil (padded.getHeight() * pixelsPerUnit));
    juce::Image source (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    {
        juce::Graphics maskGraphics (source);
        maskGraphics.addTransform (
            juce::AffineTransform::translation (-padded.getX(), -padded.getY())
                .scaled (pixelsPerUnit));
        maskGraphics.setColour (juce::Colours::white);
        maskGraphics.fillPath (emitter);
    }
    juce::Image blurred;
    juce::ImageEffects::applyGaussianBlurEffect (radiusPx, source, blurred);
    return { blurred, padded.getPosition(), pixelsPerUnit };
}

void drawSoftLight (juce::Graphics& g, const SoftLightMask& light,
                   juce::Colour colour)
{
    juce::Graphics::ScopedSaveState state (g);
    g.setColour (colour);
    g.drawImageTransformed (
        light.alpha,
        juce::AffineTransform::scale (1.0f / light.pixelsPerUnit)
            .translated (light.origin.x, light.origin.y),
        true); // Use image alpha as the mask for the current colour.
}
```

Build near and broad masks from the **same emitter**, then use, for example,
`sourceColour.withAlpha (0.10f)` for the broad layer and `0.28f` for the near
layer before drawing the unblurred source. Padding here is conservative, not
a promise that JUCE's radius is Gaussian sigma. Check the actual nonzero-alpha
support of the installed backend; increase padding if the outermost pixels
are still visible. This image does not grant permission to paint outside an
ancestor's clip: the paint owner also needs room.

`drawImageTransformed(..., true)` uses the current brush with the image alpha.
Using `false` on the white mask paints white light instead of the intended hue.
Scope opacity, gradients and transforms so they cannot leak into later text.

## Inner Glow

An inner glow belongs **inside** a face, strongest near its rim and fading toward
the center. Blurring a solid filled face and clipping it back to itself creates
a bright center, not the desired rim.

Two reusable constructions:

- **Boundary method:** create a narrow stroked perimeter mask, blur it, then draw
  it under `Graphics::reduceClipRegion(facePath)`. Modulate intensity by light
  direction. Keep the center free of tint.
- **Mask method:** rasterize face coverage `M` into a padded image. Build a blurred
  version of its complement, then multiply it by `M` pixel by pixel to retain
  only the inner edge. Complement coverage must extend throughout the padded
  image, not stop at the component boundary. Use `Image::BitmapData` for a cached
  build, respecting `lineStride`, `pixelStride` and premultiplied channels.

The second method needs explicit mask arithmetic; JUCE clipping is not a
substitute for every mask operation. Do not claim `GlowEffect` implements an
inner glow. Keep the inset radius positive: a thick centered stroke around a
small rounded rectangle can collapse its inner corner to a square. Prefer
explicit outer/inner rounded paths or a properly clipped mask.

## Gaussian Blur and layered transparency

- `ImageEffects::applyGaussianBlurEffect` applies to all channels and can use
  backend acceleration, with a software fallback. Supply separate source/result
  storage; reuse the result when rebuilding at unchanged dimensions.
- `ImageConvolutionKernel::createGaussianBlur` plus `applyToImage` is the explicit
  kernel alternative. Allocate an odd-sized kernel with adequate support,
  normalize with `setOverallSum (1.0f)` when appropriate, and follow the installed
  header's radius guidance. Do not assume its radius parameter equals sigma.
- `ImageEffects::applySingleChannelBoxBlurEffect` accepts single-channel images
  and uses multiple box blurs. It is an approximation, useful for soft shadows;
  do not label it exact Gaussian filtering.
- Work on white alpha masks and tint at composition time to avoid dark RGB
  fringes. For custom pixel math, keep JUCE ARGB data premultiplied and do not
  multiply alpha twice. Avoid assuming a byte channel order across backends.
- Two or three low-opacity blur scales usually look smoother than many equally
  spaced strokes. Concentric strokes are a cheap approximation for small,
  rapidly changing arcs, but inspect for visible bands and rounded-cap buildup.
- Overlapping translucent layers accumulate nonlinearly; eight layers at 0.05
  are not simply one layer at 0.40. Avoid accidental double attenuation by both
  nested geometry coverage and a gradient that extinguish the same halo.

For a zero-length value arc, emit no halo. Round real arc ends, do not paint a
full glowing ring or a fabricated minimum value. Keep pointers crisp and draw
them after the arc glow.

## Bevel & Emboss, specular highlights, Dodge & Burn

**Bevel & Emboss:** JUCE has no Photoshop-style bevel preset. Construct nested
face/rim paths. A raised face gets an exposed upper-left highlight and a lower-
right dark bevel/contact shadow. A recess reverses the light relationship at
its inner lip. Keep a narrow dark neighbor beside the highlight so it reads as
depth rather than an outline. Geometry must not change the component's hit area.

**Specular highlights:** add small, localized reflections on exposed metal:
an elliptical radial gradient on a knob, or a short light field along a closed
button contour. Fade both ends and the perpendicular extent. Do not leave a
full-alpha open stroke ending just after a corner. For anisotropic light, scale
the light field independently of the contour, or inverse-transform the contour,
so the actual stroke thickness does not become stretched.

**Dodge & Burn:** approximate diffuse dodge with a low-opacity warm/cool light
gradient over the face; burn with a near-black tinted gradient under the lower
edge and contact points. Clip to the receiver, preserve midtone detail and keep
color roles live. These are controlled source-over shading layers, not a claim
that JUCE implements Photoshop's Color Dodge/Color Burn blend equations.

Separate these mechanisms: a specular spot is not a broad glow, an inner bevel
is not an outside shadow, and simply increasing brightness is not material
depth. Use one consistent key-light direction across neighboring controls.
Pressed states reduce height/shadow/reflection; disabled states reduce emission
without making values unreadable. Preserve existing focus and toggle semantics.

## JUCE API choices and lifetimes

Verified against RiffCatch's JUCE 8.0.4 headers; inspect the installed version
before transplanting code into another project.

| API | Use and limitation |
| --- | --- |
| `Graphics`, `ColourGradient` | Linear/radial face shading, falloff and tinted mask composition. |
| `Graphics::ScopedSaveState`, `reduceClipRegion`, `addTransform` | Local clips/transforms; cannot expand an inherited clip. |
| `Path`, `PathStrokeType::createStrokedPath` | Filled emitter silhouettes, even-odd rings and round-ended arcs. |
| `Image`, `Image::BitmapData`, `SoftwareImageType` | Padded caches and controlled mask arithmetic; raster access may synchronize native images. |
| `ImageEffects` | Gaussian and single-channel box blur described above; acceleration is not guaranteed. |
| `ImageConvolutionKernel` | Explicit convolution; large kernels are expensive. |
| `GlowEffect::setGlowProperties` | Whole-component colored blur; includes content, not just selected trim. Not an inner-glow/HDR-bloom API. |
| `DropShadow::drawForPath/drawForImage/drawForRectangle` | Convenient shadow primitives; rectangle uses a gradient approximation. |
| `DropShadowEffect` | Whole-component shadow including children; fast approximation, not a high-quality Gaussian guarantee. |
| `ImageEffectFilter` | Custom complete effect pipeline via `applyEffect(Image&, Graphics&, float, float)`. |
| `Component::setComponentEffect` | One effect pointer, not an automatic effect stack; component does not own it. |
| `Component::setBufferedToImage`, `CachedComponentImage` | Component caching; child repaints invalidate it, so it does not make a busy whole-panel blur free. |

A broad component effect can also glow text, pointers and child displays.
Prefer explicit masks for individual hardware details. If using a component
effect, keep transparent pixels where needed, do not falsely mark the component
opaque, and own the effect longer than its attachment. Detach with
`setComponentEffect (nullptr)` before destroying the effect. Keep LookAndFeel
ownership safe as well. In JUCE 8.0.4, `GlowEffect` applies its offset to both the
blurred result and source drawing; an offset is not merely a light displacement.

## Performance, caching and repaint ownership

- Keep all rendering and UI cache changes off the audio callback. No new audio-
  thread allocation, blur, file I/O, locking or UI access.
- Cache static enclosure masks/blur layers separately from dynamic playheads,
  meter bars, text and value arcs. Do not blur an entire large panel at 30/60 Hz
  because one number changed.
- Cache keys include geometry/size, physical scale, blur radius, emitter shape
  and light placement. A tinted/composited cache also includes theme/state.
  A reusable alpha-only mask can survive color changes; repaint with new tint.
- Rebuild on relevant `resized()`, LookAndFeel/theme or physical-scale changes.
  Do not assume logical size changes when moving between different-DPI monitors.
- Bound cache memory and replacement. Do not keep a cache entry for every float
  knob value. For moving arcs use a few cheap stroke layers, or a measured,
  bounded quantization strategy only if it produces no visible stepping.
- Blur broad layers at lower resolution only after checking upsampling artifacts.
  Keep the sharp core and text at native resolution.
- Measure cache misses, image dimensions, blur/paint cost and allocation behavior
  during resize, theme switching and live playback. CPU/GPU backend choice is a
  profiling result, not a reason to add a rendering dependency speculatively.
- Repaint the union of old/new effect extents when a source moves. A child-only
  repaint cannot clean a stale parent-owned shadow outside that child's bounds.

## Avoid clipping and unwanted artifacts

Parent-owned outer effects must be painted before children and within the
parent's available area. Convert descendant geometry with `getLocalArea`;
viewport children are not already in parent coordinates. Do not enlarge the
interactive control just to reserve halo space. A transparent decorative layer,
if truly necessary, must not intercept mouse input.

| Symptom | Check and correction |
| --- | --- |
| Glow invisible except a hard line | Inspect mask alpha, opaque overpainting, inherited clip, tint alpha and double falloff. |
| Rectangular halo cutoff | Pad the image, then ensure the paint owner/ancestors also permit that extent. |
| Square inner corner | Avoid stroke width consuming the corner radius; build valid inner/outer contours. |
| White highlight stops abruptly | Fade along a closed contour or taper an open segment to zero before its endpoint. |
| Muddy/black fringe | Check premultiplication, mask tinting, transparent padding and resampling. |
| Colored area looks flat/washed out | Restore dark contact separation; reduce broad wash, preserve a saturated halo and smaller bright core. |
| Visible rings/banding | Reduce stroke layering, improve continuous falloff or use a cached Gaussian mask. |
| Uniform neon box | Limit emission to the intended edge; quiet opposite edges and avoid glowing text. |
| Soft or shifted controls at high DPI | Scale masks once, retain logical origin and draw back at inverse scale; inspect device-pixel alignment. |
| UI jumps when switching panels | Compare actual parent-coordinate bounds, header budget and paint insets; lighting cannot fix layout. |

Use debug views of source alpha, each blur layer, clips and the final composite.
Destroy temporary `Graphics`/bitmap write views before reading back image pixels.
Do not judge native/GPU raster results before writes have been completed.

## RiffCatch integration

- Put shared material/color helpers in `Source/ui/Theme.h`; explicitly opt in
  relevant controls rather than changing every LookAndFeel path.
- Keep functional content in its existing component. Layout stays in
  `resized()`/the shared layout helpers; decorative paint must not affect
  ranges, defaults, callbacks, undo, recording, Catch, sequencing or persistence.
- `Theme::accent` is Primary, `Theme::purple` is Secondary and `Theme::cyan` is
  preset Accent. None is a universal literal blue. Use named live roles for
  cool metal reflections; preserve `SegmentDisplay.h`/`LibraryCounter.h` blue
  digits and existing themes/defaults.
- Reuse `framedTile`, `wellRing`, `cardShadow`, radii and existing material
  helpers where appropriate. Preserve their unmodified callers when adding
  local effects. A technique in this skill is not permission to restyle them.
- Use the separate `juce-custom-lookandfeel` and `visual-quality-review`
  instruction documents when relevant; do not duplicate the design system here.

## Visual quality gate

Report each relevant item as PASS, CHANGES REQUIRED or BLOCKED with evidence:

- At native size, the source has a crisp core, visible soft colored falloff and
  darker adjacent material. Punch comes from this hierarchy, not uniform white.
- Emission remains present along the intended ribbon/arc; the broad tail fades
  smoothly to the receiver with no hard end, rectangle, halo gap or corner spike.
- Knob pointers, text, cell states, waveform detail and semantic colors remain
  readable. An empty screenshot does not verify a populated or active state.
- Raised controls and recessed wells have consistent light direction. Normal,
  hover, pressed, toggled, disabled and keyboard-focused states stay distinct.
- Bounds and hit areas are unchanged unless explicitly approved. Check real
  component coordinates as well as perceived edges.
- Inspect normal/minimum sizes, actual DPI, theme changes and available alternate
  DPI. In RiffCatch include Neo-Retro, Sunset and Tokyo Night; shared changes need
  the wider theme matrix from the project checklist. State any untested cases.
- Compare actual final EXE screenshots to reference and baseline. Use crops at
  native size and magnified edges; pixel contrast/falloff measurements support,
  but never replace, visual judgment.
- Measure a cross-section outside the emitter to verify a real halo, not merely
  a brighter source. Check alpha at cache borders and neighboring-pixel jumps;
  choose thresholds from the approved visual target, not to bless existing output.
- In RiffCatch capture only the verified application HWND using `PrintWindow`,
  never the desktop. Preserve settings/autosaves, verify foreground before input
  and distinguish real captures from synthetic paint-test images.
- For application-code changes, follow `AGENTS.md` for version, Windows Release,
  fresh selftest and user testing. Building this documentation alone needs none.
  Missing capture/input support is a coverage blocker, not a rendering pass.

Deliver the scoped result, exact evidence and remaining manual checks.
Never infer user acceptance, commit permission or push permission from a skill.

## References and discovery

- [OpenCode SKILL.md format and search paths](https://opencode.ai/docs/skills/)
- [Copilot skill format and project locations](https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/customize-cloud-agent/add-skills)
- [JUCE Graphics](https://docs.juce.com/master/classGraphics.html)
- [JUCE ImageEffects](https://docs.juce.com/master/structImageEffects.html)
- [JUCE ImageConvolutionKernel](https://docs.juce.com/master/classImageConvolutionKernel.html)
- [JUCE GlowEffect](https://docs.juce.com/master/classGlowEffect.html)
- [JUCE DropShadowEffect](https://docs.juce.com/master/classDropShadowEffect.html)

The online JUCE API is a moving reference; the installed headers are authoritative.
This skill lives at `.agents/skills/juce-glow-rendering/SKILL.md`, a documented
project discovery location for both OpenCode and GitHub Copilot. Keep the folder
name and YAML `name` identical, and do not install a second same-name copy.
From the repository root, use `opencode debug skill` to verify native parsing
and discovery. In a fresh Copilot session, request
`Load juce-glow-rendering and explain the padded-mask recipe without editing.`
Verify that the skill is actually loaded; a file's existence is not proof of
discovery by a running session.
