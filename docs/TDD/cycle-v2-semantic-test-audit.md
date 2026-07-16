# Cycle 2 Semantic Test Audit

## Status

Implemented (2026-07-15).

Depends on the completed Cycle 2 node architecture TDDs. This audit is the
verification follow-up to those refactors; it does not reopen their production
design unless a behavioral test exposes a broken boundary.

## Problem

Cycle 2 accumulated a large unit suite while losing important product
behavior. The suite did not detect broken curve hover, incorrect drag
classification, failure to publish edits, unstable Envelope selection,
downstream processing that ignored curve changes, or stale interactor pointers
after model synchronization.

Assertion count is therefore not a useful quality measure. Many assertions
exercise construction, getters, duplicated fixtures, synthetic payloads, or
implementation structure without proving a user-visible or architectural
contract. In-app pointer injection also bypasses AppKit and JUCE event delivery,
so it cannot establish that a real mouse gesture works.

The project needs a smaller, explicit semantic safety net whose failures map to
product risks and whose tests are themselves shown to fail when the guarded
behavior is broken.

## Goals

- Inventory Cycle 2 tests by the observable contract and product risk they
  guard.
- Remove or consolidate tests that provide no distinct semantic protection.
- Establish high-signal contracts for graph mutation, persistence, execution,
  DSP parity, editor interaction, publication, and downstream recomputation.
- Use mature Cycle 1 behavior as the oracle where Cycle 2 is intended to have
  parity.
- Exercise macOS/JUCE event delivery for editing workflows that cannot be
  proven below the application boundary.
- Keep the native editing acceptance test fast enough for routine local use.
- Record untested risks honestly instead of presenting aggregate test or
  assertion counts as evidence.

## Non-Goals

- Maximize line, branch, test-case, or assertion count.
- Preserve every existing test or fixture.
- Replace focused deterministic unit tests with UI automation.
- Test JUCE, the C++ standard library, trivial accessors, or compiler-enforced
  properties.
- Bless Cycle 2 approximations when a mature Cycle 1 implementation defines
  the intended behavior.
- Refactor production architecture solely to make an implementation-detail
  test pass.

## Semantic Test Standard

Every retained or new test must identify:

1. the product or architecture risk;
2. the public or narrow collaborator boundary it exercises;
3. the externally observable precondition, action, and result;
4. the defect that would make the test fail;
5. why a lower test layer cannot prove the same behavior, when using an
   integration or native test.

A test is high-signal when a plausible regression changes its result without
the test knowing production implementation details. A test is not justified
merely because it executes code, increases coverage, or asserts values copied
from the implementation.

### Retain

Retain a test when it uniquely guards at least one of:

- a domain invariant or rejected invalid state;
- a semantic state transition, including undo/redo and conflict handling;
- serialization compatibility or exact round-trip meaning;
- scheduling, routing, ownership, or realtime lifecycle behavior;
- numerical behavior, edge conditions, or parity with an authoritative core;
- native input delivery, rendering composition, or another platform boundary.

### Consolidate Or Delete

Consolidate or delete tests that only prove:

- constructors assign their arguments;
- getters return values just assigned by setters;
- enum values, static tables, or duplicated node-definition rows match a copy
  embedded in the test;
- mocks return data configured by the same test;
- synthetic scaffolding reaches a branch without asserting semantic output;
- internal call counts, private data shape, or class layout with no stated
  architectural invariant;
- the same contract already proven more directly at another layer.

Deletion requires checking that no distinct risk or edge case is lost. The
audit must record the consolidation destination rather than using test-count
reduction as a goal in itself.

## Test Layers

Use the lowest layer that can prove the contract, with deliberate overlap only
at critical boundaries.

### Domain And Command Tests

Run without `NodeCanvas`, OpenGL, or a live application. They own:

- graph aggregate invariants and typed node-definition validation;
- one-mutation revision behavior and conflict rejection;
- compound edit coalescing and undo/redo meaning;
- stable curve, cube, node, port, and edge identity;
- malformed snapshot rejection without partial mutation;
- exact topology and parameter persistence.

