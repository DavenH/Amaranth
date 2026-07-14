# Cycle 2 Curve Node Models And Editors

## Status

Proposed.

Depends on:

- `cycle-v2-node-definition-and-graph-model.md`
- `cycle-v2-node-canvas-architecture.md` for editor hosting

It also follows the reuse boundary in `shared-panel-renderer.md` and the
Envelope semantics in `envelope-loop-sustain-semantics.md`.

## Problem

Cycle 2 groups Envelope, Guide Curve, Impulse Response, and Waveshaper under the
Effect2D bridge and expanded editor. `Effect2DPanelBridge` uses `EnvelopeMesh`
as the backing store for every kind even though flat effect curves do not own
envelope cubes, loop/sustain markers, morph state, or `EnvRasterizer`
semantics. `Effect2DExpandedEditorComponent` then switches repeatedly on node
kind and represents unrelated controls as `firstSlider`, `secondSlider`, and
`thirdSlider`.

This is inheritance and reuse by visual resemblance rather than domain
contract. It produces a large bridge and editor, makes valid state
kind-dependent, and encourages Envelope-specific behavior to leak into flat
curve nodes.

## Goals

- Separate Envelope domain state from flat 2D curve state.
- Share panel hosting, curve editing, and rendering infrastructure through
  narrow interfaces and composition.
- Give Waveshaper, IR, Guide, and Envelope cohesive node models and named
  editors.
- Make node models authoritative for serialization, selection, validation, and
  immutable DSP configuration input.
- Reuse existing mesh, rasterizer, panel, and interactor implementations rather
  than recreating curve algorithms.
- Ensure compact preview, expanded editor, and DSP configuration consume the
  same node-model revision.

## Non-Goals

- Replace `Mesh`, `EnvelopeMesh`, `FXRasterizer`, `EnvRasterizer`, `Panel2D`, or
  established curve evaluation.
- Force all curve-like nodes to share one concrete model.
- Make Envelope loop, sustain, release, or morph semantics available to flat
  effect curves.
- Redesign the visual appearance or user-facing controls during extraction.
- Duplicate Cycle 1 curve or effect behavior in Cycle 2 UI code.

## Domain Models

### FlatCurveModel

Owns only the state common to a flat editable curve:

- ordered vertex identities and values
- curve/sharpness values
- selection expressed by stable vertex identity
- model revision
- validation and snapshot conversion

Its backing domain object is `Mesh`, not `EnvelopeMesh`. It does not know about
looping, sustain, release, envelope cubes, morph axes, or node effect controls.

### EnvelopeNodeModel

Owns:

- `EnvelopeMesh` and cube topology
- loop and sustain markers
- morph coordinates and linked axes
- view-axis state only when that state is document intent rather than transient
  editor state
- selection by stable vertex/cube identity
- validated `EnvelopeMeshState` conversion and model revision

Envelope playback remains in `EnvelopeSignalProcessor` and shared envelope DSP
state. The editor model must not become the audio state machine.

### Node-Specific Configuration Models

Waveshaper, IR, and Guide each compose a `FlatCurveModel` with typed controls:

```text
WaveshaperNodeModel
  - FlatCurveModel
  - enabled, preGain, postGain, oversampling

ImpulseResponseNodeModel
  - FlatCurveModel
  - enabled, size, postGain, highPass

GuideCurveNodeModel
  - FlatCurveModel
  - enabled, noise, dcOffset, phase

EnvelopeNodeModel
  - EnvelopeMesh domain state
  - logarithmic, morph, links, markers
```

Control names and constraints come from the node parameter schema. Models do
not use positional names such as `firstControlValue`.

## Shared Panel Infrastructure

Extract the reusable edge infrastructure from `Effect2DPanelBridge`:

- `CurvePanelEnvironment`: application settings and shared panel services
- `CurvePanelHost`: JUCE/OpenGL component and lifecycle hosting
- `CurvePanelRenderer`: context-bound renderer ownership and image capture
- `CurvePanelInteractionAdapter`: maps existing `Interactor2D` behavior to
  semantic model edit commands
- `CurvePanelSnapshotCache`: thread-safe rendered image publication where
  required

These services depend on a narrow panel/model adapter. They must not own an
`EnvelopeMesh` merely because one client is an Envelope.

Use separate concrete adapters:

- `FlatCurvePanelAdapter` using `Mesh` and `FXRasterizer` or the authoritative
  node-specific rasterizer
