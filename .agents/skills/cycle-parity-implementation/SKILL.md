---
name: cycle-parity-implementation
description: Implement a Cycle V2 feature or TDD through verification and completion, preserving Cycle 1 behavior and interaction complexity where applicable. Use for parity ports and explicit Cycle V2 TDD implementation requests.
---

# Cycle Parity Implementation

Use this skill when the requested outcome is working Cycle V2 behavior. Start with `cycle-architecture-review` for a proposal-only request or `cycle-ui-regression` when the task starts from a reported UI failure.

Read the relevant sections of `AGENTS.md` and the active `docs/TDD/` document. Treat them as the source of engineering rules and completion criteria; this skill supplies the task route.

1. Find the mature Cycle 1 implementation where equivalent behavior exists, the Cycle V2 owner, and any existing shared core. Make a compact parity map: observable behavior, pointer-down/update/commit cost where there is an interaction, ownership and thread boundaries, and the exact translation Cycle V2 needs. For genuinely new behavior, state the new contract instead. Record the authoritative source and deletion target before proposing an adapter. Do not copy mature domain behavior into a bridge.
2. Resolve the active TDD. Create or update one for cross-subsystem work or multiple coherent slices. State semantic acceptance, negative boundaries, complexity expectations, and what old path must disappear. Keep unresolved architecture decisions explicit rather than approximating Cycle 1 behavior.
3. Implement the smallest complete slice. For a live gesture, verify multiple updates, commit, downstream effect, and undo; scale unrelated state and use operation counters when complexity parity is at issue. For DSP, compare observable output and edge rates or sample rates. Use `docs/cycle-agent-runbook.md` when UI automation is needed.
4. Follow the `AGENTS.md` engineering loop: reread and refactor the production diff, run style and architecture checks, run proportionate semantic tests, and commit. Reopen the TDD after each commit and continue until its completion and deletion criteria are met, unless the user explicitly narrows or stops the work or a genuine design blocker remains.

Report the existing source reused where applicable, Cycle V2 translation, semantic evidence, committed slices, and any remaining TDD criteria. Never label partial work complete.
