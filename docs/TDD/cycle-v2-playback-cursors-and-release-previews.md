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

Spy traversal covers normalized voice time [0,1], followed by the longest
prepared envelope release in the preset. The release proportion varies with the
preset. Release holds the unscratched mesh at x=1; scratch envelopes advance
through their authored release. Guide noise continues changing with column
index. Shared preview traversal coordinates own this policy, independently of
editor surface coordinates. The release span comes from the same routed note-on
preparation and shared release policy used by audio, including preview MIDI/CC
inputs and pitch-envelope bipolar release entry.

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

`PreviewTraversal` places note-off at x=1 and extends the shared time axis by
the longest prepared envelope release. Held and release column counts follow
their durations, with the final column including the release endpoint. Shorter
envelopes finish on that same axis. Unscratched mesh morph holds at 1 while
scratch follows the shared engine's release. Per-column Guide noise seeds keep
advancing. This schedule applies to Spy products, not editor surface grids.
The original fixed-quarter/80–20 sampling path is removed.

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

## Complete-tail timing and cache ownership

`GraphPreviewTiming.cpp` measures the envelope dependency closure through the
existing executor at one sample with grid capture disabled. It prepares the
same routed MIDI/CC note-on shapes as audio; it does not execute a second
full-graph preview or rebuild meshes. `EnvelopePlaybackEngine::releaseDuration`
uses the shared release-entry policy, including the unipolar sustain floor and
bipolar sustain entry. Disabled envelopes do not extend the horizon. All timing
is in normalized voice units: total duration is voice duration multiplied by
(1 + longest release span).

The common horizon travels with diagnostic products and traversal-grid
metadata. Envelope, mesh, pitch and phase previews use it; FFT/IFFT preserve the timeline
metadata while translating signal domains. Compiler pitch
previews retain their ordinary held-note scope; Spy phase composition requests
pitch lanes for the full horizon explicitly. The timing pass is diagnostic-only;
realtime execution retains its allocation/lock-free contract.

Each cached node records its horizon. Changed timing invalidates even nodes
outside the edited node's ordinary downstream closure, including after scoped
captures or cancelled incremental work. Local-editor rendering preserves the
published Spy horizon until deferred probes are refreshed. Oscillator region
reuse now also checks scope membership, preventing cached banks from retaining
Envelope processors evicted during a full-to-scoped preparation change.

Verification: all 16 focused CTest cases pass. They cover short/long/no releases,
disabled envelopes, shared maximum timing, complete endpoints, actual routed
morph timing, scoped and cancelled capture sequences, existing cursor behavior,
Guide noise and realtime allocation/lock counts. The standalone builds and the
27-command app fixture passes. App snapshots confirm different horizons for
Pwm Lead, Brass Section and Icycle. Artifacts: `.context/full-tail-ctest-final.log`,
`.context/full-tail-ui-final-report.json`, and
`.context/full-tail-preset-timelines.json`. Eight failing cases in the broader
Unison/grid selection reproduce with the prior DSP baseline binary; evidence
is in `.context/full-tail-regressions.xml` and `.context/full-tail-baseline.xml`.

The final large-file review keeps measurement orchestration in the dedicated
50-line timing file and release semantics in the shared engine. GraphAudioExecutor
1289 lines (from 1258) only coordinates timing, cache validity and processor
lifetime. GraphCompiler shrinks 1581→1579 by sharing pitch-preview constants.
NodeCanvasAutomationInspector 1321→1322 only exposes the measured duration.
No graph mutations, UI clocks, domain algorithms or per-pixel scalar math were
added to these orchestration owners.

## Expanded Spy envelope annotations

Status: Complete

The expanded Spy reserves a 30px top annotation band. A small ADSR outline
shades attack/decay/sustain above held voice time and shades the descending tail
above release time. A note-off tick aligns those regions to the actual sampled
column boundary, including the preset-dependent longest release. A zero-tail
preview displays only the normal icon. Time and spectrum views retain the same
annotations because both preserve traversal columns.

Ownership: traversal-grid metadata supplies release duration through the probe
DTO; `PreviewTraversal::noteOffPosition` owns the sampled boundary. Detail state
and its existing painter translate those facts into display geometry. No graph
mutation, DSP policy, clocks, or second timing calculation is introduced.

Icon brief:

| Item | Meaning | Silhouette / distinguishing shade | Accent |
| --- | --- | --- | --- |
| Normal | Held voice ADS | Complete ADSR, filled rising/decay/sustain body | Existing blue focus |
| Release | Note-off tail | Same outline, filled descending triangle | Same blue focus |

These are wide data diagrams: noninteractive annotation box 36×30px, vector
canvas 36×24px with 0px horizontal and 3px vertical margins, SVG viewBox 48×32,
live area x=4..44/y=5..27 before the 2.4-unit rounded stroke. Their shared full
outline remains visible; highlight geometry and fill position distinguish the
modes without depending on different colors. SVG sources use the existing
UiIconData pipeline and cached JUCE drawables. No new source boundary is needed.

