---
name: juce-custom-lookandfeel
description: Implement approved RiffCatch JUCE custom painting and LookAndFeel changes with theme support, safe lifetimes and preserved audio/UI behavior.
---

# JUCE Custom LookAndFeel

1. Read `AGENTS.md`, the approved specification and `docs/design/design-system.md`. Inspect `Source/ui/Theme.h`, affected components and their event bindings before editing.
2. Keep common control rendering in `RiffLookAndFeel`/existing `Theme` helpers. Keep component-specific content in its component; do not introduce a parallel skin framework or duplicate drawing code.
3. Reuse theme colors, radii, dark control frames, well rings and cached card shadows. Preserve all themes and `Seg` display styling. New color roles belong centrally, not in component literals.
4. For approved Neo-Retro work, layer shadows/base surfaces, bevels, directional reflections and crisp highlights deliberately. Ensure later fills/outlines do not cover the intended blue top-edge reflection. Clip effects to rounded geometry and reserve bounds for shadows.
5. Check local/parent coordinates, viewport offsets, DPI, small sizes, text bounds and hit areas. Use `Graphics::ScopedSaveState` for local clipping/transforms. Do not let decorative layers intercept input.
6. Maintain LookAndFeel lifetime through component destruction. Do not reset slider values or trigger callbacks during restyling. Preserve tooltips, focus, keyboard shortcuts, ranges, defaults, undo and accessibility behavior.
7. Keep expensive rasterization/blur out of repeated repaint paths where possible; reuse bounded caches and invalidate for relevant size/theme/scale changes. No drawing or new allocations/locks in the audio callback. Do not change DSP for a visual task.
8. Bump the app version only for an app-code iteration. Configure/build Release using existing CMake commands, run a fresh selftest, and check interactions and screenshots per `docs/design/visual-checklist.md`.

Report changed files, build/selftest evidence, screenshot paths, remaining gaps and manual audio checks for the user. Do not commit or push without explicit approval; specialist implementation is not Git authorization.
