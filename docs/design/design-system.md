# RiffCatch Design System

## Authority and status

The user-approved redesign target is [RiffCatch Retro-Tech DAW Interface](../screenshots/RiffCatch%20Retro-Tech%20DAW%20Interface.png). Open the image, not just this description, before visual design/review. If it is unavailable, ask the user to provide it. Existing app screenshots are baseline evidence, not replacement targets.

This document is a specification, not a claim that the target has shipped. At framework setup, `main` is v0.63: mostly flat themed controls, dark frames, lighter well rings, cached card shadows and blue seven-segment displays. There are nine built-in themes with Sunset as default. The discarded redesign is not an approved baseline. No GUI changes are authorized by installing this framework.

## Visual language

Neo-Retro audio hardware: dark navy/charcoal materials, tactile controls, recessed black working areas, warm orange/gold emphasis and cool blue illumination. Preserve the compact DAW layout and all current operations. This is not a generic flat dashboard and not a uniform neon outline treatment.

The reference's depth comes from deliberate light/dark contrast (Photoshop-style dodge and burn): crisp highlights on exposed edges, small reflected highlights below them, softer light on faces, dark bevel separation and contact shadows. Use consistent lighting rather than merely making every panel brighter.

## Reference observations

| Area | Visible target and acceptance intent |
| --- | --- |
| Toolbar | Rounded dark enclosure, pale-blue upper rim, localized bright glints and faint reflections below the top edge. Cool blue light across the upper surface, warmer light toward the lower-left edge. Do not flatten this into a single orange border or evenly spaced blue stripes. |
| Controls | Raised dark buttons inside deep frames, bright upper bevels and darker lower/contact edges. Blue tool accents; pink/red record, green play and orange loop remain semantically distinct. |
| Displays | Blue luminous seven-segment digits in dark blue recessed housings, bright bevels and controlled glow. Preserve the existing clock and drum/bass counter identity. |
| Tracks | Orange first track and warm yellow second track; localized colored edge light and bright name plates. Black timeline wells with layered light rims and dark inner edges, readable grid and content. |
| Knobs/faders | Dark metallic knobs with small highlights, shadows, readable pointers and colored value arcs. Fader caps stand apart from dark slots and scales. |
| Tabs/sequencer | Active tabs catch orange top light. The shown Bass panel has a warm rim, dark controls and a recessed piano roll with restrained row/grid contrast. Drums is not expanded in this image: apply shared principles without inventing hidden details. |
| Tuner/master | Inset dark tuner with cool edge reflections and legible scale; master has a warm title area, recessed meter/fader and clear numeric readout. |
| Workspace | Dark navy/black background with subtle texture, broad diagonal curved bands and localized warm light near the sides/bottom. Decorative detail remains behind and quieter than controls. |

## Color and typography

- Treat orange/gold as primary emphasis, blue as display/tool/reflection accent, charcoal as material and near-black as recess. Preserve record/play/warning semantics rather than forcing everything into the accent color.
- Exact target hex values, font family and effect dimensions are not measured here. Do not promote values from the discarded attempt to approved tokens. Sample or propose them explicitly during a scoped implementation.
- Route styling through `Theme` roles and existing `RiffLookAndFeel`; preserve all built-in/custom theme behavior. Current names are historical: `Theme::accent` maps to Primary, `Theme::cyan` to the preset Accent and `Theme::purple` to Secondary, not necessarily those literal hues.
- Keep strong transport/value hierarchy, readable small labels, stable numeric alignment and consistent icon strokes. Do not reproduce truncated mockup labels at the expense of usability.

## Material construction

- Reuse `Theme::framedTile`, `Theme::wellRing`, `Theme::cardShadow`, `Theme::radius` and `Theme::radiusLg` as implementation starting points, not as evidence that the target is already achieved.
- Typical paint order: outer/contact shadow, base surface, inset/bevel shading, localized reflected light, crisp exposed-edge highlight, content. Check actual compositing so later fills or borders do not erase highlights.
- Bright edges need dark neighbors to read as depth. Glows should taper and vary with position; avoid broad translucent rectangles, clipped corners, banding and blown-out labels.
- Normal, hover, pressed, toggled, disabled and keyboard-focused states must be distinguishable. A static reference does not specify all states; implement and verify them deliberately without altering behavior.
- Keep effects bounded and efficient during playback. Avoid repeated expensive blur work and new UI/DSP coupling.

## Scope and acceptance

For unrelated changes preserve the current visual language. For approved redesign tasks, this target supersedes the historical flat/no-gradient restriction. Implement one coherent scope at a time; do not remove features, change theme defaults or modify sound to match a picture.

Completion requires the [visual checklist](visual-checklist.md), a successful build/selftest for code changes, actual screenshot comparison and user testing. Missing visual evidence means incomplete verification. Commit and push remain separate user-approved steps.
