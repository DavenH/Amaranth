# Cycle V2 Architecture Quality Review

## Status

In progress, 2026-09-18. Graph mutation ownership has one completed extraction
slice; the UI validation and `NodeGraph` ownership criteria remain open.

## Context and baseline

The merged causal-update work extracted `PresentationRefreshScheduler`,
`PresentationPreviewRenderer`, `PresentationUpdateRequestBuilder`, and more
gesture ownership. `GraphPresentationModel.cpp` is now 542 lines. The causal
update TDD remains **in progress**: caller-side refresh choices and the broad
editor refresh API still exist. Finish those deletion targets in
[`cycle-v2-causal-update-graph.md`](cycle-v2-causal-update-graph.md), rather than
implementing a second policy here.

Current physical line counts (`wc -l`, 2026-09-18):

| Source | Lines | Main concern |
| --- | ---: | --- |
| `UI/NodeCanvas.cpp` | 2,572 | Canvas, gestures, document, editor host, automation, and refresh coordination |
| `App/CycleV2Automation.cpp` | 2,012 | Protocol, routing, UI actions, inspection, and assertions |
| `Graph/GraphCompiler.cpp` | 1,559 | Size trigger; inspect cohesion before changing it |
| `Graph/GraphSerializer.cpp` | 1,217 | Size trigger; inspect cohesion before changing it |
| `Runtime/GraphAudioExecutor.cpp` | 1,207 | Processing modes, preparation, cache, diagnostics, and voice execution |
| `Runtime/SpectralOscillatorFrameRenderer.cpp` | 828 | Source operations, transforms, combining, and output |
| `Graph/NodeGraph.h` | 479 | Topology, resources, probes, guides, indexes, and overlays in one aggregate |

`python3 scripts/cycle_v2_architecture_audit.py` reports 22 size-triggered
files among 430 Cycle V2 C++ sources and headers at this baseline. The table
lists reviewed concerns plus the largest unreviewed triggers; it is not a
finding against every flagged file.

File size is evidence to inspect, not a mandate to create one file per method.
The compiler and serializer are listed as triggers, not yet diagnosed as failed
abstractions. Count public roles, duplicated decisions, and dependency direction
before proposing their extraction.

## Ownership and boundaries

- `GraphCommandDispatcher` is the authoritative semantic mutation and durable
  gesture boundary; `GraphEditor` provides lower-level graph edits. Commands
  must return consolidated `GraphChangeSet` facts so wrappers do not infer
  invalidation policy independently. Keep UI callers outside direct graph
  mutation. `GraphTransaction` currently has test callers only; decide whether
  to delete it or make it the production transaction primitive before further
  extension.
- `NodeUpdateGraph` owns causal dependency planning. The shared presentation
  session and scheduler own gesture and publication lifecycle. Domain editors
  supply semantic deltas and local product inputs. See the causal-update TDD
  for its remaining migration and deletion work.
- Existing Cycle 1 DSP, Trimesh rasterization, and node-domain edit rules remain
  authoritative. Extract ownership around them; do not copy their behavior.

## Refactor slices

### 1. Narrow graph mutation and graph aggregate ownership

`GraphCommandDispatcher.cpp` annotates success and assigns topology, layout,
and probe change flags per wrapper. `GraphTransaction.cpp` separately clones a
candidate graph and maintains another change accumulator; it has no production
caller. `NodeGraph.h` holds topology, Guide curves/heatmaps/assignments, probes,
audio resources/bindings, their indexes, and transient overlay views.

Design a small semantic result contract per command family, then centralize
change-set merge and publication in the dispatcher. Separate domain stores
behind `NodeGraph` only where doing so removes coupled invariants or copies;
preserve its stable read API while migrating. Remove the unused transaction
abstraction unless it becomes the single production owner. Replace UI
speculative `GraphEditor` calls with a read-only connection/splice validation
service that reuses the same domain rules.

