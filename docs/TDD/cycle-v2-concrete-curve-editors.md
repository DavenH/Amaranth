# Cycle 2 Concrete Curve Editors

## Status

Implemented 2026-07-14.

Depends on `cycle-v2-curve-state-publication.md`.

## Problem

The named Waveshaper, IR, Guide, and Envelope editor classes are currently
empty subclasses of `Effect2DExpandedEditorComponent`. The generic component
still owns positional sliders, node-kind branches, node-specific parameter
IDs, Envelope interaction state, layout, and publication behavior.

The names improve construction but do not yet provide cohesive editors or an
open/closed extension point.

## Goals

- Give each curve node a concrete editor with named controls and bindings.
- Keep only genuine panel hosting and common editor chrome in shared code.
- Remove node-kind switches and positional controls from shared editor code.
- Bind editors to typed node models and one semantic publication interface.
- Preserve current visuals, gestures, automation semantics, and accessibility
  labels.

## Non-Goals

- Redesign the editors visually.
- Replace mesh panels, interactors, or rendering.
- Split the panel bridge; that is covered by
  `cycle-v2-curve-panel-adapters.md`.

## Target Design

Shared infrastructure may contain:

- close/header chrome
- panel-host placement
- OpenGL render delegation
- common transaction/publication service
- generic snapshot and repaint plumbing

Concrete editors own named controls and node behavior:

- `WaveshaperEditorComponent`: enabled, pre-gain, post-gain, oversampling
- `ImpulseResponseEditorComponent`: enabled, size, post-gain, high-pass
- `GuideCurveEditorComponent`: enabled, noise, DC offset, phase
- `EnvelopeEditorComponent`: morph plane, axes, links, logarithmic mode,
  loop/sustain markers, and selected-cube controls

Prefer composition with a small `CurveEditorHost` over inheritance if the
shared component would otherwise require virtual node-specific hooks.

## Binding Contract

Control bindings use named typed fields, not ordinal positions or string IDs
embedded in shared UI code. The graph-facing boundary accepts the complete
typed publication described by `cycle-v2-curve-state-publication.md`.

Automation state exposes semantic keys such as `preGain`, `highPass`, or
`redMorph`; compatibility aliases may be retained temporarily with explicit
deprecation tests.

## Migration Plan

### Slice 1: Shared Host Boundary

- Extract common header, close handling, panel bounds, and rendering host.
- Define the typed publication and transaction dependency supplied to editors.

### Slice 2: Flat Curve Editors

- Move Waveshaper, IR, and Guide controls, layout, and bindings into their
  concrete editors.
- Delete corresponding kind branches and positional control use.

### Slice 3: Envelope Editor

- Move Envelope axes, markers, morph controls, selected-cube interaction, and
  layout into `EnvelopeEditorComponent`.
- Remove all Envelope state from shared editor infrastructure.

### Slice 4: Cleanup

- Delete `Effect2DExpandedEditorComponent`, or retain only a narrowly named
  host with no node-kind knowledge.
- Replace static `controlIds()` tests with behavioral binding tests.

## Verification

- Each editor exposes only controls valid for its node schema.
- Each named control changes the intended typed field.
- No shared editor class switches on `NodeKind`.
- No production control is named first/second/third.
- Drag, marker, link, morph, and curve gestures preserve undo behavior.
- Existing focused automation fixtures pass for all four editors.
- OS-level captures preserve OpenGL panels, surrounding cables, and legend.

## Completion Criteria

- Concrete editor classes contain their real behavior and layout.
- Shared editor infrastructure knows no curve node kinds or parameter IDs.
- Adding another curve editor does not require changing an existing editor or
  shared host.

## Implementation Notes

- `Effect2DExpandedEditorComponent` is now a 231-line domain-neutral host. It
  owns only popup chrome, panel placement, transaction/publication plumbing,
  and OpenGL repaint delegation; it contains no `NodeKind` branch or curve
  parameter ID.
- Waveshaper, impulse-response, guide, and Envelope controls, typed bindings,
  layout, automation names, and interaction state live in their concrete
  editor classes.
- The positional `firstSlider`/`secondSlider`/`thirdSlider` production API and
  its hard-coded ID-array test were removed. Typed model binding tests and the
  native event-delivery smoke cover semantic behavior instead.
- The editor factory remains the composition root that registers node kinds;
  adding an editor does not modify the shared host or any existing editor.

## Verification Results

- `cmake --build --preset tests --parallel 10`
- `cmake --build --preset standalone-debug --parallel 10`
- `CycleV2_tests "[cycle-v2][curve-model]"`: 156 assertions in 17 cases
- `cycle-v2-agent-effect2d-panels.json`: all four concrete editors opened and
  Waveshaper/IR panel gestures completed without an assertion or crash.
- The native edit smoke completed its Effect2D sequence, then exposed an
  unrelated open Trimesh deletion regression recorded in `ui-bugs.md`.
