# Voice Context compact interaction

Status: implemented.

## Contract

The collapsed Voice Context shows voice duration as a concise clock value,
octave shift in semitones at every value, and glide only when enabled. Its four input rows use
stable pictograms matching Modulation, Unison, Pitch, and Scratch. Hovering the
node or a port reveals readable meaning and current state without filling the
compact body with prose.
The octave indicator uses a monochrome tuning fork separated from the pitch
port icon, followed by a signed semitone value, including `0 st` when unshifted.
The canvas status line owns hover explanations; no duplicate tooltip appears.
Empty inputs show a persistent `+`, independent of pointer hover repaint state.

Clicking the badge on a disconnected input creates the default compatible source,
sets an envelope's purpose before connection, places it in the connected source
stack while moving adjacent sources apart when the resulting layout is clear,
and connects it in one undoable edit. An existing connection must never be
replaced by this shortcut. Ordinary port drag remains cable authoring.
If local insertion would collide with another node, creation uses the existing
vacant-position search.
Pitch starts flat at 0.5 and scratch starts as a straight 0-to-1 ramp, using
the Cycle 1 envelope defaults. Existing edited envelopes retain their shape.

## Ownership and complexity

`VoiceContextCompactEditor` owns only the compact drawing. `NodeCanvasQueryModel`
owns hover descriptions. `NodeCanvasAuthoring` composes the existing
`GraphCommandDispatcher` add, parameter, and connect operations; graph editing,
validation, publication, and undo remain with that dispatcher. Placement checks
nearby node bounds once at creation, never during a live drag. No existing
graph or envelope behavior is copied.
`VoiceContextInputCreation` owns source eligibility, badge geometry, and local
placement. The envelope shape uses Cycle 2's `EnvelopeMeshState` and
`EnvelopeNodeModel` path; its point values come from Cycle 1's
`MeshDefaults::initEnvelopeMesh`. This translates defaults into the Cycle 2
model and needs no compatibility layer.

## Completion

- Compact drawing and hover descriptions work at normal canvas zoom.
- Quick creation works for Modulation Triple, Unison, Pitch Envelope, and Scratch
  Envelope, including undo and an occupied-port no-op.
- A focused UI fixture exercises a real badge click and screenshot.

## Verification and architecture review

`TestNodeCanvasAuthoring` covers all four source kinds, purpose-specific shapes,
connected-port refusal, insertion order, collision avoidance, and one-step undo.
The Baroque Flute fixture exercises a single click on the Unison badge: node
count 22 to 23, edge
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

The follow-up changes `NodeCanvas.cpp` from 3331 to 3337 lines,
`NodeCanvasAuthoring.cpp` from 1018 to 1043, `NodeCanvasPresentation.cpp`
from 1529 to 1528, `VoiceContextInputCreation.cpp` from 71 to 180, and
`VoiceContextCompactEditor.cpp` from 200 to 191. The large canvas files keep
their listed responsibilities, and their extraction plan remains in
`refactors.md`. The new local placement policy stays in
`VoiceContextInputCreation`; the authoring layer only applies its plan through
the dispatcher. Painting and click routing share one badge-bound calculation.
The focused fixture passed with 14 commands and no failures; its after image
is `/private/tmp/cycle-v2-voice-context-created.png`.

An additional zero-octave regression showed that hiding the neutral state made
the octave property disappear. The first always-visible `Oct 0` label read as
a date, and the later struck-through ottava symbol was unclear. The compact
state now shows the semitone offset with a spaced monochrome tuning fork. A
focused image test checks that this state paints into the semitone row.

The follow-up removes the canvas `TooltipClient` text while retaining the
tooltip host for child controls. `NodeCanvasQueryModel` continues to supply
the status-line help. The quick-create painter now draws eligible empty input
badges independent of hover state; changing the hovered badge no longer
requests a canvas repaint. It reuses the existing pitch SVG through a cached
monochrome renderer for the semitone cue. The semitone cue starts 42 world units
inside the summary area, leaving about 55 world units between the two fork
icons' edges at unit zoom. The compact value represents the octave parameter
as `octave * 12` semitones; the separate pitch parameter is not included.

Follow-up verification: the focused C++ tests pass 15 assertions in two
cases. The Baroque Flute agent fixture passes 17 commands with zero failures;
it asserts that hovering the quick-create badge requests zero canvas repaints.
The captured hover frame at `/private/tmp/cycle-v2-voice-context-port-hover.png`
shows status help without a second popup. `NodeCanvas.cpp` decreases from
3337 to 3333 lines, and `NodeCanvasPresentation.cpp` decreases from 1528 to
1525. Their responsibilities remain canvas interaction and presentation;
eligibility and badge geometry remain owned by `VoiceContextInputCreation`.
