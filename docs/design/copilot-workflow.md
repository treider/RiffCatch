# Copilot App Compatibility

## Adapter layout

Copilot discovers the three native profiles in `.github/agents/*.agent.md`. Each has `name`, `description` and an explicit `tools` allowlist. No model, default-agent override, extension, MCP server or dependency is added.

| Profile | Allowed tools | Role |
| --- | --- | --- |
| ui-designer | read, search | Read-only analysis and specifications |
| juce-developer | read, search, edit, execute | Approved implementation and Windows verification |
| visual-reviewer | read, search | Read-only review of supplied images |

There is no OpenCode `mode: subagent` field in Copilot's shared agent format. These profiles can be delegated to as specialists or selected manually with `/agent`. No profile enables the `agent` tool: specialists cannot delegate further. The main session coordinates work.

`.github/copilot-instructions.md` points to shared `AGENTS.md`. Each profile explicitly reads the existing OpenCode role's Markdown body and the relevant skill document using `read`. For the three original `.opencode/skills` documents this is instruction reuse, not frontmatter import or native Copilot skill registration. Do not expect those documents to appear in `/skills`; keep `.opencode/` files available when using these adapters.

`juce-glow-rendering` is different: `.agents/skills/juce-glow-rendering/SKILL.md` is a documented native project-skill location for both Copilot and OpenCode. Its standard YAML `name` and `description` register the skill without an adapter, duplicate instructions, Windows symlink or tool-permission override. It contains practical JUCE mask/blur, material, caching and visual-quality recipes, and refers back to the existing design system.

OpenCode continues to use its own profiles unchanged. Keep shared role/skill requirements in their existing files; update the small Copilot adapters only for client-specific behavior. Copilot's tool allowlist does not translate OpenCode's `bash: ask`: use session approval settings for command approvals. Developer Git restrictions are instructions, not a shell security boundary.

## Use in the desktop app

1. Open this repository with **Open local folder**.
2. Select **This computer** and **Current checkout** to retain the existing Windows build and working folder. Avoid simultaneous edits by OpenCode and Copilot in that checkout.
3. Start a fresh **Interactive** session with **Ask every time** tool permissions. No `/init`, settings reset or dependency installation is required.
4. Type `/agent` and confirm `ui-designer`, `juce-developer` and `visual-reviewer` are offered. Select a profile for direct use, or keep the normal main agent and ask it to delegate by name.
5. For delegated work, inspect the tool activity to confirm a real custom-agent invocation. A response merely saying it is acting as the designer is not proof of delegation. Do not assume OpenCode's `@agent` syntax applies; `@` can reference files in Copilot.

Example main-session prompts:

```text
Delegate to the ui-designer custom agent: analyze the current Bass panel against our reference and return a specification only. Do not edit or run commands.
```

```text
Delegate to the juce-developer custom agent: read our shared instructions and report the existing build/selftest commands. This is a smoke test; do not edit, build or run commands.
```

```text
Delegate to the visual-reviewer custom agent: review the attached reference and before/after images using our checklist. Report missing evidence as BLOCKED. Do not edit or capture images.
```

Supply screenshot paths/attachments and metadata explicitly. A read/search-only reviewer cannot capture the UI. Image-capable tooling/model support must be checked in the actual app; missing support is a blocker, not permission to grant shell or computer-use tools.

In a separate worktree/clone, only committed framework files are automatically available; ignored `build/`, ASIO SDK and LAME directories are not carried over. Do not copy an existing CMake cache between worktrees. For the current workflow prefer **Current checkout** and native Windows execution, not WSL/cloud. Protect unsaved sessions before builds and never delete autosaves to simplify testing.

## Verification

- Static checks: confirm the three filenames, YAML fields and documented tool aliases; verify every shared instruction path exists. No app build/version bump is required for this documentation/configuration change.
- Runtime agent discovery and permission enforcement require a live Copilot session. The `copilot` executable was not found on the original setup shell PATH, so no standalone Copilot runtime validation was claimed. The desktop app may still be installed with an embedded runtime.
- Use `/agent` in a fresh desktop session and run the smoke prompts above. Confirm designer/reviewer have no edit, execute or delegation tools. Ask the reviewer to identify missing evidence with no images supplied; expect BLOCKED, not a fabricated review.
- If using a separately installed Copilot CLI, launch `copilot` from the repository root and use `/agent`. There is no assumed `copilot agent list` equivalent here; custom-agent discovery requires a live session per the CLI reference.
- If definitions are missing, check the selected checkout, app version and conflicting global agents. Do not run `/init` or rewrite shared rules as a discovery workaround.

### Shared lighting skill

In a fresh Copilot session rooted in this checkout, request:

```text
Load juce-glow-rendering and explain its padded Gaussian mask recipe without editing or running a build.
```

Confirm that a real skill load reads `.agents/skills/juce-glow-rendering/SKILL.md`; merely mentioning its name is not proof. Check folder/name equality, uppercase `SKILL.md`, valid YAML and duplicate global skill names if discovery fails. A session started before the file existed may need reopening. OpenCode's `debug skill` verifies OpenCode, not Copilot; report format/path validation and live Copilot loading separately. This documentation-only addition does not require changing application source, building or bumping its version.

Verified locally with GitHub CLI 2.102.1:

```powershell
gh skill list --agent github-copilot --scope project --json skillName,path,agentHosts,scope
gh skill publish ".agents" --dry-run
```

The first command discovers `juce-glow-rendering` in the shared directory with
`agentHosts: ["github-copilot"]` and project scope. The second passes the official
Agent Skills validation without publishing. Its optional-license and
tag-protection recommendations are not format failures; no license, tag,
release or repository setting was added.

An actual skill-load attempt in the already-running Copilot conversation returned
`Skill "juce-glow-rendering" not found` and listed only its previously available
skills. Therefore GitHub CLI discovery and format validation are verified, but
live Copilot loading after reopening remains a smoke-test requirement. Do not
claim the current conversation has loaded it or install a duplicate to bypass
the refresh requirement.

## Official references

- [Custom agent configuration and tool aliases](https://docs.github.com/en/copilot/reference/custom-agents-configuration)
- [Copilot app session/workspace selection](https://docs.github.com/en/copilot/how-tos/github-copilot-app/agent-sessions)
- [Copilot app slash commands, including /agent](https://docs.github.com/en/copilot/reference/github-copilot-app-reference/slash-commands)
- [Copilot CLI reference](https://docs.github.com/en/copilot/reference/copilot-cli-reference/cli-command-reference)
- [Native skill format and shared project locations](https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/customize-cloud-agent/add-skills)
