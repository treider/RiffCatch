# OpenCode Framework Usage

## Files and discovery

- Root `AGENTS.md` supplies project rules.
- `.opencode/skills/<name>/SKILL.md` supplies on-demand native skills. Names match folders; each has YAML `name` and `description`.
- `.agents/skills/juce-glow-rendering/SKILL.md` is a shared native skill for OpenCode and GitHub Copilot. Its lighting recipes are stored once; the existing `.opencode/` skills stay in place.
- `.opencode/agents/*.md` supplies native subagents with YAML `description`, `mode: subagent` and permissions. No model is pinned; they inherit the invoking session model.
- No default-agent override, Git configuration, build-script change, new dependency or Copilot-specific convention is required.
- The reference lives at `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png`. Keep it available alongside the documents; it was untracked at setup. Include it in a future approved commit if it should follow the clone. No staging is performed by framework setup.

From the repository root, check discovery with:

```powershell
opencode --version
opencode agent list
opencode debug skill
opencode debug agent ui-designer
opencode debug agent juce-developer
opencode debug agent visual-reviewer
```

Expect all three specialist names as subagents and the three original skill names plus `juce-glow-rendering`, with paths in this repository. The designer/reviewer use a deny-by-default tool allowlist for read/search/skills/questions, with external directory access requiring approval. They cannot run shell commands, edit or delegate to a write-capable agent. Developer shell commands require approval; Git operations remain with the primary agent by instruction. Parent approvals and review still matter; documentation is not a substitute for tool permissions.

## Desktop smoke tests

1. Reopen this project in OpenCode Desktop and start a fresh session so new definitions are loaded. Keep your normal primary agent selected.
2. Type `@ui-designer` and select it from autocomplete. Send: `Load riffcatch-visual-design and compare the reference with the current Theme.h. Return a specification only; do not edit.`
3. Type `@juce-developer` and send: `Load juce-custom-lookandfeel. Explain the existing build and selftest commands without editing or running them.`
4. Type `@visual-reviewer` and send: `Load visual-quality-review. Review these supplied reference/before/after screenshots; do not edit. Report missing evidence as BLOCKED.` Attach actual captures or provide their paths and metadata.
5. In the primary session, request `Load the riffcatch-visual-design skill` (repeat for the other names) and check that the native skill tool loads the matching file. Skills are not necessarily slash commands.
6. Request `Load juce-glow-rendering and explain outer glow versus inner glow without editing.` Confirm the loaded path is `.agents/skills/juce-glow-rendering/SKILL.md`. No new tool permission or agent/model override is needed.

If names are missing, verify the project root, filename case (`SKILL.md`), YAML, folder/name equality and effective skill/task permissions. Check for conflicting global definitions. CLI discovery does not prove the running Desktop instance has reloaded; restart/reopen it if necessary. Image analysis requires image-capable tools and the selected model; report a blocker rather than silently changing models.

## Existing development loop

Inspect and implement the approved scope, generate the Windows EXE, let the user test, then commit and push only as authorized. Specialist agents assist this workflow; they do not replace it.

Before building, protect unsaved work and stop any running RiffCatch instance. Run these commands separately from the repository root, proceeding only after success:

```powershell
Stop-Process -Name RiffCatch -Force -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release --parallel
```

If executable lookup fails, refresh the current shell PATH (not global configuration):

```powershell
$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
```

Remove only the old generated `build/RiffCatch_artefacts/Release/selftest.txt` using the available file tool before a selftest. Run:

```powershell
Start-Process -FilePath "build/RiffCatch_artefacts/Release/RiffCatch.exe" -ArgumentList "--selftest" -Wait
```

Read the newly generated sibling `selftest.txt`; it must end in `ALL OK`. Allow about six seconds, verify freshness and report failures. Launch the same EXE for user/UI testing. Visual code changes require the screenshot checklist before completion. Documentation-only framework setup does not require building the app or bumping its version.

Native format references: https://opencode.ai/docs/agents/ and https://opencode.ai/docs/skills/.

## Setup verification

- `opencode agent list` completed successfully and discovered `ui-designer`, `juce-developer` and `visual-reviewer` as subagents with the configured permission rules.
- `opencode debug skill` completed successfully and loaded all three skill names, descriptions, content and project-local paths. This verifies the native frontmatter parser and discovery, not just file existence.
- Required folders and the reference image were opened successfully; `git diff --check` passed for the tracked instruction changes.
- Desktop autocomplete, actual subagent invocation and in-session skill loading still need the smoke tests above in a fresh Desktop session. No model call, GUI redesign, app build, commit or push was performed during framework setup.

### Shared glow skill verification

`opencode debug skill` was run from the repository root with OpenCode 1.4.1.
Its native parser returned exactly one `juce-glow-rendering` entry, including the
description, full content and location
`.agents/skills/juce-glow-rendering/SKILL.md`. The YAML has the required `name`
and `description`; name syntax, folder equality and description length pass.
The existing three `.opencode/skills` definitions are unchanged.

This verifies local OpenCode discovery, not hot reload in an already open
Desktop conversation. Use the fresh-session smoke prompt above for that surface.
