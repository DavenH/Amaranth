# Cycle V2 Guide Curve Resources And Workspace Dock

## Status

Implemented (2026-08-22). Guide Curves are document resources with typed
cube-component assignments. The Guide and Spy shelves share one persistent,
resizable workspace dock, including the completed UI-design conformance pass.

This TDD supersedes the Guide Curve canvas-node and attachment-edge ownership
described in `node-graph-workflow.md`. It changes only Cycle V2 authoring and
document ownership. The mature Guide Curve evaluation, Trimesh deformation,
component baking, and panel rendering contracts remain authoritative.

Depends on:

- `cycle-v2-spy-rail.md` for passive Spy presentation and editor-safe bottom
  workspace reservation;
- `cycle-v2-trimesh-guide-curve-parity.md` for provider-backed Guide Curve DSP,
  Trimesh preparation, cube-component meaning, and rendered guide rails;
- `cycle-v2-curve-node-models-and-editors.md` for `FlatCurveModel`, the concrete
  Guide Curve editor, and shared curve-panel hosting;
- `cycle-v2-node-canvas-architecture.md` for document commands, scene geometry,
  presentation, and expanded-editor lifecycle.

## Problem

Guide Curves are shared authoring resources, but Cycle V2 presents each one as
an ordinary canvas node with an output port. Its assignments are processing
attachment edges from that port to synthetic Trimesh input ports. This
representation conflicts with the actual domain:

- a Guide Curve may be used by many Trimesh or Envelope-layer targets;
- the authoritative target is a cube component, not a whole node or a vertex;
- one vertex-menu assignment may resolve to several owning cube components;
- persistent cables from a shared curve to those fine-grained targets add
  visual noise without explaining the assignments;
- Guide nodes consume graph space even though they do not participate in the
  signal path;
- editing Guides and observing downstream Spy previews should be possible at
  the same time.

The existing passive Spy rail solves the related inspection problem by moving
non-processing objects out of the canvas. Guide Curves need a peer resource
shelf with authoring and assignment behavior, not another kind of graph node.

## Product Decisions

- Guide Curves are document-level resources, not nodes.
- Assignments are typed references from one Guide resource to one concrete
  target. They are not `Edge` values and do not have ports or cable paths.
- The target model is extensible, but the first implementation supports only
  target kinds whose semantics are already authoritative in production.
- Trimesh assignment preserves cube-component semantics. A selected vertex is
  only a convenience for resolving its owning cube components.
- Guides and Spies share one bottom workspace dock and occupy side-by-side
  shelves. Guides are on the left and Spies are on the right.
- The initial divider is 50/50 and user-adjustable. Empty and minimized are
  distinct states: an empty expanded shelf retains useful width and shows a
  vacancy placeholder; only an explicitly minimized shelf yields its width.
- The whole dock can collapse. Each shelf can also minimize to a narrow vertical
  drawer with an inward-pointing chevron, following Blender-style detail
  drawers.
- Double-clicking a Guide tile opens the existing full Guide editor above the
  dock. The Spy shelf remains visible.
- Guide creation is available from the Guide shelf and from assignment menus.
  Guide is removed from the node palette.
- Selector menus are the authoritative assignment gesture. Dragging a Guide
  onto an editor is deferred until all target-field drop semantics are designed.
- Guide relationships never have persistent cables. A temporary tether may
  show one active relationship only while its tile or target is hovered.
- Guide order, stable labels, names, colours, models, parameters, and
  assignments are document content. Dock height, divider position, minimized
  states, scroll offsets, selection, and global collapse are application UI
  state.
- Generated short labels remain available in assignment menus and target
  badges, but unnamed shelf tiles do not display them as titles. A user-authored
  name is the only Guide tile heading.
- Guide shelf tiles do not repeat resource colour as a dot or outline. Colour
  remains meaningful in assignment badges and temporary relationship
  presentation. The shelf has no ellipsis or resource-action popover; a tile
  is a selector and editor launcher.
- Deleting an in-use Guide reports the affected assignment count and performs
  detach-all plus deletion as one undoable semantic command.
- Cycle V2 is undeployed. Repository `.cyclegraph` files are converted directly
  to the new format. No reader adapter or migration path for Guide nodes and
  Guide attachment edges is added.

## Authoritative Implementations And Reuse Boundary

