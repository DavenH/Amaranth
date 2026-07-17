# Cycle V2 Style And Reuse Audit

## Status

Proposed cleanup sequence.

## Audit Principle

Repeated mechanics should be factored until the meaningful domain variant is
the visually dominant code. The goal is not minimum line count. The goal is to
stop setup, lookup, transaction, publication, traversal, and teardown boilerplate
from competing with the semantics a reviewer needs to see.

This audit covers `cycle-v2/src` against `docs/style-guide.md`. It separates
semantic style failures from mechanical formatting so that formatting churn
does not disguise architectural work.

## Priority 0: Divergent Duplicate Behavior

### One Graph Edge Validator

`GraphValidator.cpp:73-181` and `GraphValidator.cpp:191-335` implement the
edge-validation state machine twice: once to accumulate issues and once to
return the first issue. Missing nodes/ports, attachments, domains, layouts,
pitch rules, and Trimesh attachment behavior can drift independently.

Extract one authoritative edge validator. Bulk validation and single-edge
queries should differ only in issue collection policy. The graph rule being
checked then becomes visible instead of the duplicated traversal mechanics.

### One Trimesh Control Interaction Owner

`TrimeshExpandedEditorComponent.cpp:142-239` and
`TrimeshControlsComponent.cpp:171-287` independently hit-test and dispatch
primary-axis, link, morph, vertex, guide, and selection gestures. They also own
parallel drag-target state.

Make `TrimeshControlsComponent` the cohesive control interaction owner and
remove editor-level duplication. The visible variants should be the domain
commands, not two copies of gesture routing.

### Replace The NodeCanvas Switchboard

`NodeCanvas.cpp` is 5,473 lines and owns palette behavior, kind parsing,
preview policies, compact node editors, expanded-editor selection, automation
encoding, graph authoring, persistence, and canvas events. Node-kind branches
appear throughout the file rather than behind the existing module/registry
boundaries.

Extract cohesive `NodePalette`, compact editor modules, preview renderer
registrations, and automation inspection/export. Keep `NodeCanvas` responsible
for viewport/canvas events, selection, and graph command coordination. Do not
replace it with another universal schema or kind switchboard.

## Priority 1: Repeated Mechanics Hide DSP Semantics

### Delete Parallel Legacy And Prepared DSP Paths

IR and Reverb in `EffectSignalProcessors.cpp`, Waveshaper in
`WaveshaperSignalProcessor.cpp`, and Envelope in
`EnvelopeSignalProcessor.cpp` maintain parallel member-state and published
configuration paths. Processing repeatedly branches on which representation
is active. Cycle V2 is net-new; this is ambiguity, not compatibility.

Use one immutable prepared-configuration source. After deletion, the visible
variants should be IR impulse/convolution, Reverb kernel/mix, Waveshaper
transfer/oversampling, and Envelope lifecycle/rendering.

### Share Prepared Convolver Lifecycle

IR (`EffectSignalProcessors.cpp:107-181,233-255`) and Reverb
(`EffectSignalProcessors.cpp:370-436,461-485`) repeat block/traversal processor
pairs, active selection, prepared sizes, revision adoption, and scratch-output
mechanics.

Extract only the prepared processor-pair lifecycle. Keep convolution
initialization and post-processing in concrete operations so the IR versus
Reverb semantics remain obvious.

### Share Flat-Curve Rasterization Preparation

IR (`EffectSignalProcessors.cpp:43-72,257-274`) and Waveshaper
(`WaveshaperSignalProcessor.cpp:38-65,151-183`) repeat flat-vertex decoding,
Mesh ownership, curve sharpness, rasterizer setup, and teardown.

Introduce one owning flat-curve preparation primitive. Padding/scaling and the
resulting product—impulse versus transfer table—remain explicit variants.

### Share Envelope Configuration Preparation

`EnvelopeSignalProcessor.cpp:22-61` and `:110-141` duplicate EnvelopeMesh
creation, snapshot application, EnvRasterizer preparation, validation, and
configuration copying for static and dynamic morph positions.

Extract an Envelope configuration factory taking the snapshot and explicit
morph/metadata. Static and live preparation then differ only in the source of
those values.

### Share Trimesh Prepared Topology And Rendering Mechanics

`NodeAudioProcessor.cpp:347-536` and `NodePreviewProcessor.cpp:270-381`
independently own topology snapshots, Mesh rebuilding, and block/grid renderer
configuration. `TrimeshGridwiseDsp.cpp:11-77` repeats its column-rendering
pipeline three times for different destination ownership.

Use a shared non-realtime prepared-topology owner and one internal column-range
renderer. Audio smoothing/live control and stateless graphic morph requests
remain the important variants.

### Share FFT Traversal Column Plumbing

