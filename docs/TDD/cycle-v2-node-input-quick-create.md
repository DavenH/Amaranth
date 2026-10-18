# Node input quick creation

Status: implemented.

## Contract

Hovering an empty Scratch input or Mod Triple bundle socket on a Trimesh shows
the same `+` as an empty Voice Context input. One click on the badge creates a
Scratch Envelope or Modulation Triple, places it to the left, connects it, and
groups the edit into one undo step. The existing socket drag remains available.
The badge disappears when the destination is occupied. For a Mod Triple bundle,
any connected yellow, red, or blue input counts as occupied; quick creation
must not replace partial or complete existing cabling. Hover help stays in the
canvas status line and does not create a popup.

## Design and ownership

`VoiceContextInputCreation` was the existing authoritative implementation for
quick-create eligibility, badge geometry, source defaults, and connected-source
stack insertion. Extend that service into a node-input quick-create policy
rather than copying those decisions. Preserve its Voice Context layout and
vacant-position search. For Trimesh, reuse the same vacant-position search,
aligning the new source socket with the hovered destination. No live gesture
performs graph-wide placement work; the search runs only on the click.

`ModulationCableBundle` remains authoritative for synthetic bundle addresses,
socket centres, and its three axis routes. `NodeCanvasScene` owns hit bounds;
the quick-create policy owns eligible destinations. `NodeCanvas` owns pointer
routing and hover invalidation; `NodeCanvasPresentation` owns the badge drawing.
`NodeCanvasAuthoring` orchestrates one compound `GraphCommandDispatcher` edit.
The existing `EnvelopeNodeModel` supplies Scratch's linear default. No graph
mutation or independent undo path is introduced in UI code.

## Completion

- Both Trimesh inputs show `+` on hover only, with clear status help and a
  stable cursor; off-hover and occupied inputs show no badge.
- One click creates and connects the correct source, without replacing any
  connected morph axis or Scratch input; undo restores the prior graph.
- Focused tests cover eligibility, purpose and shape, all three bundle routes,
  connected-port refusal, placement, and one-step undo.
- A focused agent fixture covers hover, one-click creation, screenshot, and
  undo on a real Trimesh preset.

## Verification and review

The former Voice Context service is now `NodeInputQuickCreate`. Its Voice Context
stack insertion and vacant-position search are retained in one policy owner.
`NodeCanvas`, `NodeCanvasPresentation`, and `NodeCanvasAuthoring` each only route
the hover, draw the badge, or dispatch the compound edit. The synthetic bundle
uses `ModulationCableBundle` for its address, position, and three routes.

The touched large files remain coordinators: `NodeCanvas.cpp` routes pointer
events (3337 to 3334 lines), `NodeCanvasPresentation.cpp` delegates drawing
(1528 to 1520), and `NodeCanvasAuthoring.cpp` coordinates semantic commands
(1043 to 1036). The extracted quick-create policy is 260 lines, replacing the
180-line Voice Context policy. No quick-create eligibility, placement, or
transaction policy was copied into those coordinators; no old service or
callsite remains. The production change adds no graph mutation path outside
`GraphCommandDispatcher` and no new preview or DSP behavior. The size audit
still reports existing PLAN thresholds for the two canvas files; this slice
does not grow them or add a responsibility to them.

Verification: `standalone-debug` built `CycleV2` and `CycleV2_tests` with
`--parallel 10`. The focused Trimesh tests passed (40 assertions), the
badge hit-test passed (16 assertions), and existing Voice Context quick-create
tests passed (61 assertions). The real-preset Trimesh fixture passed all 23
steps, including Scratch and Mod Triple hover, click, compile, and undo; the
Voice Context fixture passed all 19 steps. Screenshots are in `/private/tmp/`
under `cycle-v2-trimesh-*`. `scripts/capture_cycle_ui.sh` produced
`/tmp/cycle-ui.png` with a clean filtered log. `git diff --check` passed,
the architecture audit completed, and clang-tidy found no new issue after
the new axis-eligibility loop was simplified. An existing CoreMIDI assertion
in the agent harness is already tracked in `docs/TDD/ui-bugs.md`.
