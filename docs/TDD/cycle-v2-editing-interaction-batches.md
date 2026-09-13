# Cycle V2 Editing And Interaction Batches

Status: Implemented — vertex bulk-selection expansion intentionally deferred

## Scope

This train covers the reported Cycle V2 editing defects in coherent slices:

1. stable node multi-selection, Shift-click toggle, and one-gesture bulk move;
2. Trimesh explicit and linked selection stability, including morph changes;
3. Trimesh defaults and domain-specific mode/range controls;
4. cable context-hit feedback, implicit Voice Context, and document dirty state;
5. shared curve-reshape direction regression diagnosis and correction.

The Reverb spectrogram correction remains in the preceding UI-polish TDD but
ships before this train: it restores the mature logarithmic-frequency and
brightness transform while retaining the domain palette.

## Implemented Batch

- Node Shift-selection and bulk movement retain one primary editor target and
  commit one bounds-delta transaction. The native automation fixture covers
  selection, movement, and removal.
- Trimesh selection is empty until an explicit user selection is made. The six
  vertex-property rows remain visible in that state but are visually disabled
  and non-interactive. A morph gesture changes only the preview position: the
  explicit selected-vertex identity and all displayed vertex-property values
  remain fixed until another vertex selection is made. Linked-axis highlights
  reuse the mature `selectedFrame` movement set.
- New Trimesh nodes already default all three link parameters on; focused
  factory tests guard that contract.
- Cable popup hit testing is independent of overlapping node-body targets, and
  the same hit range drives hover highlighting.
- Cycle 1 undo-backed edits now refresh the established title asterisk and Save
  command. Cycle 2 uses save-point identities for dirty state and presents the
  same title convention and Save availability.

## Node Selection Contract

- `NodeCanvasAuthoringSession` owns the ordered selected node IDs. Its existing
  `selectedNodeId` remains the primary selection used by editors and keyboard
  commands.
- A plain click on an unselected node replaces the selection. A plain drag of
  any already-selected node moves the whole selection. Shift-click toggles one
  node without disturbing the other selected nodes.
- `NodeCanvasInteraction` retains the selected IDs and primary start bounds for
  the gesture. `GraphCommandDispatcher::translateNodes` remains authoritative
  for the semantic delta and captures only the affected bounds.
- Movement updates are absolute relative to the pointer-down position and
  translate only the selected nodes. Selected peers are excluded from snap
  candidates. One compound transaction covers all movement updates and undo.
- Painting and automation expose every selected node while preserving the
  primary-selection compatibility field.

### Shift-drag Area Selection

- Shift-drag on empty canvas begins an additive marquee. It preserves the
  existing ordered selection and primary editor target, then adds each node
  whose presented bounds intersect the completed marquee.
- Pointer-down and each movement update retain only two screen points and do
  O(1) work. Mouse-up performs the single O(n) node-bounds scan required by the
  semantic selection result. The gesture does not mutate the graph, publish a
  document revision, capture undo, or rebuild node presentation.
- A sub-threshold Shift press on empty canvas is a no-op. Plain empty-canvas
  drag retains the existing pan behavior.
- The live marquee uses one translucent fill and one crisp outline above node
  content. Its visual rectangle and completion hit rectangle are the same
  normalized screen-space bounds.

### Area-selection Evidence

- Focused interaction tests cover normalized reverse-direction geometry,
  multiple movement updates, completion, reset, and the sub-threshold no-op.
- The native fixture starts with an existing FFT selection, Shift-drags across
  Voice Context and Morph, and verifies the ordered three-node selection,
  stable FFT primary target, and unchanged document dirty state.
- Production-size captures verify the live marquee and the final focus rings.
  The existing Shift-click/bulk-move fixture remains the movement contract.

## Complexity

Pointer-down copies only the selected ID list. Each movement update retains the
existing graph-wide snap scan and adds work proportional to the selected node
count; it does not copy the graph, serialize state, or rebuild unrelated node
presentation. The dispatcher uses its existing bounds delta and compound undo
path.

## Remaining Design Gates

- Trimesh selection must reuse `Interactor2D`/`Interactor3D` linked movement
  frames rather than introduce a second selection algorithm in Cycle V2.
- The mode selector needs a domain contract for magnitude, phase, and time
  before changing `GraphRenderSemanticResolver`; `spectralMode` currently has
  only additive/multiplicative magnitude semantics.
- Voice Context implicit routing requires a compiler/graph-format decision and
  preset migration plan.
