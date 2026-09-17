# Cycle V2 Editor Interaction And Preview Repairs

## Status

In progress (2026-09-16).

First slice implemented: Trimesh Shift-box selection, five-pixel curve pickup,
legacy Envelope link defaults, implicit Time link presentation, expanded IR
and Voice Context geometry, shared Unison slider styling, spacebar propagation
through segmented controls, and Spy-over-Guide hover priority. Focused unit
tests and a standalone screenshot pass; keyboard-focus and overlapping-dock
native automation are still needed.

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

Native focus delivery was checked in an agent session: with the Voice Context
editor open, a native click selected the `2x` oversampling segment, then a
native Space key started preview playback (`previewPlaying=true`) at a long
enough voice duration to observe it. A one-second preview can finish before a
subsequent session snapshot and should not be used as a focus assertion.

Remaining: preview-driven editor morph position and the separate bimesh decision in
`docs/TDD/envelope-bimesh.md`.

## Preview Morph Ownership Decision

The existing Trimesh and Envelope morph controls author durable node values.
Replacing their values on each preview-note or CC1 movement would issue graph
edits and undo events, and would conflict with the user's release-only Spy
refresh policy. The expected design is a transient editor preview position
(Time=0, Red=normalized preview key, Blue=CC1/127) layered over authored
defaults. It must not serialize or enter graph commands. The UI must define
what happens when someone drags an authored morph slider while this preview
layer is active (temporary manual override, edit the source, or display both
positions) before implementing this cross-editor slice. A user choice was
requested; do not silently make one slider's meaning change.

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
- Transient editor morph position follows preview controls: blue is normalized
  CC 1, red is normalized selected-note position across the playable range, and
  time is zero. This must not persist to the graph or recalculate unrelated DSP.
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