These tests assert resulting graph/model state and emitted semantic change,
not internal helper calls.

### Runtime And DSP Contract Tests

Run processors and compiled graphs through production configuration and
execution boundaries. They own:

- compiled routing and buffer-slot correctness;
- retained per-voice state and allocation-free prepared execution;
- blockwise and gridwise output shape;
- downstream invalidation and recomputation after semantic edits;
- deterministic edge-rate, block-size, and sample-rate behavior;
- Cycle 1/Cycle 2 parity where both consume the same domain semantics.

Parity tests should share inputs, configuration, and tolerances. They must not
maintain a second formula in the test as the oracle. DSP tests assert complete
buffers, stable metrics, or selected samples that distinguish likely defects;
they do not rely only on “non-empty”, “finite”, or “changed” assertions.

### Host Integration Tests

Run editors through `NodeEditorHost`, command services, and real models without
a native window when native delivery is irrelevant. They own:

- editor capability discovery and stable node rebinding;
- one gesture producing one semantic publication;
- selection restoration by identity after snapshot replacement and undo/redo;
- controls, model snapshot, and revision being observed atomically;
- safe editor close and delegate teardown.

### Native macOS Acceptance Test

Use one persistent Cycle V2 process and OS-delivered pointer events through
`scripts/test_cycle_v2_native_edit_smoke.py`. In-app `pointer` commands are not
an acceptable substitute for this layer.

The test must prove state transitions, not merely successful automation
responses. It covers, in one application session:

- palette creation of ordinary production nodes, cable connection and
  replacement, insertion into an existing cable, selection and deletion;
- cable insertion preserves an editable layout with explicit clearance between
  the inserted node and both immediate signal neighbours;
- graph undo/redo around native authoring gestures and exact graph persistence
  across save/reload;
- flat curve hover entering and leaving, curve highlight state, and resize
  cursor state before mouse-down;
- vertex insertion, stable selection, movement, curve sharpening toward a
  controlling vertex from both vertical polarities, deletion, and reinsertion;
- Envelope hover, stable parameter display without interaction, vertex
  movement, publication, and downstream output change;
- Trimesh insertion, movement, collision rejection, curve reshaping, vertex
  parameter edits, morph movement, deletion, and reinsertion;
- deterministic downstream audio and configuration changes after DSP-relevant
  curve edits;
- popup open/close and editor rebinding without a crash or stale selection.

For each edit the runner captures state before the gesture and polls for the
specific expected mutation afterward. A successful command, repaint count, or
revision increment alone is insufficient when the contract concerns geometry
or output.

Normal per-edit settlement is at most 100 ms. The test must not add fixed
multi-second sleeps. Startup, shutdown, OpenGL capture, and crash-report
collection may have separate bounded timeouts. GUI acceptance tests run
sequentially because they share the foreground macOS application.

## Required Semantic Matrix

The audit produces a checked-in matrix mapping each risk below to its
authoritative test, layer, and remaining limitations.

| Area | Required contract |
| --- | --- |
| Definitions | Registered schema, defaults, ports, factories, and serialized form agree through one authoritative definition. |
| Graph edits | Add/remove/connect/parameter edits are atomic, validated, undoable, and produce one precise change set. |
| Persistence | Graph, curve, Envelope, and Trimesh identity/topology survive save and reload without inference or migration fallback. |
| Compilation | Valid graph changes produce the expected immutable configuration and routing plan; stale publication is rejected. |
| Runtime | Prepared execution routes exact payloads, preserves per-voice state, and performs no allocation-capable resizing. |
| DSP | Every production node has numerical semantics, meaningful edge cases, and parity coverage where Cycle 1 is authoritative. |
| Invalidation | A semantic edit rebuilds only the required configuration/runtime/preview state and changes downstream output. |
| Curve editing | Hover, selection, add/move/reshape/delete, identity, publication, and undo/redo work through native delivery. |
| Envelope editing | Phase is treated as traversal time; morph, loop/sustain, selection, parameters, and downstream gain remain coherent. |
| Trimesh editing | Topology, collision policy, vertex parameters, morphing, publication, persistence, and rendering consume one model. |
| Hosting | Editor capability, rebinding, teardown, popup composition, and OpenGL resource lifetime are safe. |
| Visual output | OS capture verifies OpenGL content and clipping where state assertions cannot prove composition. |