- Curve reshape polarity remains owned by shared `Interactor2D`; there is no
  Envelope sign branch. The stable gesture-start direction toward the selected
  control is authoritative. `Curve::tp.ypole` is raster metadata for one
  prepared curvelet and can disagree with the editable control on Envelope's
  blended, uneven segments, which caused the apparent inversion.

## Trimesh Morph-Selection Correction

- `TrimeshNodeModel` remains authoritative for explicit selection and vertex
  values. The mature interactors remain authoritative for selecting vertices.
- `TrimeshExpandedEditorComponent` marks the lifetime of a morph gesture;
  `TrimeshPanelBridge` suppresses only editor-state selection resynchronization
  during that lifetime. Morph parameter, render invalidation, and dispatcher
  behavior remain unchanged.
- Normal node binding still synchronizes durable `selectedVertexId`, so undo,
  load, and an explicit selection command remain authoritative outside a morph
  gesture.
- Disabled placeholder rows reuse the existing six parameter descriptors and
  side-panel geometry. They do not create a fallback vertex and cannot dispatch
  a vertex or guide edit.
- Morph movement remains O(1) with respect to graph and mesh size. The gesture
  does not copy a graph or mesh, serialize state, or scan vertices.

### Correction Evidence

- Focused model, disabled-control, pointer-interaction, and command-service
  tests pass, including two morph updates, commit, and undo.
- The native fixture selects vertex 0, assigns recognizable phase/amp values,
  performs two morph drag updates, and confirms the selected index and both
  values remain unchanged.
- The production OS capture
  `/private/tmp/cycle-v2-trimesh-inactive-os.png` confirms all six property rows
  remain visible and visibly inactive with no explicit selection.

## Dirty-State Contract

- Cycle 1's `EditWatcher` title convention is authoritative: append `*` to the
  preset title and enable Save only while the document differs from its save
  baseline. Undo-backed edits notify its existing clients just as non-undo
  edits already do.
- Cycle 2 keeps save-point identity in `GraphDocument`, without serializing or
  comparing the graph. History entries retain their before/after state IDs, so
  edit, undo, redo, save, and load update dirty state in O(1). `NodeCanvas`
  translates that state into a narrow callback for the standalone window; the
  window owns title and command presentation. Automation exposes the same
  document state for regression tests.

## Trimesh Output-Scale Contract

- The existing lower range rail is the shared Trimesh output-scale affordance.
  Spectral magnitude and phase retain their mature `range` mapping and
  Range/Width labels. A time-domain Trimesh binds the same rail to normalized
  `gain`, labels it Gain, and uses `CycleDsp::outputGain`; 0.5 remains unity.
- The parameter edit continues through `NodeEditorCommandService` and
  `GraphCommandDispatcher` as one transient gesture and one undo transaction.
  `TrimeshConfiguration::gain` is the existing runtime application point, so
  no second scaling algorithm or render traversal is introduced.

### Completion Evidence

- The shared editor control and automation hit region are named Output Scale;
  only their domain binding and visible label vary.
- Focused definition, DSP configuration, editor gesture, and undo tests pass
  for the time-domain `gain` binding while the existing spectral `range`
  contract remains covered.
- The editor sequence test publishes two values in one gesture before commit
  and undo. The native time-Trimesh fixture performs the pointer gesture,
  verifies the resulting output-scale value, undoes to unity, and captures
  `/private/tmp/cycle-v2-time-trimesh-gain.png` for production-size review.

## Trimesh Default Morph Contract

- `PreviewPitchResolver::defaultMidiNote` owns the preview key, and
  `ModulationSource::normalizeKey` owns its unit mapping across
  `Constants::LowestMidiNote` and `Constants::HighestMidiNote`.
- Newly created Trimesh nodes start Yellow and Blue at zero. Red starts at the
  normalized preview key so its durable fallback agrees with the existing
  key-scale presentation adapter. Existing presets retain their authored
  values; no load migration rewrites them.

## Node Selection Completion Criteria

- Unit tests cover replace, Shift-add, Shift-remove, stable primary selection,
  two movement updates, commit, visible positions, and one-step undo.
- A native automation fixture Shift-selects two nodes and drags them together.
- Every selected node receives the same focus-ring treatment.
- Focused tests, Standalone Debug, `git diff --check`, and production-size
  screenshot review pass before commit.

## Dirty-State Completion Criteria

- Cycle 1 refreshes its existing title and Save command after a successful
  undo-backed edit, undo, redo, load, and save reset.
- Cycle 2 marks semantic edits dirty, clears after save/load, clears when undo
  returns to the saved state, and restores dirty on redo without graph-wide
  comparison work.
- The Cycle 2 standalone title uses the Cycle 1 `Product - Preset*` convention,
  and Save availability plus automation state agree with `GraphDocument`.
