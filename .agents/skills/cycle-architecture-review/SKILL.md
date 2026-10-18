---
name: cycle-architecture-review
description: Review a Cycle design proposal, TDD, or implementation for ownership, reuse, complexity, and completion gaps. Use when asked to critique or revise architecture; use cycle-parity-implementation when the requested outcome is implementation.
---

# Cycle Architecture Review

Use this for a design or implementation review, including a request to find, create, or revise a TDD. Read the relevant `AGENTS.md` architecture rules and current TDD before judging the proposal. For a review-only request, report findings without starting an implementation train.

1. Identify the authoritative existing behavior and source, the proposed owner, each caller's domain facts, and the policy decisions they currently make. Trace dependencies across UI, graph, runtime, and node layers. Check `docs/TDD/refactors.md` before adding a new refactor wish.
2. Evaluate semantic and interaction-complexity contracts, thread and lifecycle boundaries, adapter translation, and deletion targets. A facade that preserves duplicate policy decisions or copies mature behavior is not a completed extraction.
3. For an existing production change, inspect `git diff --stat`, touched C++ file sizes, new type/kind branches, and overlap with mature sources. Run `python3 scripts/cycle_v2_architecture_audit.py` when reviewing a nontrivial Cycle V2 production change. Apply the review triggers in `AGENTS.md` as prompts for responsibility analysis, not automatic split targets.
4. Return prioritized findings with source locations, consequences, and concrete corrections. When asked to revise a TDD, record baseline and target ownership, complexity, semantic proof, deletion criteria, and unresolved decisions. Mark partial work as partial.

Keep the review concise enough to guide the next implementation decision. Distinguish confirmed defects from questions that require product or architectural direction.
