# Cycle V2 playback cursors and release previews

Status: Complete

## Contract and ownership

Cycle 1 `Envelope2D::postCurveDraw`, `PlaybackPanel`, and shared
`EnvelopePlaybackEngine` establish cursor drawing and note/loop/release behavior.
Cycle 2 will expose positions from its actual audio owners, after scratch and
morph resolution, rather than simulate a second UI envelope clock. The open
editor receives at most eight voice cursors through bounded atomic publication.
Audio never locks, allocates, copies meshes, or publishes graph edits for this.
The editor paints thin overlay lines using existing panel scaling and clipping.
Idle morph navigation draws the primary-axis slice without rebuilding surfaces.
Envelope time is Phase, distinct from the yellow morph dimension; its surface
keeps time horizontal and morph vertical. Voice ADSR, loops and release jumps
remain owned by the shared playback engine, including cyclewise scratch/pitch.

Spy traversal reserves approximately 20% of columns for release. The preceding
80% samples normalized voice time [0,1]. Release holds the unscratched mesh at
x=1; scratch envelopes advance through their authored release. Guide noise
continues changing with column index. Shared preview traversal coordinates own
this policy, independently of editor surface coordinates.

## Completion and deletion targets

- Publish resolved mesh morphs and actual envelope playback positions, including
  cyclewise envelope banks, to expanded Trimesh and Envelope panels.
- Idle slider navigation and live MIDI/overlapping notes display correctly;
  release/loop positions come from audio, and stop/reset clears stale cursors.
- Cursor updates repaint overlays only: no graph/model revisions, undo entries,
  rasterization, surface copies, or preview preparation.
- Apply a common sustain/release sampling schedule to Spy mesh/envelope grids;
  remove the sustain-only envelope traversal loop. Preserve guide noise motion.
- Verify runtime note/loop/release/scratch semantics with focused tests; build
  the exact app and inspect automated UI state and screenshots under `.context/`.
- Run style, architecture and code-map checks; review sizes and commit.

## Implementation and verification

`PlaybackCursorExchange` publishes a fixed eight-voice frame once per audio
callback. Node IDs become numeric keys during preparation, so audio performs no
string searches. `TrimeshMorphResolver` records its final scratch-resolved
coordinate; prepared cycle bindings supply the capture only for the requested
editor. `EnvelopeSignalProcessor` supplies actual Phase and prepared morph;
`PreparedCycleEnvelopeBank` replaces the Phase with its actual cyclewise clock
when relevant. There is no new envelope state machine or UI timer simulation.

`PanelPlaybackOverlay` owns drawing and coordinate translation. Trimesh and
Envelope panel hooks delegate to it under their existing render locks. Envelope
Phase remains independent of morph Yellow. The workspace's existing timer
forwards the frame to the bound widget without preview reconciliation. Equal
frames do not repaint. No graph commands, model publications, mesh copies,
surface rebuilds, or slice rasterization occur during cursor-only movement.

`PreviewTraversal` owns the 80/20 sampling schedule: note-off follows the x=1
column, and the remaining columns advance approximately another quarter of the
normalized voice duration. Unscratched mesh morph holds at 1 while scratch
follows the shared engine's release. Per-column Guide noise seeds keep advancing.
This schedule applies to Spy products, not editor surface grids. The old
sustain-only envelope traversal and alternate mesh-grid path are removed.

The exact standalone target builds. The committed
`cycle-v2-agent-playback-cursors.json` app fixture passes all 25 commands,
including physical rail routing, MIDI activity, both surface axes and final
voice retirement. Twelve focused CTest cases pass, covering
renderer lifecycle and overlapping notes, loop/release jumps, cyclewise clock
capture, resolved scratch positions, independent envelope axes, no surface/slice
rebuilds, release traversal, Guide noise, and zero realtime allocations/locks
with cursor capture enabled. The interactive app check also moves a real primary
rail through multiple updates and release: document revision and surface rebuild
count stay unchanged while the line moves. Live MIDI mesh/envelope cursor values
change between captures. Evidence: `.context/cursor-ui-report.json`,
`.context/ctest-cursors-final.log`, and `.context/playback-cursors-envelope.png`.

The broad audit's 15 unrelated failures reproduce with original HEAD DSP/preview
sources; the one changed traversal expectation now uses the new note-off
schedule. Those incidental findings are recorded in `ui-bugs.md` and
`audio-bugs.md`.

## Size and responsibility review

New runtime publication and overlay policy live in small dedicated files. Large
existing files receive forwarding or capture hooks only: GraphAudioExecutor
1257→1258 lines (prepared context key), NodeCanvas.h 482→486 (editor forwarding),
NodeWorkspace 1000→1002 (timer delivery), EnvelopeCurvePanel 1158→1169 (2D draw
hook), and CurvePanelController 866→871 (two panel destinations). Their stable
collaborators and policy owners remain unchanged. TrimeshNodeAudioProcessor
shrinks 522→503 lines by using one morph-grid path. No existing orchestration
file takes ownership of playback semantics or release policy. Architecture and
code-map audits pass their structural checks; existing size triggers were
reviewed with the cohesion reasons above.