The matrix links to test names rather than recording assertion totals.

## Deliverables

- This TDD records implementation status, verification evidence, and any
  deliberate fault-injection results.
- `docs/TDD/cycle-v2-semantic-test-matrix.md` contains the maintained risk,
  contract, authoritative-test, classification, and residual-risk inventory.
- Focused Catch2 tags or equivalent runner filters expose graph, persistence,
  runtime/DSP, invalidation, and editor-host contracts independently.
- `scripts/test_cycle_v2_native_edit_smoke.py` remains the single native
  interaction acceptance entry point rather than being split into slow,
  state-dependent scripts.

## Proving The Tests

New critical regression tests are not accepted solely because they pass on the
current code. During implementation, deliberately perturb or fault-inject the
guarded behavior and show that the test fails for the intended reason. Examples
include suppressing publication, swapping an Envelope sample index to zero,
discarding stable selection, reversing curve-drag polarity, or bypassing
collision rejection.

Temporary mutations must never be committed. The audit report records the
mutation and the failing assertion or state transition. Where practical,
automate repeatable mutation checks; otherwise preserve concise evidence in
the TDD implementation notes.

## Audit Plan

### Slice 1: Inventory And Risk Map

- Classify every `cycle-v2/tests/*.cpp` test case and Cycle V2 automation
  fixture as retain, consolidate, replace, or delete.
- Assign each retained test to a semantic matrix row and named risk.
- Identify production behavior with no authoritative test.
- Record duplicated contracts and tests coupled to obsolete scaffolding.

### Slice 2: Domain, Graph, And Persistence Contracts

- Consolidate definition, validation, transaction, editor, serializer, and
  invalidation tests around semantic state transitions.
- Add missing identity/topology round trips and conflict/undo scenarios.
- Remove tests made redundant by stronger aggregate contracts.

### Slice 3: Runtime And DSP Contracts

- Build table-driven coverage for registered production processors without
  routing all expectations through a generic “output changed” predicate.
- Add Cycle 1/Cycle 2 parity harnesses around shared or adapted mature DSP.
- Verify downstream recomputation after parameter and model edits.
- Add prepared-execution checks for retained state and an allocation probe
  around the processing call; capacity observations alone do not prove that a
  realtime path is allocation-free.

### Slice 4: Editor And Native Interaction Contracts

- Keep deterministic model/command/host tests below the window boundary.
- Exercise node creation, connection, cable insertion, deletion, undo/redo,
  and save/reload through native authoring gestures. Spy nodes are deliberately
  excluded while their design is being reconsidered.
- Extend the single native smoke session to cover Waveshaper, Envelope, and
  Trimesh workflows and deterministic downstream audio.
- Assert hover and cursor state before mouse-down and geometry/output state
  after each gesture.
- Keep targeted OS screenshots only for OpenGL composition and clipping.

### Slice 5: Test Validation And Cleanup

- Run deliberate fault injections for every critical contract added by this
  audit.
- Delete or consolidate low-signal tests identified in the inventory.
- Publish the final semantic matrix and explicit residual-risk list.
- Ensure local commands and CI expose focused contract groups as well as the
  complete suite.

## Verification

- Configure and build the test and standalone presets with `--parallel 10` or
  higher.
- Run all retained Cycle V2 unit and integration tests.
- Run focused graph, persistence, runtime/DSP, editor-host, and invalidation
  contract groups independently.
- Run the native macOS editing acceptance test twice consecutively. Each run
  uses one persistent application process for the complete edit sequence and
  permits no manual state repair between edits.
- Inspect filtered logs first and confirm no new Cycle V2 `.ips` report.
- Use OS capture for OpenGL acceptance evidence.
- Run the recorded fault injections and confirm each critical test fails for
  its intended semantic reason.

