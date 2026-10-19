---
name: cycle-ui-regression
description: Reproduce, diagnose, and verify a Cycle UI behavior bug with focused agent fixtures, state assertions, logs, and screenshots. Use for broken gestures, menus, previews, selection, or editor publication; use ui-design for aesthetic critique alone.
---

# Cycle UI Regression

Read `docs/cycle-agent-runbook.md` for current commands and artifact handling, plus the applicable UI rules in `AGENTS.md`. Start from the user's observed behavior and identify the full interaction sequence that should fail before the fix.

1. Reproduce with the smallest existing fixture or add a focused fixture under `scripts/fixtures/`. Inspect target and assertion paths before inventing selectors. Capture the relevant model state, visible panel, and downstream effect as applicable; a screenshot alone is insufficient when state can be asserted.
2. Run `scripts/run_cycle_agent.sh` for focused in-app automation and inspect the report, filtered log, then raw log or `.ips` only when needed. Use `scripts/capture_cycle_ui.sh` for a full-window comparison when layout or visual behavior matters. Run GUI fixtures sequentially.
3. If the report concerns a physical trackpad, keyboard, focus, or event-routing sequence that synthetic `pointer` commands cannot establish, compare native event evidence and use the runbook's native-input path. Preserve the complete event order, including movement and settled state.
4. Trace the failure from input through the authoritative model or command owner to presentation and downstream processing. Fix that owner, retain a focused fixture or assertion that fails on the reported behavior, and rerun the same sequence after the change. Use the relevant build and test gates in `AGENTS.md`.

Return the reproduction, root cause, fix, before/after evidence, and any remaining limitation. Record incidental assertions or crashes in the appropriate bug ledger as directed by `AGENTS.md`.
