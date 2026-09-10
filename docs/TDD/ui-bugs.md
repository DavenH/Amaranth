# Cycle V2 UI Bug Notes

## Remaining priority

There are no open deterministic P0 regressions as of 2026-09-09.
Resolved and no-longer-reproducing entries have been removed from this ledger.

1. **P1 — Guide noise is constant across spectral traversal-grid rows.**
   Reproduce the compact Trimesh and Spy output together, then compare row
   arrays before changing either presentation or audio behavior.
2. **P2 — Intermittent CoreMIDI endpoint assertion during automation startup.**
   Keep this behind reproducible product and automation failures because it has
   not affected fixture results and does not currently reproduce.

## P1: Guide noise is constant across spectral traversal-grid rows

Context:

- Reported 2026-09-09 from a compact spectral Trimesh with a noisy Guide
  assignment. The visualization shows the same noise sequence repeated across
  frequency rows, producing horizontal bands instead of row-varying detail.
- A Spy attached to the Trimesh output shows the same structure. This makes a
  stale compact-node image or colour-mapping-only defect unlikely; the captured
  traversal grid itself may contain repeated row data.
- Recent work corrected Guide/morph behavior in the live audio pipeline, but no
  causal link has been established. Realtime audio behavior has not yet been
  compared with this preview result.

Current status: open. Add a focused noisy-Guide fixture and an array-level
assertion that deterministic seeding remains repeatable while successive
spectral rows receive distinct noise samples. Trace the mature Guide sampler
before changing the traversal-grid implementation, and verify the realtime
audio path independently so a preview correction does not create audio drift.

## P1: First Spy on a spectral Trimesh side branch appeared disconnected

Resolved 2026-09-09. Adding the first Spy to a spectral Trimesh output could
leave the tile labeled `Disconnected` until a second downstream Spy was added.
Probe-only invalidation started at the graph's first execution node; an
oscillator-region side branch was not necessarily downstream of that node, so
the first probe preview was never rebuilt. Probe changes now invalidate every
authored probe source, with the existing first-node fallback retained for
removal of the last probe.

The same report exposed inconsistent magnitude presentation. A Spy attached
directly to a Trimesh used the linear mesh-authoring colour scale, while a Spy
after Add used the logarithmic spectral-output scale. Both carry the exact DSP
grid—the Spy itself applies no gain—but the direct tile looked roughly 100
times weaker. All Spy tiles and details now use the output-observation scale;
compact Trimesh nodes continue to show the authored mesh surface.

Focused coverage exercises the first spectral side-branch Spy through canvas
authoring, verifies its immediate connected preview, removes it through undo,
and checks the Trimesh-output heatmap against the spectral Spy scale.

A follow-up fixed the same invalidation gap for topology edits. Deleting the
Voice Context scratch cable recompiled the graph but initially dirtied products
only from the graph's first execution node, leaving observed spectral side
branches stale. Every topology compilation now also invalidates each active
probe source. A complete canvas-authoring regression deletes a scratch cable
and verifies that the still-connected Trimesh Spy changes immediately, then
undoes the deletion and verifies the original grid is restored. The
production `scratch-test` automation capture likewise shows all three Spy tiles
updating after the context scratch edge is removed.

## P2: Intermittent CoreMIDI endpoint assertion during automation startup

Context:

- A sequential property-control fixture run on 2026-08-27 completed every
  Reverb command successfully but logged CoreMIDI error `580` and three JUCE
  assertions at `juce_CoreMidi_mac.mm:595` during application startup.
- The five adjacent Cycle V2 fixture launches did not report the assertion,
  and the issue was independent of property value formatting or slider paint.
- Original repro artifacts were
  `/private/tmp/cycle-v2-reverb-property-controls-precision-logs.txt` and its
  `.raw` companion.

Current status: open but not reproduced on 2026-09-06 across the native
Envelope smoke, focused Cycle V2 hover-help and Guide-dock launches, or five
consecutive launches of the Reverb property-control fixture. All five Reverb
runs completed with zero failed commands. Capture the initialized endpoint
names and launch sequence if it recurs before changing MIDI initialization or
teardown.

## P2: Cycle 1 menu smoke requests the removed Graphics menu

Context:

- An incidental smoke run on 2026-09-06 listed the current menus successfully,
  then `cycle-agent-menu-commands.json` failed with `Menu not found: Graphics`.
- The application now exposes the related display commands under `View`; the
  audio-pipeline changes do not alter menus or the fixture.
- Repro report:
  `/private/tmp/cycle-agent-audio-parity/cycle-agent-smokes-report.json`.