- `EnvelopePanelAdapter` using `EnvelopeMesh` and `EnvRasterizer`

Do not create a shared adapter method whose meaning changes by node kind.

## Editors

Create cohesive editors registered by node definition:

- `WaveshaperEditorComponent`
- `ImpulseResponseEditorComponent`
- `GuideCurveEditorComponent`
- `EnvelopeEditorComponent`

They may share layout and styling helpers, but each owns named controls, typed
bindings, and domain-specific gestures. Envelope marker and morph interaction
exists only in the Envelope editor.

Editors issue semantic graph/model commands. They do not retain a mutable copy
of `Node`, mutate bridge state, serialize it, and push arbitrary parameter
strings back through callbacks.

Long-running drags use one command transaction and publish intermediate model
revisions for preview without creating an undo entry per mouse event.

## Model Publication And Synchronization

The graph document owns node-model snapshots. Editor-side mutable model state
is either:

- a transaction-local working copy committed atomically, or
- a document-owned model edited through commands with coalesced undo

Compact preview, expanded panel, DSP configuration building, and serialization
observe explicit model revisions. String comparison of serialized snapshots is
not the invalidation mechanism.

Immutable DSP configuration construction consumes a validated domain snapshot
off the audio thread. Editors and previews never share mutable rasterizer or
mesh state with realtime processing.

## Serialization

Each node model defines a typed snapshot codec and version. The graph serializer
stores the opaque node-model payload with its model version but delegates
parsing and migration to the owning node definition/model codec.

Legacy `effect2d.mesh` and `envelope.snapshot` strings receive explicit
migrations. Empty or malformed state fails validation or uses a documented
node default during migration; processing must not discover malformed state.

## Migration Plan

### Slice 1: Characterization And Interfaces

- Characterize existing default curves, control mappings, selection,
  interaction, rendered previews, and serialized snapshots for all four nodes.
- Define the flat-curve and Envelope model boundaries.
- Identify the exact existing panel/rasterizer/interactor implementations used
  by each node.

### Slice 2: Flat Curve Extraction

- Introduce `FlatCurveModel` backed by `Mesh`.
- Move Waveshaper, IR, and Guide snapshot parsing and selection into it.
- Adapt their current panel rendering without changing visuals.

### Slice 3: Envelope Extraction

- Introduce `EnvelopeNodeModel` backed by `EnvelopeMesh`.
- Move loop/sustain, morph, links, view-axis, and logarithmic behavior out of
  the generic Effect2D bridge/editor.
- Preserve the existing envelope semantic tests.

### Slice 4: Shared Host Composition

- Extract panel environment, GL host, renderer, interaction, and image-cache
  services.
- Add flat-curve and Envelope adapters.
- Remove node-kind conditionals from shared host infrastructure.

### Slice 5: Per-Node Editors

- Create named editors and typed control bindings.
- Register editor factories with node definitions.
- Route edits through graph/model commands and remove generic positional
  controls.

### Slice 6: Publication And Cleanup

- Make preview and DSP configuration consume typed model revisions.
- Add explicit legacy model migrations.
- Delete `Effect2DPanelBridge` and `Effect2DExpandedEditorComponent` after all
  clients migrate, or retain only a narrowly named host class if meaningful
  shared infrastructure remains.

## Verification

### Model Tests

- flat curves cannot contain Envelope-only state
- Envelope topology, loop, sustain, morph, link, and logarithmic state
  round-trip through typed snapshots
- stable selection survives non-destructive model publication
- invalid model edits fail atomically
- legacy snapshots migrate to equivalent typed models

### Editor Tests

- each editor exposes only controls valid for its node definition
- named control bindings map to the correct typed parameters
- drag gestures create one undoable command
- selection, curve editing, marker toggling, and morph editing preserve current
  behavior

### Rendering And DSP Tests

- compact and expanded views use the current model revision
- before/after screenshot fixtures preserve existing visuals
- Waveshaper and IR prepared DSP data comes from the same curve snapshot shown
  by the editor
- Envelope preview and audio use independent mutable rasterizer/playback state
- no UI object or mutable mesh is accessed from realtime processing

## Completion Criteria

- Flat curve nodes are backed by `Mesh`; Envelope alone owns `EnvelopeMesh`.
- Shared panel infrastructure has no node-kind switch and no Envelope-specific
  state.
- Each node has a cohesive model and named editor controls.
- Editors publish semantic model commands rather than arbitrary string updates.
- Preview, serialization, and DSP configuration identify the same typed model
  revision.

