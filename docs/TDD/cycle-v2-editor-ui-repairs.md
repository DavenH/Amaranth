# Cycle V2 Editor Interaction And Preview Repairs

## Status

In progress (2026-09-16).

First slice implemented: Trimesh Shift-box selection, five-pixel curve pickup,
legacy Envelope link defaults, implicit Time link presentation, expanded IR
and Voice Context geometry, shared Unison slider styling, spacebar propagation
through segmented controls, and Spy-over-Guide hover priority. Focused unit
tests and a standalone screenshot pass; keyboard-focus and overlapping-dock
native automation are still needed.

The Envelope panel rasterizer reports depth points under Red/Blue (14 for the
default seven cubes), not under Time: they are projections through the
storage-only Time-pole cube topology. They are presentation artifacts in the
Envelope 2D view. Only the projected point markers are suppressed; the
colour-coded depth lines remain visible.

Second slice implemented: document-reload Envelope framing now uses the
authored vertex bounding box with margin, without rewriting mesh coordinates.

Third slice implemented: Spy tethers now attach to the Spy rail's top edge,
matching the Guide rail's dock-top attachment. The node-editor slider audit
found Unison as the only plain JUCE slider remaining under `cycle-v2/src/Nodes`.

Fourth slice implemented: the File menu opens a searchable local Cycle V2
preset browser with a native file-browse fallback. Opening still delegates to
the existing V2 document loader. This does not port Cycle 1 remote/community,
ratings, or tags behavior; those remain out of scope for this local browser.
The browser has a focused search/open test and a native menu-to-load check.

Fifth slice implemented: automation now exposes Spy tile targets and hovered
Spy identity. The six-Guide/Spy overlap fixture confirms the Spy retains hover
and suppresses the occluded Guide; focused keyboard tests confirm Space is not
consumed by segmented properties or Trimesh link controls.

Sixth slice implemented: preview-note selection and mod-wheel release commit
Time=0, Red=normalized preview key, and Blue=CC1/127 across Trimesh and
Envelope nodes in one undo step. Wheel movement does not publish graph edits.
Envelope morph-only model revisions share the immutable mesh; operation-count
tests show no graph or mesh copies, serialization, or unrelated-node/parameter
scans. The reported regression to Envelope color lines was corrected by
separating line visibility from storage-only point-marker visibility. Focused
keyboard automation and a production-size expanded-Envelope capture pass.
The honerism-3 Live Spy fixture exposed a delayed graph refresh overwriting a
newer CC1 preview value: per-product currentness did not protect the shared
presentation snapshot. Async publication now also checks the whole-snapshot
generation so an older refresh cannot roll back preview controls.

Native focus delivery was checked in an agent session: with the Voice Context
editor open, a native click selected the `2x` oversampling segment, then a
native Space key started preview playback (`previewPlaying=true`) at a long
enough voice duration to observe it. A one-second preview can finish before a
subsequent session snapshot and should not be used as a focus assertion.

Remaining: remove the duplicate legacy Envelope Red/Blue cache fields from
`EnvelopeNodeModel` after migrating its editor adapter to node-parameter
authority. The optional true bimesh decision remains separate in
`docs/TDD/envelope-bimesh.md`.

## Preview Morph Ownership Decision

The user chose durable ownership: preview-note and CC1 updates overwrite the
saved morph values (Time=0, Red=normalized preview key, Blue=CC1/127). The
node parameter and Envelope model representations must remain consistent.
CC1 movement should present the current preview continuously but publish one
durable graph edit at gesture completion, preserving the release-only Spy
refresh policy and one undo step. A preview-note selection is one atomic edit.
Do not deep-copy meshes or the graph on movement updates.

Envelope geometry remains shared across morph-only model revisions. The
revision carries Red/Blue overrides, and its serializer writes those overrides
into the existing schema. `EnvelopeNodeModel` still carries legacy cached
Red/Blue fields for its editor adapter; removing those duplicates is a future
deletion target once all model/editor consumers read the node-parameter source
of truth directly. The graph keeps a morph-node identity index so a preview
gesture does not scan unrelated nodes. Loading a preset remains clean; opening
a morph editor or changing the preview note/CC1 performs the durable overwrite.

