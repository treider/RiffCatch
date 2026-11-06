---
name: visual-quality-review
description: Review actual RiffCatch screenshots against the Neo-Retro reference for depth, reflections, legibility, state clarity and layout regressions.
---

# Visual Quality Review

Use for an evidence-based review, not implementation. Read `docs/design/design-system.md` and `docs/design/visual-checklist.md`.

Required evidence: reference image, before/after app-window screenshots, build/version, window size, DPI, theme and interaction state. Request missing evidence; do not claim visual verification from source code or selftest output.

1. Open the reference and actual screenshots using image-capable tools/model. If unavailable, report a blocked visual review rather than substituting a text-only guess.
2. Inspect full-window hierarchy first, then native-size crops of the toolbar, track wells, knobs, sequencer and tuner/master. Enlarged crops supplement, not replace, normal-size inspection.
3. Check directional material depth: pale-blue toolbar top edge and small reflected highlights below it, dark bevel separation, localized orange/gold rim light, inset shadows and legible raised controls. Reject flat colored outlines as a substitute for this depth.
4. Check glow spill, contrast, clipped edges, unexpected bands, consistent light direction, text/icon clarity and control-state differentiation. Preserve blue seven-segment readability.
5. Compare normal/minimum size, alternate themes, Drums/Bass and populated/empty views. A static screenshot cannot prove interaction or audio correctness; keep these separate from visual findings.
6. Return findings ordered by severity, with screenshot path/region and relevant source references when known, expected versus observed behavior, and actionable recommendations. List untested conditions.

Verdict: `PASS` only for the reviewed scope with sufficient evidence, `CHANGES REQUIRED` for visible issues, or `BLOCKED` for missing evidence. No edits, builds, capture commands, Git mutations or claims of user approval. The primary agent/user supplies captures and performs corrections.
