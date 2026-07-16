# Cycle 2 Concrete Curve Panels

## Status

Implemented.

Depends on:

- `cycle-v2-curve-identity-and-edit-commands.md`
- `cycle-v2-curve-panel-adapters.md`

Precedes `cycle-v2-concrete-curve-editors.md`.

## Problem

Cycle 2's remaining `Effect2DPanelBridge::EffectPanel` selects rendering,
rasterization, zoom, interaction, and Envelope behavior with `NodeKind`
branches. Methods such as `drawIrBackground()`, `drawGuideBounds()`, and
`drawWaveshaperBounds()` put domain presentation inside a compatibility
object. Moving model ownership into typed adapters did not make this panel a
valid shared abstraction.

Much of this behavior already exists in mature Cycle 1 classes:

- `WaveshaperUI::postCurveDraw()` draws Waveshaper padding and bounds.
- `IrModellerUI::preDraw()` and `postCurveDraw()` draw the spectral gradient,
  impulse response, loaded waveform, padding, and bounds.
- `GuideCurvePanel::preDraw()` draws guide padding and parameter ranges.
- `Envelope2D` owns Envelope background, logarithmic grid, scales, playback
  position, and Envelope-specific curve presentation.
- `EffectPanel`, `Panel2D`, `Interactor2D`, `FXRasterizer`, and `EnvRasterizer`
  already provide mature lower-level mechanics.

Copying those methods into four Cycle 2 branches creates two implementations
of the same visual semantics and makes future fixes diverge. Directly deriving
Cycle 2 panels from the Cycle 1 application components is also wrong: those
classes combine controls, mesh-library selection, persistence, updater and DSP
coordination, tutorials, and application singleton access.

## Goals

- Give Waveshaper, IR, Guide, and Envelope concrete domain panel classes.
- Keep node-specific drawing, rasterizer policy, zoom policy, and interaction
  policy in the corresponding domain panel.
- Extract reusable mature drawing and interaction behavior from Cycle 1 where
  both applications require identical semantics.
- Make Cycle 1 and Cycle 2 consume the same extracted core rather than
  creating a second implementation.
- Keep hosting, OpenGL lifecycle, clipping, snapshots, and native event
  forwarding domain-neutral.
- Remove `NodeKind` switches and node-specific drawing methods from shared
  panel infrastructure.

## Non-Goals

- Redesign panel visuals or gestures.
- Move editor controls, graph commands, persistence, tutorials, or DSP object
  ownership into shared panel code.
- Make Envelope a subtype of a flat effect curve.
- Introduce a universal drawing interface whose methods have different
  meanings for different nodes.
- Preserve a Cycle 2 approximation when Cycle 1 already defines the mature
  behavior.

## Reuse Boundary

### Shared library panel mechanics

Continue to share existing `Panel2D`, rendering helpers, mesh primitives, and
interactor/rasterizer machinery. Extract additional code into `lib/` only when
it is independent of Cycle application services and is consumed by both Cycle
1 and Cycle 2.

Shared drawing code receives explicit typed inputs, for example:

```cpp
struct WaveshaperPanelView {
    float padding;
};

struct GuidePanelView {
    float padding;
    float noise;
    float dcOffset;
    float phase;
};
```

IR and Envelope use their own typed views. IR drawing may receive immutable
magnitude, impulse, and optional loaded-wave views. Envelope drawing may
receive logarithmic/grid configuration, morph position, marker state, and an
optional playback position. The exact types should use existing `Buffer` and
domain types rather than copying sample arrays into generic containers.

Drawing helpers may use the panel coordinate and graphics services, but must
not fetch `MeshLibrary`, `Settings`, audio effects, graph nodes, or global
application state. Callers translate those dependencies before drawing.

### Cycle 1 adapters

The existing Cycle 1 UI classes retain their controls, persistence, tutorial,
mesh-selection, updater, and DSP responsibilities. They provide typed view
state to the extracted domain panels/drawing policies. Their visible output
and interaction remain unchanged.

### Cycle 2 adapters

Cycle 2 model adapters provide mesh/model state and editor controls provide
typed view state. Graph publication and revisions remain outside panels.
`CurvePanelHost` hosts the resulting panel without knowing its node type.

## Target Design

### Flat curve base

A small flat-curve base may own only genuinely common mechanics:

- plain `Mesh` editing through the mature 2D interactor
- `FXRasterizer` integration
- shared curve drawing and selection presentation
- host callback integration

It has no `NodeKind`, Envelope APIs, or positional effect controls.

### Waveshaper panel

`WaveshaperCurvePanel` owns Waveshaper padding, bipolar/unipolar coordinate
policy as applicable, bounds decoration, rasterizer configuration, and
Waveshaper-specific interaction limits.

### Impulse-response panel

`ImpulseResponseCurvePanel` owns IR padding, spectral/impulse/loaded-wave
background presentation, bounds, frequency mapping, and IR rasterizer policy.
Its render input is an immutable IR view, not a pointer to the Cycle 1
`IrModeller` application object.

### Guide panel