The following behavior is reused unchanged:

- `FlatCurveModel` in
  `cycle-v2/src/Nodes/Effect2D/CurveNodeModels.{h,cpp}` owns point identities,
  curve values, validation, model revision, and snapshot conversion.
- `cycle-v2/src/Nodes/Effect2D/GuideCurveEditorComponent.cpp` and the
  `FlatCurvePanelAdapter`/panel/controller own Guide point editing and the
  enabled, noise, DC-offset, and phase controls.
- `lib/src/Curve/GuideCurveTableDsp.{h,cpp}` owns padded table evaluation,
  stable noise, offset, phase, density, and lookup behavior.
- `cycle-v2/src/Nodes/Guide/GuideCurveSnapshotProvider.{h,cpp}` owns immutable
  sampled tables outside the audio callback.
- `lib/src/Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h` and
  `lib/src/Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h` own
  intercept deformation and Time/component curve baking.
- `lib/src/UI/Panels/Panel2D.{h,cpp}` and
  `lib/src/UI/Panels/Panel3D.{h,cpp}` own guide rails and assignment tags.
- `cycle-v2/src/Nodes/Trimesh/TrimeshGuidePreparation.{h,cpp}` remains the
  single translation boundary from document assignments to local provider
  slots and render-time mesh metadata.

The ownership translation is:

```text
Guide Curve node parameters/model
  -> GuideCurveResource parameters/model

Guide processing-attachment edge
  -> GuideCurveAssignment with a typed target

node editor subject
  -> Guide resource editor subject

serialized global Guide index
  -> stable resource identity, translated to a local provider slot only while
     preparing one target node
```

This boundary must not copy curve interaction, rasterization, guide evaluation,
Trimesh deformation, component baking, or panel rendering. A synthetic hidden
Guide node is not an acceptable intermediate or final ownership model.
No behavioral shared-core extraction is required by this change; the work is
an ownership, serialization, event, and lifecycle translation around the
existing cores.

## Document Model

### GuideCurveResource

`NodeGraph` owns an ordered collection of Guide resources alongside nodes,
edges, and signal probes. The concrete representation may use a dedicated
immutable resource-model state, but it must express this domain:

```cpp
struct GuideCurveResource {
    String id;
    String shortLabel;
    String name;
    int colourIndex {};
    int shelfOrder {};
    bool enabled { true };
    float noise { 0.5f };
    float dcOffset { 0.5f };
    float phase { 0.5f };
    GuideCurveModelStatePtr model;
};
```

`id` is the durable reference identity. `shortLabel` is a concise stable label
such as `G2`; it does not change when shelf order changes. `name` is editable
and may be empty. Menus and detail UI display the stable label followed by the
name when present, for example `G2 · Vibrato Bend`. `colourIndex` is also
stable across reordering so target badges do not unexpectedly change identity.

The model state composes the existing `FlatCurveModel`; it does not introduce
another point-curve representation. Resource and curve-model revisions must be
available to preparation fingerprints without serializing the resource on each
lookup.

### Typed Assignments

The assignment collection is separate from signal/configuration edges:

```cpp
enum class GuideCurveField {
    Time,
    Red,
    Blue,
    Phase,
    Amplitude,
    Curve
};

struct TrimeshCubeComponentGuideTarget {
    int cubeIndex { -1 };
    GuideCurveField field {};
};

using GuideCurveTarget = std::variant<TrimeshCubeComponentGuideTarget>;

struct GuideCurveAssignment {
    String guideId;
    String targetNodeId;
    GuideCurveTarget target;
};
```

The variant is the extension point for later authoritative target types. Do not
replace it with loosely interpreted target strings, a repeated `NodeKind`
switch, or optional fields for every imagined target. Adding another variant
requires its own target validator, serializer, preparation boundary, and tests.

Initial invariants are:

- resource IDs, short labels, and assignment target addresses are unique where
  their contract requires uniqueness;
- every assignment references an existing Guide resource and destination node;
- a Trimesh target references an existing cube index and one of the six
  authoritative fields;
- at most one Guide is assigned to a particular target address;
- one Guide may be assigned to any number of targets and nodes;
- assigning an occupied target replaces its prior Guide atomically;
- removing a destination node removes all assignments targeting it in the same
  graph edit;