`FftSignalProcessor.cpp:77-118` and `:153-195` repeat traversal-grid setup,
column/block copying, transform reset, execution, and grid output copying.

Use a small typed column reader/writer contract rather than generic callback
soup. Forward magnitude/phase and inverse optional-phase behavior should be
the visible variants. Replace generic STL copying with Buffer/VecOps where the
hot-path contract allows it.

## Priority 1: Repeated UI Lifecycle

### Curve Control Binding And Discrete Edits

Waveshaper, IR, and Guide editor constructors each recreate publish/repaint and
begin/commit lambdas. Envelope repeats them and contains four nearly identical
click transactions. `CurveEditorPrimitives.h` currently exposes three
positional `std::function` arguments.

Introduce a narrow curve-edit action interface or scoped binding owned by
`CurveExpandedEditorComponent`:

- continuous controls bind transaction start/end and publication once;
- discrete controls run `begin -> operation -> publish -> commit -> repaint`;
- concrete call sites state only operations such as toggle loop, toggle
  sustain, or set logarithmic mode.

Do not create a universal control schema. Concrete editors should retain their
domain controls and layout.

### Trimesh Panel Host Delegate

Repaint, cursor, and hover lifecycle travels as three callbacks through
`TrimeshWidget`, `TrimeshPanelBridge`, and `TrimeshPanelHosts`, with three
SafePointer lambdas constructed by the expanded editor.

Replace the positional callback bundle with a narrow non-owning
`TrimeshPanelHostDelegate`. Mesh-edit completion remains a separate semantic
event sink.

### Split ConcreteCurvePanels And NodeEditorHost

`ConcreteCurvePanels.cpp` is 1,461 lines. Its Envelope panel mixes selection,
links, loop/sustain policy, raster refresh, drawing, and serialization.
`NodeEditorHost.cpp` combines concrete adapters, registry, automation encoding,
command service, and hosting lifecycle.

Split by cohesive ownership. Preserve the mature Interactor implementations;
extract policies and adapters around them rather than reimplementing
interaction algorithms.

## Priority 2: Graph And Runtime Cost/Visibility

- `GraphDomainResolver.cpp:85-360` contains parallel recursive/iterative
  algorithms and repeated edge scans with roughly O(E^3) worst-case behavior.
  Use indexed adjacency plus one worklist propagation algorithm; node transfer
  semantics become the variant.
- `GraphAudioExecutor.cpp:239-383` repeatedly linearly searches processor and
  voice preparation state. Introduce a keyed prepared-processor store and a
  `PreparationSignature` value.
- `NodeDefinition.cpp:200-419` repeatedly scans definitions while building a
  multi-phase registry. Assemble authoritative descriptors once through
  domain-family builders.
- `GraphCommandDispatcher.cpp:52-153` repeats apply/edit/change annotation.
  Extract a narrow edit decorator while leaving curve publication separate.
- `GraphEditor.cpp:302-361` normalizes and upserts parameter patches with
  repeated linear scans. Use keyed normalization at this explicit publication
  boundary.
- `TrimeshNodeModel.cpp:324-359` repeats revision fan-out. Represent affected
  derived products explicitly without hiding invalidation semantics.

## Mechanical Style Pass

Perform mechanical cleanup only alongside the owning semantic extraction:

- expand compressed registrations in `NodeAudioProcessor.cpp:544-570` and
  `NodePreviewProcessor.cpp:385-404`;
- expand multi-step one-line definitions in
  `ConcreteCurvePanels.cpp:86,131,146-147,467-469,519,1166` and
  `CurveExpandedEditorComponent.cpp:138-141`;
- split mixed-domain processor factory translation units;
- keep one declaration and one executable statement per line;
- separate setup, shared lifecycle, and domain operation with blank lines;
- make realtime prepared-capacity invariants explicit where processing paths
  currently call resize/prepare fallbacks.

Do not mechanically vectorize sequential sampler lookup in
`TrimeshBlockwiseDsp`, configuration-time `std::pow`, or once-per-block
Envelope comparisons. Those are not hot-loop style violations.

## Suggested Sequence

1. Unify GraphValidator and Trimesh control interaction; add parity tests.
2. Add curve edit bindings and the Trimesh host delegate.
3. Delete legacy DSP paths before extracting shared preparation mechanics.
4. Extract convolver, flat-curve, Envelope, Trimesh, and FFT repeated cores.
5. Split NodeCanvas, concrete panels, editor host, and processor factories.
6. Address domain-resolution and keyed runtime/parameter lookup costs.
7. Run the mechanical style pass on the now-cohesive files.

Each slice follows design -> implementation -> refactor -> style check ->
semantic verification -> commit, repeating until its TDD is complete.
