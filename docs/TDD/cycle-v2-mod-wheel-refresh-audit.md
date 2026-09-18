# Cycle V2 Mod Wheel Refresh Audit

## Status

In progress (2026-09-17). The focused On Release repair is implemented and
verified; Live commit deduplication and the Filter Saw 2 audio A/B remain open.

## Evidence

`scripts/fixtures/cycle-v2-agent-mod-wheel-refresh-audit.json` holds preview
note 73, moves the wheel from 0 to 114, and captures snapshots around release.
The 2026-09-17 baseline report at
`/private/tmp/cycle-v2-mod-wheel-refresh-before-long.json` shows no preview
render during movement, one immediate render after release, and a second after
the delayed refresh. Canvas telemetry reports two preview requests and two
publications. The held note is still active after the immediate publication,
then `previewPlaying` becomes false and the audio graph revision advances from
3 to 4 after the delayed publication. A single run measured an 80 ms synchronous
refresh and a 203 ms worker job; these are diagnostic timings, not acceptance
thresholds.

The direct cause of the duplicate request is visible in `NodeCanvas`:
`setPreviewModWheelValue` commits `setPreviewMorph`, which schedules a compiled
refresh, then explicitly calls `refreshPreviewModWheelValue`. The scheduled
refresh later processes the same semantic wheel release. The held-note stop is
also explicit: `NodeWorkspace::publishAudioPlan` releases keyboard notes when
the plan revision changes, and `RealtimeGraphRenderer::setPreparedGraph`
resets voice state on plan replacement. Removing only the keyboard release
would therefore not preserve sound.

The held-note fixture also exposed an independent async-publication defect:
`GraphPresentationModel::publishAsyncRefresh` advanced `audioRevision` only
when an `AudioConfiguration` product appeared in the executed set, while its
synchronous path used the accepted change's DSP impact. A DSP parameter edit
could therefore refresh the display without publishing a new audio plan.
Both paths must use the accepted semantic change to decide audio-plan revision.

The post-repair Honerism 3 fixture reports one preview request and publication,
zero synchronous refreshes, no preview work during movement, and a held note
that survives the refresh. The audio-plan copy counter stays at 2 throughout
the note and advances to 3 after the note ends; the renderer then adopts graph
revision 4. One measured worker refresh took 192 ms, with no audio deadline
overruns or telemetry drops during the gesture. These timings are evidence, not
fixed pass/fail limits. The existing Honerism Spy fixture also passes with an
asynchronous settle before its post-release assertion.

## Authority and design constraints

`PerformanceKeyboard` and `MidiControlState` own live CC1 delivery. The graph
dispatcher owns durable saved morph edits. `GraphPresentationModel` and
`NodeUpdateGraph` own prepared preview products and their causal trace;
`StandaloneAudioEngine`/`RealtimeGraphRenderer` own active voice state. Reuse
those paths rather than adding a wheel-only render or audio engine.

One release should normalize the final CC1 and saved morph once, request one
causal refresh containing the union of the affected preview and durable audio
products, and publish each affected Spy product once. Movement remains O(1)
in On Release and latest-only asynchronous in Live. The gesture must not copy
or serialize the graph or mesh per movement. Do not silently suppress required
audio configuration updates to make the Spy count pass.

Preserving a held note across an audio-plan revision requires an explicit
compatible live-adoption policy or a bounded deferral until that note ends.
It cannot be achieved by retaining the keyboard's UI state while the renderer
resets voices. The current narrow policy defers compatible plan replacement
while any audition or realtime voice remains active, then publishes the newest
plan when the voice finishes. Device preparation changes remain immediate and
may release voices because the old plan can no longer be assumed compatible.
This policy belongs to the workspace audio-publication boundary, not the
Mod Wheel widget. A future Voice Context live-adoption design may supersede it.

## Completion checks

- On Release: movement requests no preview work; release produces one preview
  request/publication and one Spy update after the worker settles.
- Live: repeated wheel movements remain latest-only; commit does not repeat
  the final current preview product.
- The held preview note remains audible and retains lifecycle state through a
  compatible wheel/morph update; explicit note-off still stops it.
- A focused Filter Saw 2 capture checks two CC1 positions before closing its
  older audio-morph report. Do not infer audio correctness from Spy movement.
- Canvas and audio telemetry, causal trace identities, operation counters,
  focused tests, and the UI fixture pass without extra graph/mesh copies or
  unrelated product rebuilds.

The broader single-policy/session extraction remains in
`cycle-v2-causal-update-graph.md` and follows this focused repair.
