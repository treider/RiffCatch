---
name: riffcatch-visual-design
description: Analyze the RiffCatch Neo-Retro reference and specify hardware-style UI changes without losing existing controls or behavior.
---

# RiffCatch Visual Design

Use for visual analysis and redesign specifications, not as authorization to edit the UI.

1. Read `AGENTS.md` and `docs/design/design-system.md`. Open `docs/screenshots/RiffCatch Retro-Tech DAW Interface.png` as an image. If missing or image viewing is unsupported, request the image or a visual review; never invent reference details.
2. Inspect relevant current components and a current EXE screenshot. Treat the reference as the target, not proof of shipped behavior; do not use screenshots from the discarded redesign as the target.
3. Inventory affected controls, labels, states and hit areas. Preserve functionality and existing theme support.
4. Specify visual hierarchy, geometry, theme roles and light direction. Prioritize the toolbar's pale-blue top edge, localized glints/reflections underneath, dark bevel separation, raised controls and recessed wells. A uniform colored outline is not equivalent to directional lighting.
5. Use restrained orange/gold emphasis, blue displays/tool accents, charcoal surfaces and readable text. Describe light/dark contrast (dodge/burn) as material depth, not blanket brightness or glow on every edge.
6. Propose the smallest coherent component group and the matching normal/hover/pressed/toggled/disabled/focused states. Reuse existing LookAndFeel and theme drawing helpers.
7. Hand off a specification with affected files, preserved interactions, explicit visual acceptance criteria and screenshot comparisons to make. Flag decisions that the static image cannot establish.

Do not infer exact hex values, font families or pixel dimensions from appearance alone. Label estimates and distinguish image observations from implementation proposals. Use `visual-quality-review` after implementation; approval requires screenshots, not a successful build alone.