- a topology edit that invalidates a cube target must reconcile or remove that
  assignment inside the same semantic graph command. It must not leave a stale
  string-like target for presentation code to tolerate.

The selected-vertex convenience resolves the vertex's owning cubes through the
existing Trimesh model and applies one assignment operation to the resulting
set of cube-component targets. It does not store a vertex-wide assignment.

## Serialization And Direct Preset Conversion

Advance the Cycle V2 graph format and serialize two required top-level arrays:

```json
{
    "guides": [
        {
            "id": "guide1",
            "shortLabel": "G1",
            "name": "",
            "colourIndex": 0,
            "shelfOrder": 0,
            "enabled": true,
            "noise": 0.0025,
            "dcOffset": 0.0,
            "phase": 0.0,
            "model": {}
        }
    ],
    "guideAssignments": [
        {
            "guideId": "guide1",
            "targetNodeId": "phaseLayer1",
            "target": {
                "kind": "trimeshCubeComponent",
                "cubeIndex": 0,
                "field": "amp"
            }
        }
    ]
}
```

The exact property order follows the canonical serializer. The format has no
Guide `Node`, Guide output port, `guide.cube.*` synthetic input port, or Guide
attachment `Edge`.

Because Cycle V2 is undeployed:

- the serializer reads and writes only the new canonical format version after
  this conversion;
- no v2-to-v3 runtime conversion recognizes Guide nodes or attachment edges;
- all repository graph resources are rewritten directly, including graphs
  without Guides because the required root schema changes;
- `scripts/port_cycle_v1_preset.py` emits resources and typed assignments
  directly and never constructs Guide nodes or Guide edges;
- tests and automation fixtures construct the new representation directly.

Conversion must preserve these shipped facts exactly:

| Graph | Guide resources | Assignments |
|---|---:|---:|
| `african-horn.cyclegraph` | 1 | 2 |
| `alto-sax.cyclegraph` | 4 | 17 |
| `baroque-flute.cyclegraph` | 3 | 11 |
| `stengah.cyclegraph` | 1 | 2 |
| `default.cyclegraph` | 1 | 0 |
| `with-spies.cyclegraph` | 1 | 0 |
| `fft-sawtooth.cyclegraph` | 0 | 0 |

The direct rewrite preserves each curve model, enabled/noise/DC/phase value,
resource identity, assignment destination, cube index, and field. Node
positions belonging only to former Guide cards are intentionally discarded.

## Runtime And Invalidation Contract

Guide resources are configuration inputs, not execution nodes:

- they have no ports, processors, execution-plan entries, audio buffers,
  latency, traversal-grid output, or canvas preview records;
- adding, renaming, or reordering an unused Guide cannot change
  audio execution or output samples;
- editing an assigned Guide invalidates every attached destination's prepared
  Trimesh configuration, compact and expanded rendering, DSP preparation, and
  all affected downstream preview/audio products;
- assigning, replacing, detaching, or deleting a Guide invalidates the affected
  destinations and downstream products through one consolidated
  `GraphEditResult`/`GraphChangeSet`;
- shelf-only presentation changes do not trigger DSP preparation;
- resource table seeds and target results do not depend on shelf order.

`TrimeshGuidePreparation` gathers resources referenced by one destination,
builds the immutable `GuideCurveSnapshotProvider`, assigns local provider slots,
and writes those slots into its render-time mesh copy. The local slot is an
implementation detail and is never serialized. Stable resource identity,
rather than vector order, is the source for deterministic per-guide behavior.

No audio-thread allocation or mutable resource sharing is introduced.

## Workspace Dock

### UI-Design Conformance Follow-Up

The first complete native presentation exposed six interaction and hierarchy
defects. The dock correction keeps the document/resource architecture and the
mature Guide and Spy preview renderers unchanged while applying these
contracts:

- the canvas provides a visible keyboard focus sequence across global dock
  collapse, shelf controls, and tiles. Forward/reverse Tab traversal, arrow
  movement within a tile strip, activation, and Spy deletion invoke the same
  semantic actions as pointer input;
- selecting a Guide may retain subdued endpoint highlighting, but only a
  current hover may draw the single relationship tether. Hover state clears
  when the pointer leaves the canvas, and selection/hover state cannot leak
  across document loads;
- global collapse, per-shelf minimize/restore, Guide creation, Spy refresh,
  and Spy tile removal use legible desktop-sized targets and distinct
  symbols. Shelf controls stay with their own shelf instead of clustering at
  the divider;
