# Semantic Complexity Audit

## Status

Proposed.

## Problem

The codebase contains operations whose implementation cost is much larger than
the product operation requires: rerasterizing one envelope for every grid
column, publishing per analysis column, rebuilding collections for one-point
edits, or repeatedly converting stable inputs. A textual search for loops is
not sufficient. Some `O(n)` work is inherent because `n` outputs must be
written; other `O(nR)` work repeats an expensive preparation that semantically
belongs outside the loop.

Agents need explicit domain context and measurable cost contracts, not a generic
instruction to remove loops.

## Goals

- Audit authoring, rasterization, DSP preparation, traversal grids, previews,
  publication, and painting for repeated avoidable work.
- State the semantic unit of work before judging complexity.
- Separate inherent output cost from repeated preparation/publication cost.
- Add counters and scaling tests that detect regressions without timing flakes.
- Prefer reuse of mature Cycle v1 algorithms when they embody the intended
  complexity.

## Audit Method

For each public operation, record:

1. Product meaning: e.g. move one vertex, render one envelope, produce `n`
   columns, publish one panel generation.
2. Input/output sizes and which may vary independently.
3. Required work and expected asymptotic cost.
4. Expensive sub-operations: sorting, slicing, baking, FFT, serialization,
   allocation, locking, snapshot copying, graph traversal, repaint requests.
5. Call multiplicity and ownership: once per edit, node, layer, column, sample,
   block, voice, or paint.
6. Existing authoritative implementation and parity evidence.

Shape methods as pure functions where practical: pass values that genuinely
vary per call explicitly, so the signature reveals the computation's changing
dependencies. Keep stable collaborators or saved references as members when
they are object identity rather than per-call input. Do not mechanically expand
every member into a parameter list.

Do not optimize from syntax alone. A pass may change code only after the
semantic contract and independent proof are written.

## Initial Audit Targets

- Envelope signal-grid preparation versus per-column application, contrasted
  with E3's required per-morph-position cross-section renders.
- Ordinary/chained voice rasterization and snapshot publication.
- Point and curve drag paths, including sort/rebuild/publication frequency.
- Trimesh block/grid rendering across columns and layers.
- Envelope preparation/adoption and release-buffer copying.
- Node preview recalculation and downstream invalidation.
- Graph editing layout/collision passes for single-node operations.
- Panel snapshot copying, locking, texture baking, and repaint fan-out.

## Verification Technique

Prefer deterministic instrumentation over wall-clock benchmarks:

- number of mesh slices, curve bakes, sorts, publications, allocations, locks,
  graph traversals, and repaints;
- input-scaling tests at `n`, `2n`, and `4n` that assert operation counts;
- prepared-path allocation guards;
- fault injection that moves an expensive operation back inside the repeated
  loop and demonstrates test failure.

Use performance timing only for coarse end-to-end confirmation after operation
counts establish the cause.

## Deliverables

- A table of audited operations, current cost, required cost, evidence, and
  disposition.
- Focused corrective TDDs for each architectural change; do not combine
  unrelated optimizations into one implementation branch.
- Semantic tests for every accepted complexity contract.
- Explicit notes for apparently expensive work that is actually inherent or
  intentionally cached elsewhere.

## Completion Criteria

- Every high-frequency edit/render path has a documented semantic cost model.
- No expensive preparation or publication is repeated at a finer lifecycle
  scope than its changing inputs require.
- Complexity tests fail on representative `O(nR)` or `O(n log n)` regressions
  where the contract requires `O(R + n)` or local-edit cost.
