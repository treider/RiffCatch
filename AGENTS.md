# RiffCatch – project notes for agents

JUCE 8 / C++ multitrack looper for Windows. Source in `Source/`, UI in `Source/ui/`, audio in `Source/audio/`.

## Build and test
- Stop a running instance first: `Stop-Process -Name RiffCatch -Force`.
- Configure: `cmake -S . -B build`. Build: `cmake --build build --config Release --parallel`.
- Exe: `build/RiffCatch_artefacts/Release/RiffCatch.exe`.
- Selftest: `RiffCatch.exe --selftest` writes `selftest.txt` next to the exe (delete the old one first, wait ~6 s). Must end with `ALL OK`.
- PowerShell 5.1: no `&&`; refresh PATH from Machine+User if cmake/git is not found.

## Workflow
- Bump `project(RiffCatch VERSION x.y ...)` in `CMakeLists.txt` on every iteration.
- "kjør" means build, selftest, UI check, commit. Do not commit or push without being asked.
- Answer in Norwegian, short. Ask before trying random things; use plan mode for open questions.

## Design rules
- Flat look, no gradients. Buttons, combo boxes and tool buttons sit in a dark frame ring (`Theme::framedTile`). Dark wells get a lighter ring (`Theme::wellRing`).
- Use `Theme::radius` / `Theme::radiusLg`, theme colours and `Theme::cardShadow`; never hard-code colours. Nine presets in `Theme.h`, Sunset is the default.
- Audio thread: no locks or allocation; parameters are atomics.

## Gotchas
- Ctrl+letter arrives in `keyPressed` as a control character (Ctrl+T = 20); `MainComponent::keyPressed` normalises it.
- Children inside a viewport (mixer strips) have their own coordinates; convert with `getLocalArea` before painting in the parent.
- UI testing: place the window with `SetWindowPos` (20,10,1500,980), click it first so it has focus, `SendKeys` works for shortcuts. Screenshots appear with a delay; re-read if missing.

## Not in the repo
- `third_party/asio_sdk` (Steinberg licence) and `third_party/lame` are user-supplied and git-ignored; the build degrades gracefully without them.