- Guide and Spy tiles share one chrome grammar: neutral inactive border,
  optional terse identity, preview region, and active focus/selection border.
  Spy domain colour and its close action remain meaningful; Guide shelf colour
  tokens and trailing actions do not;
- vacancy presentation remains visible but occupies one quiet tile slot rather
  than a large centred card. Per-tile Guide usage sublabels are removed from
  the shelf; relationship detail remains available through target highlighting;
- overflowing tile strips show edge and position feedback. Scrolling one shelf
  remains independent and keyboard tile movement keeps its focused tile in
  view.

The keyboard navigation and shared chrome are presentation collaborators, not
new domain authorities. They do not mutate `NodeGraph`, dispatch Guide/Spy
commands, render curve/signal content, or interpret assignment targets.

### Ownership

Introduce a narrow workspace-dock coordinator that owns shared bottom geometry,
clipping, the main resize handle, divider geometry, and global collapse. It
hosts two independent domain components:

- `GuideCurveShelf`, responsible for Guide resource presentation and hits;
- `SignalProbeRail`, responsible for Spy ordering, previews, markers, and Spy
  interactions.

The coordinator must not become a Guide/Spy switchboard. Shared code may own
layout primitives, headers, tile-strip scrolling, and vacancy styling; Guide
commands and Spy preview behavior remain in their respective components.

`WorkspaceDockInteractionController` is the narrow input-routing peer to that
geometry coordinator. It owns dock gesture lifetimes, focus traversal, and UI
state persistence, while delegating Guide commands, Spy authoring, editor
hosting, and both mature preview renderers to their existing authorities.

### Geometry

The dock remains at the bottom of the workspace and above all canvas and
expanded-editor content. Preserve the existing useful vertical contract:

- expanded height defaults to 190 pixels;
- minimum expanded height is 120 pixels;
- maximum height is 40 percent of workspace height;
- globally collapsed height is 34 pixels so the disclosure control retains a
  legible 28-pixel target inside the collapsed bar;
- expanded-editor JUCE bounds, OpenGL viewport/scissor, and input capture use
  the content rectangle above the dock.

When both shelves are expanded, the divider starts at 50 percent. The divider
is draggable and clamped so each shelf retains a useful minimum width. Its
ratio is application UI state.

An empty expanded shelf does not disappear and does not surrender all its
width. It contains a subdued node-shaped vacancy tile:

- Guides: `No guides` with the shelf's visible add affordance;
- Spies: `No spies`.

Vacancy labels report state only. They do not contain instructional copy.

The default divider still applies when a shelf is empty. The user may resize it
down to the shelf minimum. The peer receives effectively full width only when
the empty shelf is explicitly minimized.

Each shelf has an independent horizontal offset, wheel/trackpad hit region,
clip, and maximum scroll extent. Scrolling under one shelf must never move the
other.

### Minimized And Collapsed States

Per-shelf minimization is distinct from global collapse:

- minimizing Guides leaves a narrow vertical `GUIDES (n) >` drawer at the left
  side of the dock content and gives the Spy shelf the remaining width;
- minimizing Spies leaves a mirrored `< SPIES (n)` drawer at the right side;
- the chevron points into the area the drawer will occupy when opened;
- restoring a shelf restores its prior divider ratio and horizontal offset;
- if both shelves are minimized, the global collapsed presentation is used;
- global collapse shows both counts and any disconnected-Spy warning while
  stopping tile rendering; canvas Spy markers remain visible.

Dock height, divider ratio, global collapse, per-shelf minimization, horizontal
offsets, and currently selected Guide/Spy are application UI state and do not
dirty or serialize the graph.

## Guide Shelf Presentation

Expanded Guide tiles show:

- optional user-authored name over the preview, without a generated short-label
  heading or reserved header row;
- curve thumbnail rendered through the existing OpenGL flat-curve presentation
  path across the available tile, including its grid, fill, vertices, and
  modulation trace;
- neutral selection and hover state, plus the shared keyboard focus treatment.

The tile is an organizer and launcher, not a miniature parameter editor. Noise,
DC offset, phase, enable, and point editing remain in the full Guide editor.