Current status: open; update the menu fixture against the intended current menu
contract in a focused UI slice.

## P2: Drunkard pitch render logs VisualDsp column-size assertions

Context:

- The focused pitch-envelope audio render passes its signal thresholds but logs
  three assertions at `VisualDsp.cpp:367` because source and destination column
  sizes differ.
- The assertion is in visualization column copying, outside the audio voice and
  envelope paths changed by the parity fix.
- Repro log: `/private/tmp/cycle-agent-pitch-envelope-log.txt`.

Current status: open; reproduce through the visual update path and decide which
column resolution owns resampling before changing the assertion.

## P2: Graph document save test cannot use JUCE's default temporary directory

Context:

- The full Cycle V2 suite and a focused rerun on 2026-09-07 fail
  `Graph documents save canonical JSON with stable line endings` at
  `TestGraphSerializer.cpp:111`.
- JUCE first asserts at `juce_TemporaryFile.cpp:126`; the destination is chosen
  from `File::tempDirectory`, and `GraphDocument::save()` then returns false.
- Preset serialization itself passes in the workspace-backed native migration
  runs, including 221 load/save/reopen cycles.
- Full-suite log: `/private/tmp/cycle-v2-full-tests.log`.

Current status: open as a sandbox/test-fixture path issue. Give temporary-file
fixtures an explicitly writable test root rather than weakening document-save
behavior.

## P1: Cycle 1 Calming Keys preset crashes during visual refresh

Context:

- The Cycle 1 factory-library migration sweep opened and exported 28 presets,
  then crashed while opening `cycle/content/presets/calming-keys.cyc`.
- The crash is an invalid `dynamic_cast` read in
  `EnvRasterizer::renderWaveformOnly()` reached from
  `UnisonPhaseColumnRenderer`, `VisualDsp::processFrequency()`, and the pending
  UI update graph. It occurs after the document load starts scheduling visual
  work; the preset's canonical migration itself is not yet implicated.
- Repro artifacts are `/private/tmp/cycle-v1-preset-migration-session.log` and
  `/Users/daven/Library/Logs/DiagnosticReports/Cycle-2026-09-07-111853.ips`.

Current status: open. Update suppression was insufficient because live document
application still touched UI-owned state, and `cluck-2.cyc` exposed the same
class of failure. The migration exporter now decodes and migrates the source
without applying it to the live document, isolating canonical export from
editor, updater, rasterizer, and audio lifecycles. Reproduce normal interactive
loads separately before changing Envelope or Unison rasterization ownership.

## P1: Cycle V2 graph replacement races Envelope preview interaction state

Context:

- The Cycle 1 preset migration verification opened the converted `crash`
  graph successfully, then Cycle V2 crashed on its OpenGL renderer thread.
- The invalid read starts in `Interactor::getModPosition(bool)`, reached from
  `Interactor::updateSelectionFrames()`,
  `EnvelopeCurvePanel::setEnvelopeAxisLinks()`, and the node-preview render
  path while the newly loaded graph is being presented.
- The graph had already parsed without validation errors; the failure is in
  editor/preview lifetime synchronization after document replacement, not in
  converter serialization or Pan compilation.
- Repro artifacts are
  `/private/tmp/cycle-v2-migration-final-session.log` and
  `/Users/daven/Library/Logs/DiagnosticReports/CycleV2-2026-09-07-201842.ips`.
- A manual load of migrated `ooh-2.cyclegraph` reproduced the same stack on
  2026-09-08. Its report is
  `/Users/daven/Library/Logs/DiagnosticReports/CycleV2-2026-09-08-100335.ips`:
  `EXC_BAD_ACCESS` on the OpenGL renderer thread at
  `Interactor::getModPosition(bool)`, called while
  `EnvelopeCurvePanel::setEnvelopeAxisLinks()` synchronized a preview.

Resolved 2026-09-09. Loading `downfall.cyclegraph` produced the same crash and
exposed the precise initialization fault: `Interactor::positioner` was an
uninitialized raw pointer before `Interactor::init()`. Pre-host envelope sync
correctly guarded a null positioner, but indeterminate storage could pass that
guard and enter `updateSelectionFrames()`. The pointer now initializes to null,
so selection work is deferred until the preview host initializes the
interactor. The `cycle-v2-agent-downfall-open` fixture covers repeated graph
replacement through `downfall` and `ooh-2`; the focused pre-host widget test
covers synchronization with Downfall's unlinked envelope axes.

## P1: Expanded Trimesh morph controls lost pointer capture during drag

