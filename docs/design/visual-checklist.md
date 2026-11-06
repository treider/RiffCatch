# Visual Verification Checklist

Use for every visual change. Mark each item PASS, FAIL, BLOCKED or N/A with a reason; do not silently skip missing coverage.

## Evidence and capture

- [ ] Open the reference at `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png` and the actual before/after screenshots. Do not substitute an earlier redesign attempt.
- [ ] Record build/version, changed scope, screenshot paths, theme, window dimensions, Windows DPI/scaling and application state. Compare like-for-like views.
- [ ] Configure/build Release successfully and run a fresh selftest ending in `ALL OK`. Confirm screenshots are from the newly built EXE, not a stale process.
- [ ] Capture only the RiffCatch window, using Win32 `PrintWindow` on its verified nonzero `MainWindowHandle`. The primary agent/user performs capture, not the read-only reviewer. No full-desktop capture.
- [ ] Place the window with `SetWindowPos` at (20,10,1500,980) for the standard comparison when the display permits. Also test the current minimum size (1100x620 in `Source/Main.cpp`); record any DPI/work-area limitation.
- [ ] Save temporary images under `%TEMP%`, wait for completion, then read and inspect them. Reject blank, stale or incomplete images. Preserve full-window originals alongside detail crops. No image generation or enhancement as evidence of the real app.
- [ ] Ensure RiffCatch has focus before clicks/`SendKeys`. Never send commands to other apps. Preserve unsaved work, settings and autosaves; close normally after testing. Request user screenshots if capture is unavailable.

## Reference fidelity

- [ ] Toolbar has pale-blue exposed top-edge light, localized glints and small reflections below, with darker bevel separation. Neither a flat outline nor a rectangular blue wash substitutes for depth.
- [ ] Raised controls and recessed wells are clearly distinct at normal viewing size. Shadows, highlights and corner treatments share a consistent light direction.
- [ ] Orange/gold rim light is localized; blue displays/tool accents remain clear. Glow does not swamp text, grid lines, icons or control states.
- [ ] Knobs, pointers, value arcs, meter/fader scales and seven-segment digits remain legible.
- [ ] Workspace texture/bands stay subordinate to functional content. No unintended gradient banding, clipped glow or hard rectangular reflection boundaries.

## Layout and interaction

- [ ] Compare full window and toolbar/track/sequencer/tuner detail crops at native size. Check minimum size and available alternate DPI; report unsupported test conditions.
- [ ] Inspect Drums and Bass, pattern/song views, empty and populated tracks, scrolling/zoom and tuner/master. For a shared helper change, test all affected component families.
- [ ] Check all built-in themes and a custom theme when changing shared styling; keep existing default and saved selection. State clearly if coverage is narrower.
- [ ] Inspect normal, hover, pressed, toggled, disabled and keyboard-focus states, including toolbar actions enabled by a selection.
- [ ] No lost controls, clipped labels, overlapping content, broken focus, changed hit areas, tooltip regressions or unreadable numeric values.
- [ ] Verify affected shortcuts, value edits, undo/redo, transport, record/loop/Catch, sequencer and save/export actions. Record manual audio/hardware checks left for the user; screenshots cannot verify sound.

## Handoff

- [ ] Report findings by severity with screenshot path/region and expected versus observed appearance. Use PASS, CHANGES REQUIRED or BLOCKED for the reviewed scope.
- [ ] Give the user the EXE path and remaining test items. Do not claim user acceptance from automated review.
- [ ] Obtain explicit approval before committing visual work; obtain push approval separately. Do not publish temporary screenshots or private session data without approval.