`GuideCurvePanelCore` owns guide padding, noise/DC/phase range decoration,
guide bounds, and guide curve interaction limits. It consumes typed guide
values and does not inspect Cycle 1 layer selection or Cycle 2 graph controls.

### Envelope panel

`EnvelopeCurvePanel` owns `EnvelopeMesh`/`EnvRasterizer` presentation,
logarithmic grid policy, morph projection, loop/sustain markers, playback
position decoration, selected-cube behavior, seam synchronization, and
Envelope-specific gestures. It does not inherit from the flat effect panel.

Names may follow existing repository conventions, but each concrete type must
be independently constructible without setting a node-kind discriminator.

## Ownership Rules

- Domain panels own domain presentation and interaction policy.
- Models own document state and stable identity.
- Editors own controls, layout, binding, and semantic graph publication.
- Hosts own JUCE/OpenGL lifecycle and event delivery.
- Rasterizers own curve sampling; drawing code must not reimplement sampling.
- Cycle-specific adapters translate application services into typed inputs.

No shared host or base panel may contain `switch (NodeKind)`, `if (kind ==
...)`, Envelope marker APIs, or methods named for a concrete sibling domain.

## Migration Plan

### Slice 1: Characterize parity

- Capture Cycle 1 and Cycle 2 OS-level images for all four panels using the
  same representative mesh and control state.
- Add focused behavioral probes for zoom limits, drawing inputs, hover/drag,
  curve reshape, insert/delete, and Envelope marker/morph behavior.
- Identify which current Cycle 2 drawing is an exact port, an approximation,
  or missing Cycle 1 behavior.

### Slice 2: Extract reusable Cycle 1 behavior

- Extract pure domain drawing/policy code from the Cycle 1 panel classes.
- Adapt Cycle 1 to the extracted code first and prove unchanged behavior.
- Keep application-only state translation in Cycle 1 adapters.

### Slice 3: Introduce concrete Cycle 2 panels

- Construct the Waveshaper, IR, and Guide panels over the flat curve core.
- Construct Envelope over its separate Envelope panel core.
- Supply typed model and render state through the existing adapters and host.

### Slice 4: Delete the compatibility panel

- Remove `Effect2DPanelBridge::EffectPanel`, `setNodeKind()`, and all
  node-kind drawing/rasterizer/interaction branches.
- Retain only narrowly named composition objects where call-site migration is
  still necessary.
- Remove duplicated Cycle 2 drawing once parity is established.

## Verification

- Cycle 1 and Cycle 2 use the same domain drawing/policy implementation where
  their intended visuals and semantics match.
- Concrete flat panels contain no `EnvelopeMesh`, `EnvRasterizer`, cube,
  marker, morph, or seam code.
- Envelope panel construction requires its Envelope domain dependencies and
  contains no flat-effect discriminator.
- Shared host and flat base contain no concrete node names or `NodeKind`.
- OS captures cover compact and expanded OpenGL rendering and show no node,
  cable, legend, or popup bleed.
- Native macOS/JUCE smoke tests cover insert, move, reshape, delete, parameter
  edits, Envelope morph and marker edits without synthetic direct interactor
  calls.
- Tests compare observable geometry/render inputs and semantic edits; they do
  not merely assert subclass construction or method forwarding.
- Modified DSP/visualization hot paths use `Buffer`/`VecOps` operations and do
  not add scalar standard-library math inside sample/bin/pixel loops.

## Completion Criteria

- Four concrete domain panels own their drawing and interaction policies.
- Cycle 1 and Cycle 2 share extracted mature behavior instead of duplicated
  drawing implementations.
- `Effect2DPanelBridge::EffectPanel` and its node-kind branches are deleted.
- Shared hosting contains no curve-domain drawing or state.
- Adding a new curve node requires a new domain panel and registration, not
  modification of an existing shared panel.

## Implementation Notes

- `ConcreteCurvePanels` now provides independently constructed Waveshaper,
  Guide, impulse-response, and Envelope panels. The three flat panels share
  only plain `Mesh`/`FXRasterizer` mechanics; Envelope requires an
  `EnvelopeMesh` and owns its `EnvRasterizer`, morph, marker, seam, grid, and
  cube interaction policies.
- Flat and Envelope factories are separately typed, so an Envelope panel
  cannot be constructed with a plain mesh or through a nullable dependency.
  The only remaining `NodeKind` dispatch is the flat-panel registration
  factory at the composition boundary.
- `CurvePanelDrawing` extracts the mature Waveshaper, Guide, and IR decoration
  used by both Cycle 1 and Cycle 2. Cycle 1 remains responsible for translating
  application-owned IR buffers and effect state; no Cycle application service
  leaks into the shared drawing layer.
- `Effect2DPanelBridge::EffectPanel`, `setNodeKind()`, and its rendering,
  rasterizer, zoom, and interaction branches were deleted. The bridge now
  composes a typed model adapter, concrete panel, and domain-neutral host.
- Curve presentation continues through the mature rasterizers and `Panel2D`;
  the former extra sorted-vertex drawing pass was removed rather than retained
  as a second approximation of curve geometry.