Completion evidence: one production transaction owner; no UI `GraphEditor`
calls; no second change-flag accumulator; operation-count tests show unrelated
resources are not copied for local gestures; the graph's public mutation
surface and index ownership are materially narrower.

First slice: removed the test-only `GraphTransaction` implementation and
retargeted its two semantic tests to the production dispatcher compound edit.
Moved topology, probe, and Guide change facts from dispatcher wrappers into
`GraphEditor` results. `GraphCurveStateCommands` now retains the Guide model
result rather than overwriting its change flags. `GraphCommandDispatcher.cpp`
fell from 588 to 484 lines; `GraphEditor.cpp` rose from 968 to 1,035 lines.
This is a policy ownership improvement, not completion of this slice: the
editor still needs domain composition, UI speculative validation still calls
it directly, and `NodeGraph` still owns all collections and indexes. Focused
topology, Guide, probe, and compound transaction tests pass. The broad `[graph]`
run has eight serializer/preset-fixture failures outside the changed command
paths; output is in `/private/tmp/cycle-v2-graph-tests.log`.

### 2. Reduce UI coordination surfaces

`NodeCanvas` inherits component, OpenGL, timer, editor presentation/resources,
expanded-editor delegate, and invalidation roles. It also owns document,
commands, presentation, automation, rendering, dock, and per-domain gestures.
Move remaining domain gesture state and editor lifecycle behind their existing
collaborators, and narrow the host interfaces to the actions each client uses.
`CycleV2Automation::runCommand` is a single conditional router over unrelated
command families. Split protocol/transport, command registry, assertions, and
UI-facing domain handlers while retaining one external automation schema.

Completion evidence: a new gesture or automation command no longer requires
adding state and dispatch branches to both central classes; production sizes
fall for the original files; existing focused UI automation fixtures retain
their observable behavior. The causal TDD owns removal of refresh host calls.

### 3. Separate runtime execution policies

`GraphAudioExecutor::processInternal` takes diagnostics, observer, dirty-node,
incremental-result, pass, and traversal controls together. Extract preparation
and cache ownership, execution-mode orchestration, and diagnostic capture so
the realtime path has a narrow prepared contract. Preserve processor identity,
buffer ownership, and no-allocation audio-thread behavior. The existing
realtime-storage refactor note remains a separate prerequisite where relevant.

`SpectralOscillatorFrameRenderer` combines source rendering, FFT/IFFT,
operation execution, and final output in one frame method. Compare its
`supports`/`prepare` graph-role policy with
`ChainedOscillatorRecipeRenderer`; move only truly shared eligibility and
planning into one region planner. Keep distinct renderer behavior distinct.

Completion evidence: execution modes have explicit inputs rather than
nullable policy arguments; no duplicated region eligibility decisions;
operation-count and audio-parity tests preserve preparation, rendering,
capture, and realtime allocation contracts. Original orchestration files
shrink as behavior moves to cohesive owners.

### 4. Correct node-domain dependency direction

`TrimeshNodeModel::renderGrid` constructs blockwise/gridwise DSP processors and
applies `TrimeshRenderProfile`. Move this presentation work into a Trimesh
service that reads model state and delegates to the existing DSP. `GraphValidator`
imports `TrimeshGuideAttachmentTarget` from `Nodes/Trimesh/Editor`, although
that helper now handles Envelope targets. Put the shared target contract in
the Guide domain and keep family-specific selection lookup with each editor.

Completion evidence: model code has no presentation/DSP construction;
graph validation has no editor include; Trimesh and Envelope guide attachment
semantics and render products remain covered by focused tests.

## Measurement and exit criteria

For each slice, record `wc -l` for touched production files, the number of
callers that decide its policy, public methods/roles removed, dependencies
removed, and the semantic tests or operation counters used. A smaller source
file alone is insufficient: the old path and duplicate decisions must be
deleted. Re-run the repository's architecture review triggers after each slice
and update this TDD until every slice is implemented or explicitly superseded.
