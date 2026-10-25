# Draggable Spy note-off audition

Status: Complete

## Contract and ownership

The expanded Spy note-off boundary and release icon are draggable. The chosen
note-off is normalized voice time x in [0,1], independent of the extra tail
width. The full voice-plus-longest-release horizon stays fixed while auditing
staccato notes. Earlier release holds an unscratched morph at the chosen slice;
scratch and pitch envelopes use actual shared-engine note-off playback.

This is an ephemeral expanded-Spy preview setting. No graph publication, undo,
preset write, audio-plan update, compilation or capture occurs on pointer-down
or movement. Pointer-up captures the selected probe once if its note-off changed.
An unchanged click does nothing. Escape cancels an active drag before closing
the detail; closing/reopening resets the audition. Refreshing the same open Spy
retains its committed note-off. Other Spies and live MIDI retain normal timing.

Authoritative behavior: shared EnvelopePlaybackEngine noteOn/noteOff/renderToBuffer
and the existing PreviewTraversal grid schedule. GraphPresentationModel and
PresentationPreviewRenderer forward a capture-only time override through the
scoped diagnostic executor. No second envelope or release algorithm is added.
FFT/IFFT preserve the grid's timing metadata. Pitch/unison traversal follows the
same override, including cache invalidation. Diagnostic caches distinguish
changed note-off settings; realtime execution remains allocation/lock-free.

The detail state owns the drag (constant size: original/preview/committed
coordinates), and SignalProbeDetailView owns hit/draw geometry. NodeCanvas only
routes input and commits an existing probe capture. Paint frames borrow detail
state instead of copying its grid; the detail hashes captured content once on
publication, avoiding per-movement image fingerprint work.

## Completion and deletion targets

- Reuse PreviewTraversal for early note-off, including x=0, x=1 and zero tails.
- Preserve common fixed horizon, Guide noise, scratch/pitch release and DC-free
  harmonic lanes; remove the unconditional note-off-at-one sampling assumption.
- Draw an obvious draggable boundary, support the release icon, and expose a
  resize cursor and automation targets using the same bounds as hit testing.
- Verify multiple drag updates leave render/capture, graph/undo, audio-plan and
  grid identity counters unchanged. Commit exactly once, verify changed signal
  data, cancel/no-op/clamp sequences, then restore x=1.
- Run focused runtime and interaction tests, isolated actual-app fixture and
  screenshot, standalone build, style/diff checks and architecture/code-map
  audits; review production sizes and commit.

## Verification and size review

`SignalProbeDetailState` retains only scalar drag coordinates. Its grid storage
and content revision remain identical through multiple moves, no-op and cancel.
Paint frames borrow the state, and captured content is fingerprinted once on
publication. The release icon now sits on the boundary, with a visible triangle
and thin line over the plot; both icon and line use shared hit/draw geometry.

All 26 focused cases pass (7,837 assertions), including early envelope/scratch
release, pitch lanes, zero/short/long horizons, endpoint clamping, cache return
from audition to normal rendering, existing cursor behavior, harmonic lanes
and realtime preparation/allocation contracts. Both build targets succeed.
The known empty-Guide SignalBuffer assertions remain recorded in audio-bugs.md;
no new app assertion or crash was observed.

The isolated app fixture passes all 29 commands. Eight snapshots prove two
movement updates preserve captured content/render count, mouse-up captures
exactly once and changes content, unchanged clicks and Escape do not capture,
icon dragging clamps at x=1, and closing/reopening resets the audition. All
checkpoints preserve document/model revisions, dirty/undo state, audio-plan
copy count and unrelated preview products. Evidence:
`.context/spy-note-off-tests-final.log`,
`.context/spy-release-audition/report.json`,
`.context/spy-release-audition/run.json`, and
`.context/spy-note-off-expanded.png`.

Architecture/code-map audits and whitespace/style review pass. Large-file
review: GraphAudioExecutor 1289→1304 only forwards diagnostic timing and keys
its existing cache; GraphPresentationModel 688→690 only forwards the capture
setting. NodeCanvas 3615→3649 routes input/capture to detail state, retaining its
existing orchestration boundary. NodeCanvasAutomationInspector 1324→1338
exports presentation facts/targets. NodeCanvasPresentation 1534→1533 borrows
large detail content rather than owning another copy. Gesture state and drawing
remain in the 237-line SignalProbeDetailView implementation; no new graph,
audio lifecycle, envelope or transaction policy enters those large owners.
