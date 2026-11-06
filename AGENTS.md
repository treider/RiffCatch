# RiffCatch – project notes for agents

JUCE 8 / C++ multitrack looper for Windows. Source in `Source/`, UI in `Source/ui/`, audio in `Source/audio/`.

## Architecture and C++ conventions
- C++20, JUCE 8.0.4 via CMake FetchContent. `Source/Main.cpp` owns the application, window and `RiffLookAndFeel` lifetime.
- `MainComponent` owns the audio device manager/engine, transport controls, component layout, shortcuts, undo and project actions. Inspect existing callbacks before changing UI.
- `Source/audio/`: `AudioEngine` is the device callback and sample-based transport; tracks, drums, bass, patterns, effects and analysis live here.
- `Source/project/`: project persistence, audio import and offline export. Preserve saved project/settings compatibility.
- `Source/ui/`: track rows, mixer, drum/bass panels, pattern/song controls, tuner and displays. Shared painting and LookAndFeel live in `Theme.h`; display rendering in `SegmentDisplay.h` and `LibraryCounter.h`.
- Follow local JUCE formatting, ownership and RAII conventions. Keep layout in `resized()`, drawing in `paint()`/LookAndFeel, UI changes on the message thread. Respect component and asynchronous callback lifetimes.
- Audio thread: no new locks, allocations, file I/O or UI work; parameters use atomics. Inspect existing synchronization rather than assuming the engine is entirely lock-free.

## Build and test
- Stop a running instance first: `Stop-Process -Name RiffCatch -Force`.
- Configure: `cmake -S . -B build`. Build: `cmake --build build --config Release --parallel`.
- Exe: `build/RiffCatch_artefacts/Release/RiffCatch.exe`.
- Selftest: `RiffCatch.exe --selftest` writes `selftest.txt` next to the exe (delete the old one first, wait ~6 s). Must end with `ALL OK`.
- PowerShell 5.1: no `&&`; refresh PATH from Machine+User if cmake/git is not found.
- Check actual command exit codes and full errors; filtered output is not proof of a successful build. Run configure before building after a version change.
- Code changes require a Release build, fresh selftest output ending in `ALL OK`, and relevant interaction checks. Selftest does not establish visual quality or replace the user's audio/hardware testing.
- Visual changes also require before/after screenshots inspected against the reference, at normal and minimum window sizes. Follow `docs/design/visual-checklist.md`; if capture or image viewing is unavailable, report verification as blocked.
- Documentation/framework-only changes require path/configuration checks, not a version bump or application rebuild.

## Workflow
- Bump `project(RiffCatch VERSION x.y ...)` in `CMakeLists.txt` on every application-code iteration, not on a documentation-only task or rebuild.
- Preserve the workflow: inspect/edit, build the Windows EXE, let the user test, then commit/push only with the user's approval. No automatic commit or push for visual changes.
- "kjør" executes the currently agreed scope. It does not override an explicit no-commit/no-push instruction; when Git approval is unclear, ask.
- Answer in Norwegian, short. Ask before trying random things; use plan mode for open questions.
- Inspect branch, status and diff first; preserve unrelated work. Do not create/delete/switch branches, discard changes, change Git configuration or force-push without approval. Do not resurrect the discarded redesign branch.
- When approved, stage only relevant reviewed files, follow recent commit style (often `vX.Y: ...` for app iterations), and verify status. Commit approval is not push approval.
- Preserve every control, shortcut, parameter range/default, menu, undo action, recording/loop/Catch behavior, sequencer operation and save/export path unless explicitly asked to change it. Redesign is not permission to change the audio engine.
- Never delete autosaves or reset user settings to make a test pass. Ask before potentially losing an unsaved session.

## Design rules
- Current v0.63 baseline: mostly flat controls. Buttons, combo boxes and tool buttons sit in a dark frame ring (`Theme::framedTile`). Dark wells get a lighter ring (`Theme::wellRing`).
- Approved future redesign target: `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png`, documented in `docs/design/design-system.md`. Its directional highlights, reflections, gradients and recessed shadows supersede the old flat/no-gradient rule for approved redesign work only. Do not redesign as part of unrelated tasks.
- Use `Theme::radius` / `Theme::radiusLg`, theme colours and `Theme::cardShadow`; never hard-code colours. Nine presets in `Theme.h`, Sunset is the default.
- Preserve all built-in/custom themes and the blue seven-segment displays. Add reusable theme roles only when needed; do not scatter new palette literals through components.

## OpenCode skills and agents
- Native skills live in `.opencode/skills/<name>/SKILL.md`. Load `riffcatch-visual-design` for reference-led specifications, `juce-custom-lookandfeel` for UI implementation, and `visual-quality-review` for screenshot reviews.
- `@ui-designer`: read-only visual analysis/specification. `@juce-developer`: scoped implementation and build/testing. `@visual-reviewer`: read-only, evidence-based review; cannot capture screenshots or build, so supply artifacts from the primary agent/user.
- For redesign: reference and baseline -> specification -> approved implementation -> build/selftest -> screenshot review -> user test/approval -> separately authorized Git actions. Use specialists when helpful, not as mandatory overhead for trivial tasks.
- Agents inherit the invoking session model. Do not change the default agent, install dependencies or add provider-specific conventions for this framework.
- Discovery and smoke tests: `docs/design/opencode-workflow.md`.

## Gotchas
- Ctrl+letter arrives in `keyPressed` as a control character (Ctrl+T = 20); `MainComponent::keyPressed` normalises it.
- Children inside a viewport (mixer strips) have their own coordinates; convert with `getLocalArea` before painting in the parent.
- UI testing: place the window with `SetWindowPos` (20,10,1500,980), click it first so it has focus, `SendKeys` works for shortcuts. Screenshots appear with a delay; re-read if missing.
- Capture only the RiffCatch window with Win32 `PrintWindow` and a verified nonzero window handle, not the whole desktop. Store temporary captures in `%TEMP%`; record window size/DPI/theme/state. Check for blank/stale captures. Do not send input to another app. Close normally after testing.

## Not in the repo
- `third_party/asio_sdk` (Steinberg licence) and `third_party/lame` are user-supplied and git-ignored; the build degrades gracefully without them.
