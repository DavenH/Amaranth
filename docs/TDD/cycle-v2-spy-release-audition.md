# Draggable Spy note-off audition

Status: Complete

## Contract and ownership

Drag the expanded Spy boundary or release icon to a normalized voice time x in
[0,1]. The full voice-plus-longest-release horizon stays fixed while auditing
staccato notes. An unscratched morph holds the chosen slice; scratch and pitch
envelopes use the shared EnvelopePlaybackEngine note-off playback.

Each Spy stores its committed time in PresetPresentation.spyNoteOffTimes.
GraphCommandDispatcher changes presentation metadata only on pointer-up. This
marks the preset dirty without graph publication, undo, audio-plan updates or
compilation. Unchanged clicks do nothing. Escape cancels a drag before closing
the detail. Collapsing, reopening and switching preview views retain the value;
saving and loading a preset restores each Spy independently. Missing settings
mean x=1, and restoring x=1 removes the override. Live MIDI retains normal timing.

SignalProbeDetailState owns constant-size original/preview/committed drag
coordinates. Movement changes only these scalars and repaints the detail; it
never captures, hashes or copies a grid. Paint frames borrow the detail state.
Pointer-up captures the selected probe once and fingerprints the published detail
once. PresentationPreviewRenderer derives compact time/spectrum products from
that same capture. GraphPresentationSnapshot carries the settings into downstream
refreshes, recapturing only auditioned probes whose compact products changed.
Other node previews and Spies retain their own timing.

GraphPresentationModel and PresentationPreviewRenderer forward capture timing
through the scoped diagnostic executor. PreviewTraversal supplies the shared
sampling schedule; FFT/IFFT retain its metadata. Pitch/unison traversal and cache
invalidation use the same override. No second envelope algorithm or audio-thread
allocation is introduced.

## Native input and rendering corrections

The detail was painted relative to canvas content but hit tested relative to
editor content, which excludes the sidebar. This offset the handle by the sidebar
width contribution. NodeCanvas now computes one rectangle for paint, hit testing,
occlusion and automation. Both the visible line and the release icon accept
native drags.

The former handle hover message was suppressed by overlay text policy while
still requesting a status repaint. The status-only paint shortcut omitted the
Spy backdrop, exposing an empty undimmed console rectangle. Remove that hover
message and use the normal clipped paint path whenever an overlay is open.
Native screenshots verify identical blank status-background pixels before and
during dragging, including a transition through the information button.

Both ADSR SVG annotations use moderate attack, decay and release curves. Their
shared 48×32 canvas has live x=4..44, y=5..27 and stroke 2.4; the UI renders at
36×24. The held region is shaded in Normal; only the tail is shaded in Release.
The release icon sits on the boundary. Compact Spies also draw the committed
release boundary over the rendered product.

## Verification

- Both standalone and test targets build. The 28 focused cases pass with 7,890
  assertions, covering traversal, envelope/scratch/pitch release, cache timing,
  harmonic lanes, drag/no-op/cancel/clamping, compact retention, per-Spy preset
  round trips, unchanged node products and realtime preparation contracts.
- Another 16 presentation/serialization cases pass with 293 assertions.
- The isolated semantic fixture passes 35 commands and ten checkpoints. Movement
  preserves captured content and counts; commit captures once; compact/reopened
  views retain the value. Model revisions, undo, audio-plan copies and unrelated
  previews remain unchanged; only a completed changed drag marks metadata dirty.
- The isolated native test verifies both line and icon, actual mouse delivery,
  capture deferral, compact/reopen retention, preset save/load, independent preset
  defaults and status-background stability. ScreenCaptureKit targets the exact
  isolated PID; the user's running instance is untouched.
- SVG XML validation, icon coverage/shading, three raster review passes,
  architecture/code-map audits and whitespace checks pass. No new runtime
  assertion or crash was observed. Existing empty-Guide SignalBuffer assertions
  remain recorded in audio-bugs.md.

Evidence: `.context/spy-corrections-tests.log`,
`.context/spy-corrections-presentation-tests.log`,
`.context/spy-release-audition/report.json`,
`.context/spy-release-audition/native-report.json`,
`.context/spy-envelope-curves-pass-c.png`, and
`.context/spy-release-audition/native-releaseIcon-panel.png`.

## Size and responsibility review

NodeCanvas 3649→3663 adds shared bounds, a metadata command and preview routing;
NodeCanvasPresentation 1533→1534 only consumes those bounds. Their existing
orchestration roles stay intact; gesture and domain algorithms remain delegated.
NodeCanvasAuthoring 1043→1045 restores settings at its existing document-load
boundary. NodeCanvasAutomationInspector 1338→1344 only exports preview metadata.
GraphCommandDispatcher 827→849 follows its existing presentation-command pattern.
GraphPresentationModel 690→706 publishes selected products and session snapshots;
the 356-line PresentationPreviewRenderer owns compact product derivation and
refresh. SignalProbeCanvas adds only boundary drawing. No new cross-subsystem
utility, duplicated envelope state machine or unrelated graph work is introduced.
