# Voice Context compact interaction

Status: implemented.

## Contract

The collapsed Voice Context shows voice duration as a concise clock value, octave
shift only when nonzero, and glide only when enabled. Its four input rows use
stable pictograms matching Modulation, Unison, Pitch, and Scratch. Hovering the
node or a port reveals readable meaning and current state without filling the
compact body with prose.

Double-clicking a disconnected input creates the default compatible source,
sets an envelope's purpose before connection, places it outside nearby nodes,
and connects it in one undoable edit. An existing connection must never be
replaced by this shortcut. Ordinary port drag remains cable authoring.

## Ownership and complexity

`VoiceContextCompactEditor` owns only the compact drawing. `NodeCanvasQueryModel`
owns hover descriptions. `NodeCanvasAuthoring` composes the existing
`GraphCommandDispatcher` add, parameter, and connect operations; graph editing,
validation, publication, and undo remain with that dispatcher. Placement checks
nearby node bounds once at creation, never during a live drag. No existing
graph or envelope behavior is copied.

## Completion

- Compact drawing and hover descriptions work at normal canvas zoom.
- Quick creation works for Modulation Triple, Unison, Pitch Envelope, and Scratch
  Envelope, including undo and an occupied-port no-op.
- A focused UI fixture exercises a real port double-click and screenshot.

## Verification and architecture review

`TestNodeCanvasAuthoring` covers all four source kinds, connected-port refusal,
collision avoidance, and one-step undo. The Baroque Flute fixture exercises a
real canvas double-click on the empty Unison port: node count 22 to 23, edge
count 20 to 21, successful compilation, then one undo restores both counts.
The scene geometry test verifies that the visible `+` is inside its port's hit
target and that adjacent targets remain distinct at normal zoom.
The production-size OS capture at
`/private/tmp/cycle-v2-voice-context-os.png` shows the compact indicators. The
physical-hover capture at
`/private/tmp/cycle-v2-voice-context-hover-physical.png` shows the `+` and
port explanation.

Reviewed sizes (before to after): `NodeCanvas.cpp` 3300 to 3331,
`NodeCanvasAuthoring.cpp` 953 to 1018, `NodeCanvasPresentation.cpp` 1485 to
1529, `NodeCanvasScene.cpp` 746 to 759, and `VoiceContextCompactEditor.cpp`
106 to 200. `NodeCanvas` retains
pointer routing and repaint invalidation; `NodeCanvasAuthoring` retains
transaction orchestration and selection; `NodeCanvasPresentation` retains
dynamic canvas painting; `NodeCanvasScene` owns port hit geometry. The source
choice and collision placement policy live
in `VoiceContextInputCreation`, along with the one eligibility rule shared by
hover and authoring. Callers supply only the current connection fact. No graph
mutation bypasses
`GraphCommandDispatcher`. No existing edit or rendering policy was copied. The
larger canvas-file extraction work remains tracked in `refactors.md`; this
slice adds only a short dispatch branch and hover-state transition there.