Single-click selects a Guide and highlights its uses. Double-click opens its
full editor above the dock. The shelf does not open a resource-action popup.
Reordering commands change `shelfOrder` and do not change short labels,
colours, assignments, provider seeds, or sound.

An optional usage detail for the selected Guide groups targets by owning node
and field, for example:

```text
Used by
Magnitude Layer 1 · Amp ×3
Phase Layer 1 · Time ×2
```

Activating a usage selects its node and, when the editor supports it, selects
or reveals the relevant cube/component.

## Assignment Workflow

Trimesh component controls retain the authoritative selector menu. For a
selected vertex and field, it contains:

- `None` or `Detach` when any resolved target is assigned;
- `New Guide…`;
- every Guide as `short label · name`, with stable colour and current
  assignment state.

`New Guide…` creates a resource and assigns it to all resolved owning
cube-component targets in one undoable command. Selecting an existing Guide
replaces assignments for the same target set atomically. Detach removes the
target-set assignments without deleting the resource.

Target controls and mature panel assignment tags use the Guide's stable label
and colour. Relationships are disclosed without persistent cables:

- hovering or selecting a target highlights its Guide tile;
- hovering or selecting a Guide highlights all visible target badges;
- a temporary tether is drawn only for one hovered tile-to-target relationship;
- hovering a shared Guide does not fan out several tethers across the canvas;
- the usage list provides navigation when not all consumers are visible.

The canvas may show a small Guide-usage badge/count on affected nodes, but it
must not recreate source sockets, destination ports, or cable routing.

## Guide Editor Hosting And Commands

The full Guide editor binds directly to a stable resource ID. It must not create
a temporary `Node` or mutate a copied resource and push arbitrary JSON back at
close time.

The existing curve panel, controller, renderer, and `GuideCurveEditorComponent`
are adapted at their ownership boundary to obtain:

- the current immutable Guide model and parameters;
- semantic resource-model and resource-parameter commands;
- begin/update/commit/cancel gesture lifecycle;
- document and presentation refresh callbacks.

The generic expanded-editor host may accept a node or resource subject if that
is purely a lifecycle/bounds distinction. It must delegate construction and
domain behavior to separate node and Guide factories rather than accumulating
kind-specific branches.

A Guide gesture captures its durable document base revision once. Every live
publication retains that base; only `GraphCommandDispatcher` translates it to
the current transient resource snapshot. Commit produces one undo record.
Cancel restores the pre-gesture resource and every dependent destination.

Spy tiles remain visible and refresh according to the existing `On Release` or
`Live` policy while a Guide editor is open.

## Deferred Drag-And-Drop Direction

Guide-tile dragging is deliberately outside the initial implementation. A
future design may:

- highlight an eligible rendered Guide rail beneath a pointer in the Trimesh
  3D editor and assign on drop;
- highlight the component-curve region in the 2D editor for a Time/component
  assignment;
- expose explicit drop targets for red, blue, phase, amplitude, curve, and
  other component fields.

There is not yet one unambiguous spatial mapping for all six fields. Do not
ship a partial drag gesture that makes 3D rail or component-curve assignment
easy while hiding sharpness/curve or other component assignments. The selector
menu remains the complete workflow until every eligible field has a legible
drop target and one typed semantic command.

## Implementation Slices

Each slice receives its own refactor/style/test pass and coherent commit before
continuing.

### Slice 1: Resource And Assignment Domain

- Add Guide resources and typed assignments to `NodeGraph`.
- Add validation, stable identity lookup, target replacement, destination
  cleanup, and assignment queries.
- Move/rename the Guide-specific configuration wrapper so it composes
  `FlatCurveModel` without depending on `Node` or `NodeDefinition`.
- Add semantic dispatcher commands for create, rename, reorder, duplicate,
  edit, assign, detach, and detach-all/delete.

### Slice 2: Serialization And Repository Conversion

- Advance the graph format and encode required `guides` and
  `guideAssignments` arrays.
- Rewrite all seven repository `.cyclegraph` resources directly.
- Change `port_cycle_v1_preset.py` to emit the new schema.
- Rewrite tests and fixtures rather than adding a Guide-node compatibility
  reader.

### Slice 3: Provider And Trimesh Preparation

- Make `GuideCurveSnapshotProvider` consume immutable resources.
- Make `TrimeshGuidePreparation` consume typed assignments and map stable Guide
  IDs to destination-local provider slots.
