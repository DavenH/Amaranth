# Semantic Complexity Audit

## Status

Implemented.

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

## Audit Results

| Operation | Semantic unit and current cost | Required cost | Evidence and disposition |
| --- | --- | --- | --- |
| Envelope traversal grid | Prepare one envelope on configuration revision, sample `C` time positions once, then write `C * R` repeated grid cells. | `O(S + C + C*R)`; no per-column slice. | `EnvelopeSignalProcessor::publishTraversalGrid`; corrected before this audit and retained. |
| E3 envelope morph surface | Each column changes the selected morph axis and is a genuine cross-section. | `O(C * (S + R))`. | `EnvelopeMorphSurfaceRasterizer`; per-column render is inherent, method now names the varying morph input. |
| Trimesh traversal audio | Each morph column requires a slice/bake, but owning column vectors allocate on the audio path. | `O(C * (S + R))`, zero prepared-path allocation. | Follow-up: [`cycle-v2-trimesh-traversal-workspace.md`](cycle-v2-trimesh-traversal-workspace.md). |
| Trimesh source preview | Independently repeats the audio traversal render when captured traversal already exists. | Reuse at `O(C*R)` presentation cost; zero extra slices. | Same traversal-workspace TDD. |
| Point movement | One point influences three curve triplets and four blended waveform segments. | `O(Raffected)` unless endpoint padding or resolution layout changes. | Implemented in [`point-list-incremental-bake.md`](point-list-incremental-bake.md); counters reject full rebuild/sort for an ordinary edit. |
| Curve identity/edit publication | Move the selected object in place and publish one conflict-checked state. | `O(1)` model mutation plus affected render range. | Covered by completed curve identity/state TDDs and live JUCE smoke fixtures. |
| Envelope configuration adoption | Build on configuration revision, then adopt prepared render data once; blocks reuse playback state/storage. | `O(S + R)` per revision, `O(B)` per block. | `EnvelopeSignalProcessor::prepareExecution`; accepted. Dynamic live modulation remains a separate product contract. |
| Ordinary voice slice | Slice each cube, then order potentially crossing/deformed intercepts before baking. | `O(K*S + K log K + R)` per required voice render. | `VoiceRasterizer::renderVoiceSlice`; sorting is semantically required because guide/deformation output can reorder phase. No snapshot publication occurs unless explicitly requested. |
| Chained voice render | Accumulate the next slice and rebake the evolving chained waveform; fixed 2048 storage can grow from render. | Dependent on chain growth, zero realtime allocation, no hidden publication. | Follow-up: [`voice-rasterizer-realtime-storage.md`](voice-rasterizer-realtime-storage.md). |
| Snapshot publication | Copy one complete generation outside the exchange and atomically swap ownership. | `O(output)` once per publication; `O(1)` reader acquisition. | Implemented in [`rasterizer-immutable-snapshot-exchange.md`](rasterizer-immutable-snapshot-exchange.md), including nonblocking publication tests. |
| Preview graph execution | Repeated linear summary/audio lookup creates quadratic graph overhead and propagates owning vectors. | `O(V + E + samples written)`. | Follow-up: [`cycle-v2-preview-execution-index.md`](cycle-v2-preview-execution-index.md). |
| Graph invalidation | Recursively scans all edges and linearly checks visited output per reachable node. | `O(Vr + Er)`. | Follow-up: [`cycle-v2-graph-dependency-index.md`](cycle-v2-graph-dependency-index.md). |
| Graph edit validation/layout | Transactional connect/splice copies and validates a candidate graph; single-node placement examines neighbours to uphold non-overlap. | `O(V + E)` per committed authoring action; no per-mousemove serialization. | Existing authoring smoke tests assert semantic spacing. Accepted for committed edits; keep candidate copies out of drag frames. |
| Panel painting | Visible-node/cable scans are clipped; repaint and texture-bake calls arrive through several editor/host layers. | `O(Vvisible + Evisible)` paint and one dirty request per affected surface/frame. | Follow-up: [`cycle-v2-render-invalidation-coalescing.md`](cycle-v2-render-invalidation-coalescing.md). |

The audit deliberately retains loops whose outputs or changing inputs require
them. The five follow-up TDDs isolate the remaining architectural changes so a
test for one cost contract cannot pressure unrelated production code.
