# Performance waveform and textured keys

Status: Complete (2026-11-07).

Replace the keyboard's progress strip with a right-hand density waveform using
ReferenceWaveformView, pinned to the existing Blues gradient on the same grey
background as the bar. Let the keys use
that vertical space without increasing the 56 px bar. Keep the current layout
and mapped-controller behavior. Add stable, subtle grain and broad tonal washes
to keys; retain dark-natural/near-black-accidental contrast and readable states.

Reuse SignalProbeWaveformCapture on the prepared snapshot's default output
observation point. A cohesive PerformanceWaveformPreview owns a worker for the
existing private-audition capture and the shared waveform child; no new DSP or
density algorithm. Compare immutable source identities before scheduling, copy
the prepared plan only when accepting a changed source after a gesture, retain
the previous image until publication and reject stale jobs. Never capture or
analyze from paint, cursor updates, or the audio thread.

Reuse PlaybackCursorExchange for independent bounded voice timeline positions
alongside the existing editor-specific cursor data. NodeWorkspace reads once
and distributes that frame to both consumers. Preserve editor selection and
rendering. Positions come from real audio voices, including releases and voice
retirement; no UI timer approximates playback. Only the overlay repaints during
ordinary playback. Curve/mesh/controller gestures keep the cached waveform
until release; reference/Spy palette choices remain independent of this Blues
preview. Expose waveform diagnostics for cache/publication/cursor assertions.

Verify polyphony, release/retirement, unchanged editor cursor behavior, waveform
capture reuse, stale rejection, no waveform rebuilds on cursor-only frames,
commit/undo refresh, density identity on resizing and fixed palette behavior.
Build the exact app/tests, run focused and in-app checks sequentially, inspect
normal/compact and playing captures, run architecture/code-map/style checks,
and commit the completed slice.

## Implementation and ownership review

PerformanceWaveformPreview owns asynchronous capture handover and the shared
waveform child; its worker delegates the full audition to the prepared snapshot's
SignalProbeWaveformCapture. Source identity, pitch and controller values gate
requests. A changed prepared plan is copied once at the accepted post-gesture
audition boundary for worker lifetime safety. Cancelled requests cannot publish,
and readiness includes both capture and density publication. Old pixels remain
visible until their replacement is ready. Below 120 px available preview width,
the view hides and keeps the cached source for its return.

ReferenceWaveformView adds an optional fixed palette, leaving ordinary reference
and Spy behavior unchanged. This preview pins Blues and paints its backing with
the toolbar's existing grey. Waveform and voice overlays remain separate children.
RealtimeGraphRenderer adds two scalar clocks per existing bounded voice and
publishes those positions through the existing atomic PlaybackCursorExchange;
DSP control clocks, editor cursor selection and synthesis are unchanged. Release
positions map to the audition's release region and retire with their voices.

The progress drawing and bounds API are deleted; transport progress remains only
as playback state. Keys use 3 px opposing vertical insets instead of 6 px, giving
50 px keys in the same 56 px bar. Their deterministic grain/wash image is prepared
once per keyboard, clipped over the existing vector keys with low opacity, and
varied by note. No noise generation occurs in paint or on the audio thread.

NodeWorkspace remains over the file-size review threshold (1,057 lines); its
addition only routes the existing cursor frame and eligible prepared source to
the widget. Capture, analysis, rendering and overlay policy stay in their owners.
NodeCanvas gains only a named source-eligibility query in the existing audition
translation unit. RealtimeGraphRenderer remains below 800 lines. The new preview
implementation is about 150 lines; key presentation stays below 300 lines. Broader
canvas/workspace decomposition remains recorded in refactors.md.

## Verification

Exact standalone and test targets build. The focused waveform, playback-cursor,
keyboard and status selection passes 138,265 assertions in 67 cases. It covers
polyphony independent of editor selection, release/retirement, retained density
under cursor updates, fixed Blues despite global palette changes, resizing without
source analysis, and rejection of superseded captures.

`scripts/test_cycle_v2_performance_waveform.py --no-build` passes in an isolated
exact app instance. During two-voice playback, capture requests/publications and
source/heat/geometry build counts stay unchanged. Commit and Undo refresh the
preview; resizing preserves source analysis. A real held MOD drag keeps capture
requests fixed until mouse-up, then publishes the refreshed waveform. Normal
and narrow-window layout checks preserve 50 px keys and the 56 px band.

Evidence: `.context/performance-waveform-tests.log`,
`.context/performance-waveform/results.json`,
`.context/performance-waveform/performance-waveform-playing.png`, and
`.context/performance-waveform/performance-waveform-idle.png`.
Normal-size native captures were inspected, including the final grey backing.
Architecture audit, semantic code-map, production style/diff and whitespace checks
pass. No new incidental assertions or runtime crashes were observed.
