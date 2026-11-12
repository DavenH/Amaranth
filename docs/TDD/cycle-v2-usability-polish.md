# Canvas usability and monitoring polish

Status: Complete

## Contract and owners

- NodeWorkspace/ReferenceTrayBar retain tray lifecycle, import, drop and asset
  browsing. Replace the permanent 30px strip with an overlaid 190x28 disclosure
  tab at the bottom-left of the canvas content. Expanded actions belong to the
  tray header. Hidden tray takes no layout height. Title: Visual Reference.
- InlinePresetBrowser retains tag editing, favorites, rename and confirmed Trash
  policy. Cards prioritize title and waveform: one quiet tag rather than four
  pills; hover exposes a 22px Trash target without selecting/opening the preset.
  Label tag editing explicitly and name the compact footer Preset Manager.
  SidebarMediaRow owns shared card geometry; SidebarLibraryToolbar owns actions.
- Existing playback overlays retain bounded audio frames and GL ownership.
  Remove time-cursor caps and use the shared envelope release palette for
  released voices in toolbar, node, Spy and expanded-editor paths. No capture,
  cache rebuild or graph publication from animation.
- NodeDefinition/NodePortLayout/NodeViewModule retain geometry and port identity.
  Lower Trimesh context priority without changing serialized port IDs or DSP
  input order. Reduce mesh/envelope bodies about 20%, improve text and preserve
  arithmetic icons at reduced zoom. Reuse the existing font/icon vocabulary.
- Output gain/meter and default output Spy must share a point before the terminal
  wet-effects chain. Reuse DefaultOutputProbeResolver, compiled buffer addressing,
  existing gain smoothing, meter exchange and waveform capture. Final safety
  clipping remains at device output. Replace the tall output presentation with a
  compact knob and small stereo level indication. Preserve authored effect order.

## Architecture and complexity

Reuse existing DSP and graph-mutation paths; format 10 adds the explicit output
stages through a representation migration. UI commands
remain with GraphCommandDispatcher; layout and cursor movement are presentation
only. No audio-thread allocation, graph copy or waveform cache invalidation from
cursor movement. Pointer work scales with the touched objects; new monitoring
work is O(block frames), independent of unrelated graph content. Keep concrete
rendering and lifecycle responsibilities in their existing owners; extract a
focused owner if a large orchestrator would acquire policy.

The incoming Trombone preset modification is user-owned and excluded from edits
and commits. Factory validation will use other presets.

## Verification

Capture before/after at production size and compact width. Exercise tray toggle,
imports/drops/pinned Spies, browser hover-delete/confirmation cancellation/tag
editing/manager access, held/released cursors and unchanged cache counters,
port/cable alignment and zoomed icon visibility, gain gesture/undo and signal
routing. Assert gain/meter/Spy agreement before wet FX with a known effect chain,
stereo and callback partitions. Build exact app/tests with parallelism 10, run
focused tests and sequential native automation, inspect production diff/style,
architecture audit, code-map and whitespace checks. Commit coherent slices.

## Gain and Global Out

The user requires an explicit Gain node, distinct from a neutral, non-deletable
Global Out endpoint matching Global In. The existing gain/smoothing processor
now belongs to a normal pass-through Gain node.
Global Out owns only the final sink; fixed device headroom/clipping stay realtime.
GraphExecutionPlan outputGain, the renderer's authored-gain smoother and
setter plumbing, and the synthetic apply-at-probe policy are removed. Gain configurations
publish through the ordinary node configuration owner. Meter/Spy sampling is
Gain's output; terminal effects remain audible after it.

Add gain and globalOutput type IDs and format version 10 (current baseline is 9).
Legacy output nodes retain their ID, gain value, explanation and group membership
as Gain nodes. Insert Global Out at the final endpoint, route Gain before the
terminal Reverb/Delay/EQ suffix identified by DefaultOutputProbeResolver, and
preserve effect order and non-wet processing. Extend the resolver's result with
its excluded-effect IDs so migration does not duplicate chain traversal. Move
that suffix right just enough for the inserted Gain, retaining unrelated layout.
Migration runs after existing representation upgrades, is idempotent, and never
rewrites factory files automatically. Cover branched/nonlinear ambiguous inputs
with explicit migration errors rather than guessing, and preserve old data on
load failure. New graphs contain Gain and the required Global Out. Gain is an
ordinary removable, reusable processing node; Global Out has no gain parameters
and cannot be deleted or duplicated. Keep new endpoint rendering and semantics
with existing Global In/Voice Out owners and registration.


## Completed evidence and responsibility review