Visual baseline: `.context/spy-adsr-before.png`. All automated app captures use
coordinator `--clean`, which launches an isolated run PID and unique socket;
the default persistent session can also be the user's interactive instance.

Pass A renders both SVGs at native 36×24 and enlarged 144×96, verifying shaded
regions by pixels: `.context/spy-adsr-icons-pass-a.png`. Pass B inspects actual
time and spectrum details. It exposed the renderer's existing heatmap inset;
that rectangle is now exposed by `NodePreviewRenderer::heatmapBounds` and reused
by the header so the note-off tick aligns with data rather than outer padding.
The shared geometry formula is unchanged.

Pass C reviews the final family sheet and both app views after alignment:
`.context/spy-adsr-icons-pass-c.png`, `.context/spy-adsr-detail-time.png` and
`.context/spy-adsr-detail-spectrum.png`. Normal and release use the same outline,
stroke, margins and colors, with deliberately different filled regions. No
remaining recognition or legibility issue was found.

Verification: both app/test targets build; 12 focused detail and release-timing
cases pass (939 assertions), and the isolated 18-command app fixture passes for
time and spectrum Spies. SVG XML, code-map, architecture and whitespace checks
pass. The existing empty-Guide SignalBuffer assertions are recorded in
`audio-bugs.md`; GUI initialization in the raster review test avoids font-cache
shutdown assertions. Final evidence: `.context/spy-adsr-tests-final.log` and
`.context/spy-adsr-ui-final-report.json`.

Size review: NodeCanvas 3613→3615 and NodeCanvasAutomationInspector 1322→1324
only forward/export two presentation facts; they retain their existing canvas
orchestration and automation responsibilities. Detail state/painter own header
presentation. NodePreviewRenderer 969→973 shares its existing inset geometry;
preview resources, semantic mapping and image caching remain its stable
collaborators. GraphPreviewExecutor 660→661 copies existing grid metadata into
its probe product; it adds no new timing policy or preparation lifecycle.

## Harmonic lanes in spectrum Spies

Status: Complete

Reuse `TrimeshSurfaceRenderer::createTrimeshImage` and its stepped magnitude
bands (the CPU counterpart of shared `Panel3D::drawLogSurface`) for linear-bin
magnitude Spies. Each harmonic retains one color throughout its logarithmic
lane; vertical resampling must not blend adjacent harmonics. Existing Spy
contrast normalization and spectral value scaling remain authoritative. A source
row offset translates FFT's DC-inclusive layout to Trimesh's first-harmonic
layout. Time, phase and non-Spy products retain their current presentation.

`TrimeshRenderProfile` exposes its existing value-only spectral mapping so the
Spy adapter can omit continuous logarithmic interpolation. The shared renderer
owns lane geometry, gradient lookup and note-dependent placement. Cache images
are rasterized at display height, and Spy magnitude draws use nearest-neighbor
resampling. Delete the smooth-frequency mapping path for magnitude Spies; add
no audio/model state, controls, graph mutations or new sampling algorithms.

Acceptance: distinct solid lanes at compact/expanded sizes and multiple notes,
DC exclusion, unchanged gain normalization and source samples, actual-app
spectrum screenshot preserving ADSR annotations, focused tests, build/audits.

Implemented through the existing stepped renderer. `firstHarmonicRow` is a
layout fact (zero for Trimesh, one for FFT-inclusive Spies); the original
Trimesh band boundaries and gradient lookup are unchanged. Spectral value
mapping has one owner in `TrimeshRenderProfile`, and its former private grid
wrapper is removed. Both preview image caches and preset thumbnails request
final display height and preserve sharp row boundaries while scaling.

The native-image test alternates silent/full harmonics, with a large DC value,
at notes 36/60/84 and heights 64/537. Every pixel is exactly one of the two
source gradient colors, the fundamental lane remains silent despite DC, both
colors remain present, and source samples are unchanged. Existing contrast,
Trimesh-band and ADSR/timing checks remain covered. The isolated 18-command app
fixture passes and the expanded spectrum visibly retains separate lanes with
its ADSR header: `.context/spy-harmonics-expanded.png`.

Both build targets and architecture/code-map/whitespace checks pass. The broad
27-case selection has three preexisting failures (five assertions), all
confirmed in the prior baseline executable and recorded in `ui-bugs.md`.
Evidence: `.context/spy-harmonics-tests-focused.log`,
`.context/spy-harmonics-ui-report.json`, `.context/spy-harmonics-baseline.log`,
`.context/spy-harmonics-baseline-compact.log`.

Size/responsibility review: NodePreviewRenderer 973→1004 retains its existing
preview product adaptation and image-cache ownership. Harmonic geometry stays
in TrimeshSurfaceRenderer (191→196), while TrimeshRenderProfile shrinks 407→382
by removing its redundant mapping wrapper. No audio preparation, graph state,
interaction lifecycle, or new family of domain decisions moves into UI code.
