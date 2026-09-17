# Cycle V2 Curve Interaction Repairs

## Status

Implemented (2026-09-17): interaction, layout, logical selection presentation,
and session-only link controls pass focused C++ and UI automation checks. A
genuine Envelope bimesh remains separate work in `envelope-bimesh.md`.

## Authority and design

`Interactor::setMouseDownStateSelectorTool` and `Interactor::doBoxSelect` own the
mature selection gesture. The Trimesh editors already opt into Shift-left box
selection; Guide and Envelope should opt into the same algorithm, not implement
their own selection state machines. Pointer-down, movement, and commit retain
the existing cost of that algorithm. No extra mesh snapshot is permitted on
movement.

`EnvelopeCurvePanel` edits the existing `EnvelopeMesh`/`VertCube` topology.
Each Red/Blue logical corner has a Time-pole storage pair. Editing must continue
to move both physical vertices to preserve old preset, rasterizer, and DSP
semantics; presentation and automation must count/show only the four logical
corners. A genuine two-axis storage mesh is a separate migration tracked by
`docs/TDD/envelope-bimesh.md`, not a cosmetic change in this slice.

Envelope link state is session-only. All editors start linked, regardless of
legacy saved flags. Preset load ignores those flags and save omits them. A link
click changes only editor/panel state, with no graph transaction, undo entry,
mesh copy, or rasterizer rebuild. The next actual mesh gesture rebuilds movement
frames from current links.

For layout, the Envelope Purpose selector moves to the expanded header in the
same text-segmented visual language as the Trimesh Type selector. The lower
action bar then centers its Marker, Scaling, and Zoom controls as one group.

## Completion checks

- Shift-left drag box-selects in Guide and Envelope through the shared gesture.
- Envelope selected markers and count use at most four Red/Blue logical corners
  per cube, while physical Time-pole pairs still move together.
- Envelope links start on even for saved-off presets, are not saved, and toggling
  does no per-vertex or document work before the next edit gesture.
- Header Purpose and centered lower actions pass production-size visual review
  and focused automation bounds/interaction checks.

Verification: the Guide/Envelope Shift-box fixture, Purpose fixture, and
session-link fixture pass. A production-size macOS screenshot was captured
at `/private/tmp/cycle-v2-envelope-header-review.png`. Focused Envelope model
and editor-host tests pass.