Gain owns the existing SmoothedParameter and preallocated stereo ramp. Each
published Gain configuration retains a node-local atomic target across ordinary
parameter refreshes; its processor samples that target once per block. This
keeps held voices and wet tails running while gain changes, without a second
master-gain policy in the workspace or renderer. Immutable configurations still
record the authored gain. New topology creates new targets. The executor exposes
the already compiled default probe buffer; realtime peak reductions read it
without allocating. Only the Gain owning that buffer receives its live meters.

Verification completed on 2026-11-11:

- Debug app, test executable and graph migrator build; Release app builds with
  the standalone-release preset. All builds use parallelism 10.
- 70 focused cases / 5,563 assertions pass: output definitions, Gain smoothing,
  live edits and undo with held voices, post-Gain waveform scaling, continuing
  Delay tails, output migration/round trips/collisions/ambiguous input rejection,
  browser actions, cursor palette/cache and graph compiler contracts.
- All 231 factory presets checked (excluding user-owned Trombone) migrate to
  format 10 with distinct Gain and Global Out. Canonical source files are not
  rewritten. Output IDs/rationale/gain and signal order survive migration.
- Sequential native automation verifies disclosure toggling without reserved
  height, hover Trash cancellation, Gain before Delay, neutral Global Out and
  rejected deletion, released red cursors without capture/cache rebuilds, and
  compact 1000x700 layout. Evidence: `.context/polish-native-results.json`,
  `polish-final.png`, `polish-released-cursor.png`, `polish-compact.png`.
- Code-map validation and architecture audit pass; production diff/style and
  whitespace reviewed. Broad legacy tests still have pre-existing empty-model
  and missing-preset debt; the independently reproduced failures/crash are
  recorded in ui-bugs.md. The full suite is not claimed green.

The browser loses concrete card layout/hit/hover ownership: InlinePresetBrowser
falls from 839 to 664 lines; PresetCardList owns 205 cohesive lines. Deletion
confirmation stays in the browser. OutputStageRepresentationMigration owns 132
lines and reuses DefaultOutputProbeResolver rather than duplicating wet-chain
traversal. No generic output-monitor facade remains.

Large touched owners retain their existing delegation: NodeCanvas 3756->3753
routes commands and lifecycle; NodeCanvasPresentation 1532->1545 renders facts;
GraphCompiler 1579->1568 compiles topology and probe addresses; GraphSerializer
1388->1394 invokes representation migrations; GraphAudioExecutor 1305->1321 owns
prepared buffers and bounded processing. Automation inspector 1351->1351 and
NodeCanvasAuthoring 1045->1045 change node eligibility only. NodeWorkspace
941->955 composes/reparents the disclosure and forwards meter measurements;
RealtimeGraphRenderer 827->830 publishes measured peaks and cursor state.
NodeDefinition 1010->1025 remains the declarative registry. NodeCanvas.h
498->497 and NodeGraph.h 489->490 retain their established interfaces/model.
These additions stay with their existing responsibilities; new card and
migration policy live in focused owners. Existing large-file extraction work
remains tracked in refactors.md.


## Gain and preset hierarchy follow-up

Match Gain's fixed height to Delay/Reverb (194 world units); retain its width.
Place a compact knob and its value together, with a paired stereo meter to their
right. Center the occupied group in the body; keep the existing relative drag,
fine adjustment, semantic command, and undo behavior. Remove the redundant Gain
header symbol while retaining the normal global-processing and explanation
controls. NodeDefinition owns natural sizing; OutputMeterPresentation owns the
shared paint/hit geometry.

PresetThumbnailCache desaturates decoded library thumbnails once in its existing
worker, retaining cache/revision handling and authored preview data. The active
canvas and toolbar keep their semantic colors. No per-paint image conversion.

The one-time align_cycle_v2_scratch_envelopes.py migration changes only envelope
position.y when an explicit scratch attachment targets a Trimesh and that Trimesh
is its only consumer. Count consumer identities, not just edges. Preserve shared
and Voice Context default envelopes, x coordinates, graph versions, DSP/models,
metadata and embedded previews. Reuse the existing scalar JSON encoding and
preserve all other source bytes; loading never forces a user's layout. Verify semantic equivalence after applying
just those y edits, rerun for idempotence, and preserve user-owned Trombone edits.


Follow-up completed on 2026-11-11:

- 110 envelopes aligned in 77 factory presets. Every diff changes only the
  intended position.y token; 5 Python cases cover content/text preservation,
  idempotence, shared/default envelopes, other uses, and multiple routes to one
  consumer. The second migration pass reports zero changes. Trombone is intact.
