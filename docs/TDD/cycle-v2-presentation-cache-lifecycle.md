# Cycle V2 Presentation Cache Lifecycle

Status: implemented on 2026-08-31; document-replacement curve reset completed
2026-09-09

## 2026-10-21 Guide audio publication follow-up

The reed study exposed a second document-lifecycle cache: reopening the
original graph and repeating the same Guide-noise edit could retain the noisy
audio plan. Full compilation cleared prepared-product fingerprints, but the
semantic request gate still remembered that node stream's previous edit.
It could reject the repeated request before configuration publication. Separately,
Guide automation returned while its asynchronous refresh was still pending, so
an immediately following scheduled capture could copy the preceding plan.

After cancellation and full compilation, `PresentationRefreshScheduler` now
resets both product fingerprints and request identities. `SemanticEditGate`
invalidates effective-state comparisons while retaining monotonic identities,
audit and active-gesture cancellation baselines. The session discards pending
request identities without discarding dispatcher-owned graph gestures. This
reset belongs to the existing compilation lifecycle, including graph-semantic
changes; ordinary Guide movement retains its asynchronous update behavior.
Committed Guide automation uses the existing synchronous canvas refresh boundary,
so a later capture sees its prepared configuration before the command returns.

The added work scales with cached source streams only at full compilation.
There is no movement-time graph/mesh copy, replacement DSP, or new publication
owner. The large canvas retains only its existing semantic-command and refresh
adapter; gate/cache policy stays in the runtime session/scheduler collaborators.
The old product-only reset and asynchronous committed-automation refresh are
replaced, rather than kept as competing paths.

The focused regression renders actual PCM before/after the same Guide edit,
reopens the original document and repeats it. Before the fix, the second edit
produces unchanged noisy PCM; evidence is `.context/guide-reopen-before.txt`.
Afterward, incremental PCM matches a freshly compiled current graph without
an extra topology compile. The focused reset/PCM/gesture checks pass 67
assertions in three cases; adjacent causal, Guide and presentation-session
checks pass 502 assertions in 38 cases. Separate checks protect active gesture
commit, cancellation, one undo and its original effective-state baseline.

The rebuilt owned app passes immediate capture after noise-zero publication
and after reopening/repeating the edit. Both zero-noise captures have PCM hash
`81e6acea607efa38fcc23a3b14f11b83bd1b600f559219417fee973c4c7b1253`;
the noisy baseline differs. The verification also retains exact seeded repeats
and zero-breath silence. Evidence is
`.context/sax-reed-core/ready/verification.json` and its command log, with no
export/reopen workaround. Architecture, style, code-map and diff checks pass;
no additional DSP or graph-mutation path was introduced.

## Problem

Presentation caches can outlive the graph content that produced them. This is
visible as stale Spy tiles after loading another preset, invalid first-frame
Envelope/Waveshaper/IR snapshots, and Guide preview renders flashing at the
canvas origin. Guide edits in `Live` mode also synchronously recompile the
graph, preventing repeated latest-state feedback.

## Authoritative Implementations

- `GraphPresentationModel` owns compiled plans, incremental preview execution,
  latest-generation publication, and the durable presentation snapshot.
- `GraphCommandDispatcher` owns transient Guide publications and their
  consolidated `GraphChangeSet`.
- `CurvePanelHost` owns OpenGL curve rendering and framebuffer snapshot
  publication. Its snapshot is valid only after the widget has synchronized
  the current graph model and completed that render.
- `NodeCanvasPresentation`, `NodeCanvasNodeLayerCache`, and
  `SignalProbePreviewTileCache` own raster presentation caches only. They must
  not decide graph or preview semantics.

## Design

1. A full document replacement explicitly resets presentation-only caches.
   Cache entries may reuse node/probe ids and numeric revisions within a new
   document, so those values are not a document identity. This reset reaches
   both canvas sprites and the persistent Curve widget render/snapshot caches,
   including Guide shelf widgets; retaining the widgets must not retain pixels
   from the previous document.
2. Curve widgets synchronize the current graph model at the existing OpenGL
   preview-render boundary before snapshot capture. Model synchronization may
   precede lazy panel-host initialization, so interaction selection restoration
   is deferred until the host initializes the interactor and its morph
   positioner. A default-model snapshot must not become the first authoritative
   preview for a loaded node.