- Preserve all Guide table, rail, tag, component-bake, blockwise, gridwise,
  oscillator, compact-preview, and expanded-editor behavior.
- Consolidate resource and assignment invalidation into destination/downstream
  change sets.

### Slice 4: Assignment Authoring

- Convert the Trimesh attachment menu and labels to resource queries and typed
  commands.
- Add detach and replacement behavior for the complete selected-vertex owner
  set.
- Remove synthetic Guide port IDs from view modules and scene targets.
- Add stable Guide badges and one-at-a-time hover/selection tether geometry.

### Slice 5: Shared Workspace Dock

- Extract bottom workspace geometry from `SignalProbeRail` into the narrow dock
  coordinator.
- Host `GuideCurveShelf` and `SignalProbeRail` side by side.
- Implement 50/50 default layout, divider resizing, independent scrolling,
  vacancy placeholders, per-shelf drawers, global collapse, and UI-state
  restoration.
- Keep expanded editors and OpenGL content above the resulting dock bounds.

### Slice 6: Guide Shelf And Resource Editor

- Render Guide tiles using the existing flat-curve preview path.
- Implement selection, `+`, naming, ordering, duplication, usage counts, and
  guarded deletion.
- Rebind the full Guide editor and gesture lifecycle to resource commands.
- Keep Spy compact and detail previews live and visible during Guide edits.

### Slice 7: Canvas And Runtime Cleanup

- Remove Guide from the palette, node factory/definition registry, node view,
  compiler/runtime module roles, scene, cable renderer, and canvas selection.
- Delete Guide attachment-edge bundling and all Guide port/cable interaction.
- Re-read the production diff for hidden-node compatibility code, repeated
  target-kind branching, duplicated curve behavior, and mixed Guide/Spy domain
  logic.
- Update superseded TDD language, automation documentation, and shipped UI
  captures.

## Expected Change Surface And Complexity

Expected new cohesive production units are:

- a Guide resource/assignment domain unit under `cycle-v2/src/Graph/`;
- a small shared workspace-dock geometry/coordinator unit under
  `cycle-v2/src/UI/`;
- a Guide shelf presentation/interaction unit under `cycle-v2/src/UI/`.

Expected focused edits include:

- `NodeGraph`, `GraphSerializer`, `GraphValidator`, `GraphEditor`,
  `GraphCommandDispatcher`, and graph change-set/invalidation code;
- `GuideCurveSnapshotProvider` and `TrimeshGuidePreparation`;
- the Guide curve model/editor factory and expanded-editor subject binding;
- `SignalProbeRail`, `NodeCanvas`, `NodeCanvasPresentation`, scene/hit routing,
  editor commands, automation inspection, and application settings;
- `port_cycle_v1_preset.py`, the seven repository graphs, focused fixtures,
  and existing Guide graph/runtime/UI tests;
- build manifests for new files and deletion of `TrimeshGuideCableBundle`.

The expected production envelope is roughly 1,200-2,000 added lines and
300-700 deleted lines across the complete train. New translation units should
remain cohesive and generally below 500 lines. Existing large canvas,
presentation, editor-command, and curve-model files should remain flat or
shrink; substantial growth in any of them is evidence that a domain component
has not been extracted. A new adapter approaching several hundred lines,
repeated Guide target-kind branching, or a dock class containing resource and
preview behavior requires design review before proceeding.

Expected complexity is:

- resource lookup by stable ID and assignment lookup by typed target are
  constant-time through revision-owned indexes;
- creation, deletion, and document reorder may rebuild small resource indexes
  once at commit, but point/parameter gesture updates do not sort, serialize,
  or scan every assignment;
- selected-vertex assignment is linear in the number of owning cubes, not all
  graph nodes or resources;
- usage counts and visible highlighting come from an index built once per
  document revision in `O(G + A)`, where `G` is Guide count and `A` is
  assignment count; tile painting does not rescan all assignments per tile;
- preparing one Trimesh is linear in its own Guide assignments plus the
  existing off-thread table rasterization cost; it does not scan unrelated
  destination nodes;
- dock layout and hit testing are linear in visible tiles, while divider,
  drawer, resize, and scroll updates are constant-time apart from repaint;
- audio-time Guide lookup and deformation retain their existing preallocated,
  constant-time per-sample behavior. No new audio-thread collection lookup,
  allocation, serialization, or target dispatch is permitted.

