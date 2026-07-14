# Cycle 2 Curve Panel Adapters

## Status

Proposed.

Depends on `cycle-v2-curve-identity-and-edit-commands.md`.

May proceed alongside `cycle-v2-concrete-curve-editors.md` once the shared
identity and publication contracts are stable.

## Problem

`Effect2DPanelBridge` now gives flat curves a `Mesh` and Envelope an
`EnvelopeMesh`, but it remains a large compatibility object with nullable
flat/Envelope model combinations and node-kind branches across
synchronization, serialization, interaction, automation, rendering, and
preview extraction.

The object can represent invalid combinations and continues to couple
Envelope-only behavior to flat panel hosting.

## Goals

- Separate flat-curve and Envelope panel adaptation.
- Keep OpenGL lifecycle, panel hosting, clipping, and image snapshot services
  shared through narrow composition interfaces.
- Make invalid mesh/model combinations unrepresentable.
- Let models own document state and adapters own only UI translation.
- Preserve the mature panel, interactor, rasterizer, and OS capture behavior.

## Non-Goals

- Reimplement curve rendering or interaction.
- Merge flat and Envelope domain models.
- Change popup compositing, cable layering, or visual design.

## Target Boundaries

### CurvePanelHost

Owns context-bound panel hosting, OpenGL resource lifecycle, clipping,
snapshot capture, repaint callbacks, and mouse-event forwarding. It does not
own a curve model and does not switch on node kind.

### FlatCurvePanelAdapter

Owns the mapping between `FlatCurveModel` identities and a plain `Mesh`, flat
curve preview extraction, and flat interactor notifications. It cannot access
Envelope cubes, markers, morph axes, or `EnvRasterizer` state.

### EnvelopePanelAdapter

Owns the mapping between `EnvelopeNodeModel` and `EnvelopeMesh`, marker and
selected-cube behavior, morph/view inputs required by the mature Envelope
panel, and Envelope preview extraction.

Adapters receive typed state and emit semantic edit events. They do not own
graph commands, revisions, or duplicated generic control values.

## Migration Plan

### Slice 1: Shared Host Extraction

- Move snapshot cache, host callbacks, clipping, and GL lifecycle behind a
  mesh-agnostic host contract.
- Characterize resource creation/release and popup composition with tests.

### Slice 2: Flat Adapter

- Move plain `Mesh` setup, flat preview vertices, and edit translation into
  `FlatCurvePanelAdapter`.
- Remove flat branches from the compatibility bridge.

### Slice 3: Envelope Adapter

- Move `EnvelopeMesh`, markers, morph state, and selected-cube behavior into
  `EnvelopePanelAdapter`.
- Remove nullable Envelope members from shared infrastructure.

### Slice 4: Cleanup

- Delete `Effect2DPanelBridge`, or retain only a narrowly named façade during
  call-site migration with no domain behavior.
- Remove duplicated `firstControlValue`/`secondControlValue` style state.

## Verification

- Flat adapters contain no `EnvelopeMesh`, cube, marker, or Envelope
  rasterizer references.
- Shared host code has no `NodeKind` branch.
- Envelope interaction and preview behavior remain unchanged.
- Model and adapter remain synchronized across insert/delete/move operations.
- Repeated popup open/close releases context resources safely.
- OS-level screenshots show no panel, cable, node, or legend bleed.
- Existing compact/expanded preview automation fixtures pass.

## Completion Criteria

- Flat and Envelope adapters have non-null, domain-correct construction.
- Shared panel hosting contains no domain state.
- Graph/document controls and revisions are owned outside panel adapters.
- `Effect2DPanelBridge` no longer acts as a cross-domain god object.

