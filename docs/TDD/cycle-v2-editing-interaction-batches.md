# Cycle V2 Editing And Interaction Batches

Status: In progress — first interaction batch implemented

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
- Trimesh selection is empty until an explicit user selection is made. Morph
  changes preserve that explicit selection, and linked-axis highlights reuse
  the mature `selectedFrame` movement set.
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
