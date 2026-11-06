# RiffCatch Copilot Instructions

Read and follow root `AGENTS.md` as the shared project rules for architecture, functional preservation, Windows build/selftest, screenshot privacy and Git approvals. Read `docs/design/design-system.md` and `docs/design/visual-checklist.md` for visual tasks. Answer in Norwegian, concisely.

Copilot profiles live in `.github/agents/*.agent.md`: delegate design specifications to `ui-designer`, approved implementation to `juce-developer`, and screenshot review to `visual-reviewer`. Pass a bounded scope and required artifacts; do not delegate to evade a permission restriction. Specialists do not perform Git mutations. Commit and push remain separate, explicitly approved steps for the main agent after user testing.

The profiles read existing `.opencode/agents/` role text and `.opencode/skills/` documents as shared instructions. Do not assume Copilot discovers `.opencode/` definitions natively or supports OpenCode tool names/frontmatter. Do not duplicate the design system or change the OpenCode setup. Runtime usage and smoke tests: `docs/design/copilot-workflow.md`.