There is no temporary compatibility layer in the intended change surface. If
a slice cannot land without a hidden Guide node or old-edge reader, stop and
reorder the slices rather than introducing a transitional second authority.

## Semantic Verification

### Document And Serialization

- Resource IDs and short labels are unique and stable through reorder.
- One resource may feed several nodes and fields; one target accepts only one
  resource.
- Assigning an occupied target replaces it atomically.
- Selected-vertex assignment writes the exact owning cube-component targets.
- Destination deletion and topology edits cannot leave invalid assignments.
- Delete-in-use detaches every usage and deletes the resource in one undo step;
  undo restores the model, parameters, order, identity, and all assignments.
- The canonical serializer round-trips resource models and every typed target.
- Old Guide nodes and Guide edges are rejected rather than migrated.
- Every shipped graph has the exact resource/assignment counts in the table
  above and contains no Guide node or Guide attachment edge.

### DSP And Presentation

- Directly converted presets produce the same Guide tables and prepared
  Trimesh metadata as their current node/edge forms.
- African Horn, Alto Sax, Baroque Flute, and Stengah preserve their existing
  observable guided output, rail segments, assignment tags, and Spy results.
- Editing one shared Guide changes every attached destination and downstream
  Spy while leaving unattached destinations unchanged.
- Reorder, rename, and unused-resource creation do not change
  execution plans, provider samples, latency, or audio output.
- Provider results are stable when shelf order changes.

### Gesture Sequence

- A full Guide edit test performs at least two point or parameter updates in one
  gesture, publishes them from one durable base revision, observes every
  attached destination, commits, verifies downstream Spy refresh/effect, and
  undoes to the exact original resource and outputs.
- Cancel after multiple live updates restores the original Guide and all
  dependent compact, expanded, DSP, and Spy products.
- Create-and-assign, replacement, detach, and delete-in-use each create one
  semantic undo entry.

### Dock And Interaction

- Two populated shelves begin at 50/50 and clamp to their minimum widths.
- An empty shelf retains its region and node-shaped vacancy indicator.
- Minimizing one shelf leaves a directional vertical drawer and expands its
  peer; restoring it recovers the prior ratio and scroll offset.
- Global collapse shows both counts, stops tile rendering, and retains canvas
  Spy markers.
- Guide and Spy scrolling are independent and clipped to their own shelf.
- At small window sizes the drawer/minimum-width rules prevent unusable tile
  compression.
- Guide double-click opens the full editor only above the dock and keeps Spy
  tiles visible.
- Hover/selection highlights matching endpoints; only hover draws at most one
  temporary tether.
- Assignment menus expose detach, new, and every named Guide without requiring
  a canvas Guide card.
- Native automation captures both populated shelves, one empty shelf, each
  minimized drawer, global collapse, Guide editing with a changing Spy, and
  deletion of a shared Guide.
- Tab and Shift-Tab traverse every currently visible dock control and tile;
  arrow, activation, and deletion keys exercise the same observable actions as
  pointer input.
- Selected Guides do not draw tethers. Hover draws at most one tether and
  leaving the canvas removes it immediately.
- Loading another graph clears stale Guide/Spy selection, hover, keyboard
  focus, and horizontal offsets.
- Both shelves use the shared tile/header grammar, neutral inactive borders,
  legible metadata, and visible overflow position feedback.

## Deletion Targets

The work is incomplete while any of these remain as production concepts:

- `NodeKind::GuideCurve` and its node definition;
- Guide in `NodePalette` and Guide node icons/cards/previews;
- Guide output ports and synthetic `guide.cube.*` input ports;
- `AttachmentType::GuideCurve` or Guide processing-attachment edges;
- `GraphEditor`/`GraphCommandDispatcher` APIs that create a Guide node or
  attach one by edge;
- `TrimeshGuideCableBundle` and Guide cable bundling/rendering/hit behavior;
- canvas placement, selection, movement, deletion, or expanded-editor lookup
  for a Guide node;
- runtime/compiler Guide execution or preview roles that exist only because a
  Guide was a node;
- serialized Guide node positions and attachment edges;
- tests or fixtures that construct Guide nodes as scaffolding.