3. Guide preview readback retains the panel renderer's proven capture bounds,
   then immediately restores the OpenGL canvas underlay before publication.
   The temporary render therefore cannot leak at the canvas origin while the
   JUCE layer catches up.
4. Guide content changes preserve the compiled topology. They refresh the
   affected Trimesh configurations and preview products. `Live` movement uses
   the existing coalesced asynchronous latest-generation scheduler; commit
   persists once and supersedes stale movement work.

## Boundaries

- No cache duplicates model, graph traversal, rasterizer, or DSP behavior.
- No UI code mutates the graph outside `GraphCommandDispatcher`.
- Cache reset is a lifecycle operation, not a substitute for comparing
  ordinary same-document content dependencies.
- Guide add/remove/assignment remains expressed by the existing semantic
  change set; the compiled node/edge topology is unchanged.

## Verification

- Load African Horn and then Baroque Flute in one process; the second snapshot
  exposes only Baroque Flute probe values and freshly painted tiles.
- A loaded curve node is synchronized before its first cached OpenGL preview.
- Replacing a document invalidates the inner Curve render key, clears preview
  and expanded snapshots, changes the persistent widget presentation identity,
  and schedules every retained Guide preview for a fresh OpenGL render.
- The OpenGL underlay is restored in the same frame after Guide readback.
- Two Guide updates in one transient gesture change the Spy payload twice,
  compile zero additional times, commit once, and undo once.
- The focused Live-spy automation performs two updates and observes distinct
  probe metrics after each publication.

## Completion Criteria

- All four reported regressions have focused coverage.
- Presentation cache reset is wired to graph-file and snapshot replacement.
- Guide movement no longer recompiles topology or blocks the message thread on
  synchronous preview work.
- Relevant tests, focused automation, screenshot capture, style checks, and a
  coherent commit are complete.

## Verification Evidence

- `[cycle-v2][runtime][guides]`: 26 assertions in 2 cases passed.
- `[cycle-v2][canvas][performance][cache]`: 92 assertions in 5 cases passed.
- `[cycle-v2][canvas][presentation]`: 76 assertions in 8 cases passed.
- `cycle-v2-agent-guide-editor-live-spy.json` passed.
- `cycle-v2-agent-preset-cache-reset.json` passed.
- OS captures:
  `/private/tmp/cycle-v2-preset-switch-after2.png`,
  `/private/tmp/cycle-v2-alto-curve-preview.png`, and
  `/private/tmp/cycle-v2-stengah-curve-preview.png`.
- `git diff --check` passed. `clang-tidy` was unavailable in the environment.

## 2026-09-09 Regression Closure

Preset switches could reuse a blank Guide tile or an old compact Waveshaper
snapshot when the next document reused the same resource/node id and numeric
model revision. `NodeCanvas` cleared its outer sprite caches, but not the
`CurvePanelHost` render key or framebuffer snapshots owned by persistent Curve
widgets. Full document replacement now resets those existing owners without
destroying their OpenGL hosts. The focused node-editor-host regression covers
snapshot clearing and the new presentation identity boundary.

The `cycle-v2-agent-curve-preview-preset-reset.json` fixture switches between
documents that reuse `waveshaper` and `guide1`, then verifies the replacement
graph compiles without validation issues. The production-size OS capture at
`/private/tmp/cycle-v2-curve-preview-preset-reset.png` shows both the current
compact Waveshaper curve and a non-black Guide tile after the switch.

## 2026-09-09 Envelope Selection Follow-up

The earlier pre-host Envelope fix initialized `Interactor::positioner` to null,
but persisted editor selection still called `updateSelectionFrames()` before
the lazy preview host initialized that pointer. Envelope selection restoration
now retains the selected cube as pending interaction state and applies it from
the panel's existing initialization lifecycle. Compact preview synchronization
continues to reuse the authoritative controller and interactor without creating
a second preview-only selection path. The selected-cube pre-host regression
passes with 7 assertions, the remaining Envelope-tagged tests pass with 510
assertions across 36 cases, and four focused Cello transition runs completed
without a failed automation command or crash.