## Completion Criteria

- Every retained test maps to a named semantic contract and distinct risk.
- Every required matrix row has an authoritative test or an explicit residual
  risk with owner and proposed boundary.
- Low-signal and redundant tests identified by the audit are removed or
  consolidated.
- Critical graph, persistence, DSP, invalidation, editor, and native gesture
  regressions have tests shown to fail under deliberate fault injection.
- Native editing covers real AppKit/JUCE delivery with no per-edit wait above
  100 ms.
- Cycle 1 parity is used wherever Cycle 1 defines the intended behavior; no
  simplified duplicate oracle is introduced.
- Test reports lead with guarded contracts and residual risks, not assertion
  or line-coverage totals.
- The semantic matrix and audit classification are checked in and remain the
  entry point for future Cycle 2 test additions.

## Implementation Results

- `cycle-v2-semantic-test-matrix.md` classifies every Catch2 source and every
  Cycle V2 fixture as retain, consolidate, support, or delete, maps it to named
  product risks, and records residual risks. Tests inherit their source
  classification except where the matrix names an explicit exception.
- Removed the malformed-triple test that blessed partial recovery at an
  obsolete string boundary. Typed curve snapshots remain authoritative and
  reject malformed state atomically.
- Removed the unused node-module role-label test, which guarded presentation
  strings with no runtime, serialization, or UI consumer contract.
- Added a compiled-graph contract that publishes a constant Waveshaper curve
  and a constant Envelope model, then verifies exact node output and exact
  downstream multiplication rather than relying on revision or “changed”
  assertions alone.
- Audio capture now constructs a fresh diagnostic executor per request. This
  makes repeated capture deterministic and prevents retained effect/Envelope
  state from masquerading as downstream recomputation after an edit.
- The presentation integration test first proves identical captures are equal,
  then proves Waveshaper, IR, and Envelope configuration revisions and output
  change after semantic commands.
- The native macOS runner now includes Envelope hover/selection stability,
  movement, publication, and downstream audio. Waveshaper and Trimesh sequences
  likewise compare deterministic downstream captures before and after edits.
- The same native session creates an Envelope from the palette, replaces a
  live attachment cable, selects and deletes the node, and proves undo/redo and
  save/reload topology. It also creates a Delay and inserts it into the
  WaveMesh-to-FFT cable, proving both replacement edges, 56-unit clearance on
  each side, and insertion undo/redo. Spy authoring is intentionally outside
  this audit.
- Native gestures use separated mouse-down and drag delivery with a bounded
  hold so AppKit does not coalesce the gesture. Curve reshape uses the mature
  explicit reshape modifier after independently proving ordinary hover and
  resize-cursor classification before mouse-down; both vertical gesture
  polarities travel beyond the controlling vertex.

## Fault-Injection Evidence

Temporary production mutations were applied and reverted before completion:

- Replacing Envelope traversal-column selection with column zero made
  `Graph audio executor applies envelope phase across traversal columns` fail
  with a maximum numerical error of `0.949391186`.
- Making Envelope configuration construction ignore published parameters made
  `Published curve edits change their node and downstream graph output` fail
  because edited Envelope samples were byte-for-byte equal to the initial
  samples.

These failures occurred at the intended semantic assertions. No temporary
mutation remains in the working tree.

## Verification Evidence

- `CycleV2` and `CycleV2_tests` build successfully with
  `cmake --build --preset standalone-debug --parallel 10`.
- The complete Cycle V2 suite passes: 244 test cases and 2,666 assertions.
  This count is recorded only as execution evidence; the maintained evidence
  of coverage is the semantic matrix.
- Focused graph-presentation and curve/downstream contracts pass independently.
- Two consecutive complete native macOS edit sessions pass. Each uses one app
  process for graph authoring plus Waveshaper, Envelope, and Trimesh workflows,
  and keeps normal post-edit settlement at 80 ms, below the 100 ms limit.
- No new `CycleV2` `.ips` crash report was produced.