Resolved 2026-09-08. A mouse-down changed the selected morph value, but further
drag movement did not follow the pointer. Each transient morph update rebound
the entire expanded editor, replacing interaction state during the native
gesture. Successful updates now mirror the active parameter into the existing
editor while `GraphCommandDispatcher` continues to own transient publication,
commit, and undo. The `trimesh-morph-drag` native automation sequence covers a
multi-step drag; `TestNodeEditorHost` covers two transient updates, commit, and
undo.

## P2: Cycle V2 domain-context fanout cables lack obstacle-aware routing

Status: Resolved on 2026-09-07

The migrated graphs connect `Voice Context.context` directly to every time,
magnitude, and phase mesh. With several time layers, a later context cable can
cross an earlier mesh node even when the ordinary audio/control graph has a
clean left-to-right layout. The canvas currently has neither obstacle-aware
cable routing nor a visual fanout/bundle for domain-context distribution.

The migrated-preset cable-crossing assertion intentionally covers ordinary
audio/control signals and excludes domain-context, configuration-attachment,
and processing-attachment routes until this routing support exists.

## P2: Inline cable Pan cannot be removed from the canvas

Status: Open

Once a cable has the inline Pan/headset operation, the canvas provides no
discoverable way to return it to an unpanned direct connection. Right-clicking
the headset should offer a `Stop Panning` action that removes the inline Pan
node, reconnects its incoming and outgoing signal edges, and publishes the
semantic edit through `GraphCommandDispatcher` with undo/redo support.

The edge context menu now switches from `Add Panning` to `Stop Panning` for
either segment adjacent to the inline Pan. Removal and reconnection are one
compound dispatcher command, and undo restores the Pan and both cable segments.

## P1: Curve previews survive preset replacement with stale pixels

Resolved 2026-09-09. After switching presets, Guide shelf tiles could remain
black and a compact Waveshaper could show the previous preset's curve while its
expanded editor showed the current model. The persistent Curve widgets keyed
their OpenGL preview reuse by node/resource id and numeric revisions, which can
repeat in another document, while document replacement cleared only the outer
canvas sprites. The replacement lifecycle now invalidates each Curve host's
render key, clears both framebuffer snapshots, advances its presentation
identity, and schedules retained Guide widgets for a fresh render. A focused
node-editor-host regression covers snapshot clearing and identity advancement.

## P1: Cycle V2 reported crash transitioning to Cello Vibrato

Context:

- On 2026-09-09, Cycle V2 reportedly exited while opening
  `cello-vibrato.cyclegraph` after another preset had already been loaded.
- Recent-file history identified `solo-string-2.cyclegraph` as the immediate
  predecessor and `time.cyclegraph` before it. The Cello graph contains local
  authoring edits, including envelope topology and a Reverb node.
- The focused `cycle-v2-agent-cello-vibrato-open` fixture covers
  `time -> cello-vibrato -> solo-string-2 -> cello-vibrato`, including an
  active voice during the first replacement and live playback afterward.
- Twelve repeated fixture processes completed without a failed command or a
  fresh `.ips` report. A Guide popup left active across replacement and the
  attached-Guide Trimesh editor also survived in separate focused runs.

Resolved 2026-09-09. The transition reproduced under LLDB with a persisted
Envelope `selectedCubeId`. Compact preview synchronization restored that cube
before the lazy panel host had initialized `Interactor::positioner`, then
`updateSelectionFrames()` dereferenced the null pointer. The apparent hang was
the debugger stopping at the access violation; its main thread was waiting for
JUCE's OpenGL message-manager lock. Envelope selection restoration now defers
the selected cube until the existing panel initialization lifecycle completes.
The focused widget regression covers pre-host synchronization and verifies that
the selection is restored after host initialization. Live evidence is in
`/private/tmp/CycleV2-hang-2026-09-09-2206.txt`; macOS could not create an
`.ips` because ReportCrash logged `Log limit exceeded`.

## P2: African Horn factory graph is not canonical JSON

Status: Open

The full Cycle V2 suite currently fails `Every shipped graph is canonical JSON
and compiles` on `african-horn.cyclegraph`. The graph compiles, but a
deserialize/serialize pass changes its JSON representation. This predates and
is independent of the document-declick changes; regenerate that preset through
the canonical serializer without expanding unrelated preset diffs.

## P2: Envelope parameter-rail spacing regression

Status: Open

The full Cycle V2 suite on 2026-09-10 fails the Envelope editor geometry
assertion in `TestNodeEditorHost.cpp`: adjacent parameter rails are 39.825
pixels apart instead of the specified 39.1 +/- 0.02 pixels. This appeared
incidentally during realtime note-on envelope verification and is unrelated to
audio materialization.