`GuideCurveEditorComponent`, `FlatCurveModel`, `GuideCurveTableDsp`,
`GuideCurveSnapshotProvider`, `TrimeshGuidePreparation`, the guide policies,
and mature panel rail/tag rendering are reuse targets, not deletion targets.
Names containing `Node` must be corrected where their ownership is now a Guide
resource, but behavior is retained.

## Completion Criteria

- Guide Curves are authoritative document resources with typed assignments.
- No Guide palette item, canvas node, port, processing edge, cable bundle,
  compiler step, or hidden-node compatibility layer remains.
- The bottom workspace dock presents Guide and Spy shelves harmoniously with a
  50/50 default, vacancy placeholders, independent scrolling, directional
  drawers, and global collapse.
- The dock is keyboard operable, its controls have unambiguous shelf ownership,
  and its active focus is visible.
- Guide relationship tethers are hover-only and cannot persist across pointer
  exit or document replacement.
- Full Guide editing occurs above the dock while Spy feedback remains visible.
- Trimesh selection menus cover every authoritative component field and
  preserve cube-component assignments without persistent cables.
- Shared Guide edits invalidate and refresh every consumer and downstream Spy
  through consolidated semantic change sets and correct gesture revisions.
- All repository graphs and the Cycle 1 porting script emit only the new schema;
  no runtime migration adapter exists.
- Directly converted shipped presets preserve exact Guide counts, assignments,
  DSP output, guide rails/tags, and Spy observations.
- Drag-and-drop is not required and no partial field-specific approximation is
  shipped.
- The production diff has been audited for unexpected size, large adapters,
  repeated target-kind switches, domain logic in dock orchestration, and copied
  curve/Guide behavior.
- `git diff --check`, hot-loop scalar-math inspection, applicable clang-tidy,
  focused graph/runtime/UI tests, the full Cycle V2 suite, standalone build,
  focused agent fixtures, and native UI captures pass; unrelated failures or
  unavailable tools are recorded explicitly.

## Completion Evidence

Completed on macOS through the repository build and native launch scripts:

- standalone `CycleV2` and `CycleV2_tests` targets build with `--parallel 10`;
- the focused Guide-dock run passes 45 assertions in 5 test cases; the broader
  focused Guide/Guide-dock/automation run remains at 224 assertions in 15 test
  cases;
- the full Cycle V2 test executable passes 8,781 assertions in 493 test cases;
- the seven shipped graphs have the exact Guide/assignment counts above and
  contain zero Guide nodes or synthetic Guide edges;
- native fixtures pass for the populated OpenGL dock, independent Guide
  scrolling and drawer restoration, Spy drawer, empty Guide vacancy, global
  collapse, one relationship tether, Guide editing with Live Spy output, and
  shared Guide deletion/undo;
- OS-rendered captures were produced for populated shelves, the empty Guide
  shelf, both drawers, global collapse, Guide editing above visible Spies, and
  a keyboard-focused Guide tile. The final populated/focus capture is
  `/private/tmp/cycle-v2-guide-focus-native.png`;
- the launch script's macOS rectangle option was corrected so those captures
  use the same native runbook rather than a software-rendered substitute;
- deletion-target searches find no legacy Guide node kind, attachment type,
  cable bundle, synthetic port, runtime role, or serialized representation in
  production or shipped graphs;
- cumulative production changes were re-audited after implementation. The new
  dock units remain cohesive and below 500 lines, and the extraction reduces
  `NodeCanvas.cpp` below its pre-pass size. Shared dock geometry, input routing,
  Guide relationship painting, resource commands, and mature OpenGL curve
  rendering remain separated. The added automation and exact native-preview
  acceptance account for the change exceeding the original estimate; no
  compatibility adapter, copied curve behavior, or target-kind switchboard was
  introduced;
- `git diff --check`, shell syntax validation, and the scalar-math hot-loop
  inspection pass. `clang-tidy` could not be run because it is not installed
  in the development environment.
- The 2026-08-24 shelf simplification removes Guide colour dots/outlines and
  the resource-action popover, and expands the authoritative OpenGL preview
  into the recovered header area. Both build targets, the 45-assertion focused
  dock run, and the full 8,781-assertion suite pass. After Screen Recording
  permission was renewed, the native fixture and OS-rendered capture also pass;
  `/private/tmp/cycle-v2-guide-populated-native-v2.png` confirms the simplified
  chrome and expanded authoritative previews.
