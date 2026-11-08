# RiffCatch Agent Guide

Windows multitrack looper. C++20, JUCE 8.0.4, CMake. Answer briefly in Norwegian.

## Working rules
- Inspect branch/status/diff and relevant code before editing. Preserve unrelated work. Ask when scope or a risky action is unclear; otherwise execute the agreed scope.
- Prefer small, local changes and existing patterns. Read only relevant files; load specialist instructions on demand. Delegate bounded work when useful, not for trivial tasks. Avoid duplicate research and repeated full-file reads.
- Preserve every control, shortcut, parameter range/default, callback, undo action, recording/loop/Catch operation, sequencer behavior and save/export path unless explicitly asked otherwise. Preserve project/settings compatibility.
- Never delete autosaves or reset settings to pass tests. Protect unsaved sessions before stopping the app.
- Workflow: inspect/edit -> Windows build and verification -> local commit -> user test. Commit every completed, verified change so it is easy to roll back; "Kjor" includes the local commit.
- Never push without explicit approval, and commit approval is never push approval. No branch creation/deletion/switching, discarded-redesign resurrection, destructive Git commands or Git configuration changes without approval. Stage only reviewed relevant files; follow recent commit style and verify status.

## Code map and conventions
- `Source/Main.cpp`: application/window and LookAndFeel lifetime. `MainComponent`: devices/engine, controls/layout, shortcuts, undo and project actions.
- `Source/audio/`: callback engine, sample transport, tracks, drums, bass, patterns and effects. No new audio-thread locks, allocations, file I/O or UI work. Inspect existing synchronization; do not assume everything is lock-free. No DSP changes for visual tasks.
- `Source/project/`: persistence, import/export. `Source/ui/`: components; shared painting in `Theme.h`, displays in `SegmentDisplay.h`/`LibraryCounter.h`.
- Follow local JUCE formatting/RAII. Layout in `resized()`, painting in `paint()`/LookAndFeel, UI on message thread. Preserve component, LookAndFeel and asynchronous callback lifetimes. Never reset parameters during restyling.
- Gotchas: normalize Ctrl+letter control characters in shortcuts; convert viewport-child coordinates with `getLocalArea` before parent painting.

## Build and verification
- App-code iteration: bump `project(RiffCatch VERSION ...)` in `CMakeLists.txt`. Documentation/configuration-only work or rebuild: no bump or app build required.
- After protecting unsaved work, stop RiffCatch before building. Run `cmake -S . -B build`, then `cmake --build build --config Release --parallel`. Check real exit codes/full errors, not filtered output.
- EXE: `build/RiffCatch_artefacts/Release/RiffCatch.exe`. Delete only its old generated `selftest.txt`; run EXE with `--selftest`, wait for exit, read fresh output ending in `ALL OK`. Also check affected interactions; hardware/audio testing remains with the user.
- PowerShell 5.1: no `&&`. If needed, refresh current PATH from Machine+User. ASIO SDK and LAME are optional user-supplied, ignored dependencies; do not install them automatically.
- Visual changes require actual before/after window screenshots at normal/minimum sizes, reference comparison and stated coverage gaps. Follow `docs/design/visual-checklist.md`. Missing capture/image support is BLOCKED, not a pass. Capture only verified RiffCatch HWND with `PrintWindow`, never the desktop; use `%TEMP%`, focus before input, close normally.

## Design and specialists
- Preserve existing themes, defaults and blue seven-segment displays. Use `Theme` colors/radii/shadows; centralize new roles. Do not redesign unrelated code.
- Approved redesign target: `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png`. For redesign, open it and read `docs/design/design-system.md`; directional highlights/reflections supersede the historical flat/no-gradient rule.
- Design -> approved scope -> implementation -> build/selftest/screenshots -> review -> user test. Designer/reviewer are read-only; the primary agent or developer supplies captures.
- Load only the relevant `.opencode/skills/<name>/SKILL.md`: `riffcatch-visual-design`, `juce-custom-lookandfeel`, or `visual-quality-review`.
- For glow, bloom, bevels and lighting diagnostics, load `juce-glow-rendering` from `.agents/skills/juce-glow-rendering/SKILL.md`, the shared native OpenCode/Copilot skill.
- OpenCode: `@ui-designer`, `@juce-developer`, `@visual-reviewer`; setup in `docs/design/opencode-workflow.md`.
- Copilot: `.github/agents/*.agent.md`, select with `/agent` or delegate by name; setup in `docs/design/copilot-workflow.md`. Adapters read shared role/skill text; OpenCode YAML is not Copilot configuration.
- No model pinning, default-agent changes or framework dependencies. Use native Windows/current checkout for EXE work; never run competing editing sessions on the same files.