- Debug app/tests and Release app build. 39 focused C++ cases / 2,499 assertions
  pass, including actual cached monochrome pixels, Gain/FX height equality,
  centered control-group geometry, meter ordering, live gain and undo.
- Native production/compact captures confirm hierarchy, aligned scratch ports,
  meter grouping and removed header symbol. Native knob drag and undo pass;
  the full usability fixture still verifies tray, Trash, endpoint protection and
  released cursor/cache behavior. Evidence: gain-refinement-before.png,
  gain-refinement-after.png, gain-refinement-compact.png and
  gain-refinement-native-results.json under .context/.
- NodeCanvasPresentation shrinks 1545->1539 lines by deleting the header branch;
  NodeDefinition stays 1025 declarative lines. OutputMeterPresentation 197->203
  retains paint/hit geometry, and PresetThumbnailCache 43->45 retains async
  decoding/cache policy. No new UI orchestration owner or rendering pass.
  Production style/diff review, architecture audit, code map and whitespace
  checks pass.

## Collapsible preset filters

The preset filter cloud now starts collapsed behind a 24px, keyboard-operable
Filter tags disclosure. InlinePresetBrowser retains disclosure state across
sidebar switches and library refreshes. Collapsing releases the cloud's height
without clearing active tags or Favorites; the heading shows their active count.
Hidden chips expose no automation targets. PresetBrowserLookAndFeel draws the
standard caret and left-aligned label using its existing quiet-button treatment.

Debug app/tests and Release app build. The 23 browser cases pass 375 assertions,
including retained filtering, reclaimed list bounds, hidden targets, active count,
and tab switching. Native disclosure clicks pass the same height/target checks
without graph publication; the existing usability sequence also passes. Reviewed
collapsed/expanded production-size captures under .context/filter-tags-*.png.
InlinePresetBrowser grows 664->679 lines within its existing composition/layout
responsibility; its header stays 154 lines. The LookAndFeel grows 98->113 lines
with button rendering only. Architecture audit, code map and whitespace pass.


## Gain and downstream Spy amplitude regression

Reproduced: editing Gain from unity to unit 0.4 publishes the same output Spy
peak (ratio 1.0 instead of 0.5488117). Automatic per-capture contrast normalization
cancels signal-level changes. Keep monitoring data in signal units and apply
fixed display transfer functions for cyclograms, harmonic spectra and waveforms.
Library JPEG generation may normalize explicitly; reference-file audition keeps
its existing normalization. Retain signal amplitude through compact, expanded,
tray and exported Spy rendering, plus toolbar waveform geometry. Do not introduce
a second gain multiplier. Preserve realtime smoothing; analysis grids use the
authored target rather than one audio block's ramp endpoint. Verify published amplitude, rendered
pixels, gesture commit, undo, upstream isolation and reusable waveform caches.


Gain/Spy regression completed on 2026-11-11:

- The default output tap is now an observed dependency even when explicit Spies
  observe only upstream nodes. Gain edits schedule its traversal and publication.
- Monitoring data keeps its amplitude. DefaultOutputPreview owns level-preserving
  view conversion; compact, expanded, comparison and exported Spies use fixed
  transfers. PresetPreviewGenerator alone normalizes static library images.
  Signal waveform sources retain sample amplitude; reference-file normalization
  stays with the existing StereoBuffer path. The obsolete normalizedTime API and
  live uses of the asset-preview normalization policy are deleted.
- Gain's analysis grid uses its target value, while block audio keeps its smooth
  ramp. No realtime allocations, traversal duplication or synthetic gain stage.
- Regression first failed with peak ratio 1.0 instead of 0.5488117. It now checks
  exact time/spectral levels, explicit/default taps, upstream isolation, pixels
  and undo, both with and without a Spy directly observing Gain. Native drag to
  0.15 changes exported pixels; undo restores the original PNG byte-for-byte.
- Debug app/tests and Release app build. 25 focused cases / 54,096 assertions
  pass. Native evidence: .context/gain-spy-native/{unity,quiet,undo}.png and
  results.json. The scratch-cable fixture's pre-existing empty-mesh failure was
  reproduced independently on b87afb37; no full-suite-green claim is made.
- Architecture/style/whitespace/code-map checks pass. NodePreviewRenderer shrinks
  1039->1028 lines by removing adaptive normalization; its existing rendering
  owner retains transfer/heatmap work. GraphPreviewExecutor shrinks 680->672 by
  removing default-only scaling; capture/extraction remain there. The 429-line
  PresentationPreviewRenderer changes publication only. The observation builder
  adds one compiled tap lookup, bounded by its existing observed-probe work.