Diff review: `CurveNodeModels.cpp` reaches 825 lines because it already owns
FlatCurve, Envelope, and their codec; the new immutable morph revision remains
beside its existing snapshot constructors. Split the Envelope model/codec when
removing its duplicated scalar cache. `NodeCanvas.cpp` remains an oversized
orchestrator; the added code only routes preview events to the semantic command
and does not move model or rendering algorithms into the canvas.

## Scope And Authority

This train covers the reported Trimesh selection gesture, curve proximity,
link defaults, expanded-editor geometry, Envelope framing, preview-control
morph position, spacebar audition, slider style, Guide/Spy tethers, and preset
loading. Preserve the mature Cycle 1 mesh interaction, rasterization, and preset
browser behavior where reusable; Cycle V2 adapters may translate events and
ownership but must not copy those algorithms.

The proposed true Envelope bimesh is a separate domain-model decision. The
current EnvelopeMesh, VertCube, graph model, serializer, rasterizers, and DSP
consumers remain authoritative until a separate TDD establishes an extraction
and migration plan. Hiding a misleading Time link is a presentation change,
not authorization to replace the mesh representation in this train.

Cycle 1 `PresetPage` is 1,485 lines and owns Cycle 1 `SingletonRepo`,
`DocumentDetails`, `Document`, remote upload/download threads, and UI panel
services. Reuse wholesale would bring those lifecycle and graph-format
dependencies into Cycle V2. The stable boundary is a Cycle V2 local-file
browser that passes a selected `File` to `MainWindow::openGraphFile`, leaving
all graph decoding and dirty-state policy in the existing V2 document path.
The browser may reuse JUCE table/search controls, but no Cycle 1 preset
loading or remote state machine is copied.

## Interaction Contract

- Shift-left drag in Cycle V2 Trimesh 2D and 3D editors starts and retains box
  selection; it cannot enter vertex movement. Cycle 1's established modifier
  semantics remain unchanged.
- Curve reshaping hover/acquisition requires a measured distance of at most
  five production pixels from the rendered curve. Vertex hit targets keep
  priority when both are nearby.
- Link axes are enabled for newly created Envelope and Trimesh editors. The
  Envelope Time link is implicit and absent from painting, hit targets, focus,
  and automation, without exposing misleading Time-pole points.
- Spacebar audition works after focus moves into property controls, except
  while a text-entry control must receive an actual space character.

## Presentation Contract

- IR expanded editor gains 20% width without shrinking its drawable content;
  Voice Context expanded content grows to remove cramped control grouping.
- On preset load, Envelope view framing contains the authored visible vertex
  bounding box with deliberate margin, while leaving durable mesh coordinates
  unchanged.
- Saved editor morph position follows preview controls: blue is normalized
  CC 1, red is normalized selected-note position across the playable range, and
  time is zero. Gestures commit atomically and remain undoable; movement must
  not trigger synchronous Spy refresh.
- Unison uses the existing precision-slider visual language, and other legacy
  slider presentations are audited rather than converted indiscriminately.
- Spy tether endpoints use the top of the Spy bar, and Guide tethers are not
  highlighted by pointer positions occluded by the Spy bar.
- Preset loading reuses the mature Cycle 1 browser where its data and lifecycle
  contracts fit; otherwise extract a shared browser core and keep graph-format
  loading in each app's own document service.

## Complexity And Verification

Gesture movement must not clone or serialize complete graphs or meshes. Tests
cover full down/move/up sequences, selection stability, downstream effects,
and operation counts. Geometry assertions accompany production-size before/
after captures. Each coherent slice receives a refactor pass, style check,
focused tests, and a commit before the next slice. Keep this TDD in progress
until every item and any explicit deletion target is complete.
