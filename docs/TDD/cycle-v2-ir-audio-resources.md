# Cycle V2 IR Audio Resources

## Status

Implemented (2026-08-27).

Automation follow-up (2026-10-21): `importImpulseResponse` accepts `nodeId`,
`path` and an explicit `mode` (`direct` or `modelled`). The App adapter calls
the existing `ImpulseResponseResourcePreparation::prepare`, then publishes its
complete edit through `GraphCommandDispatcher::setNodeAudioResource`. No second
decoder, trim, modeller, parameter mapping or resource codec is introduced.
Wrong node types, invalid paths/modes and active gestures reject without edits.
The result reports the retained normalized sample count and source rate, chosen
IR length, actual stored processing parameters and committed model/document
revisions. The resource remains embedded and portable in either mode.

The automation service owns request validation and preparation orchestration;
the canvas only delegates and refreshes its existing presentation/editor
collaborators after success. The existing large canvas retains this thin adapter
to its document/dispatcher owners; IR policy stays outside it. Focused tests
exercise real WAV import, audible kernel preparation, save/reload after deleting
the source, one-step undo/redo, and rejection atomicity. The exact app and test
targets build; focused IR tests pass 162 assertions in five cases. The rebuilt
owned reed-study app imports a 48 kHz authored body response in modelled mode,
retains 215 normalized source samples, selects a 256-sample IR and produces a
37-vertex editable model. Source/filter captures and reload verification pass;
the command and results are under `.context/sax-reed-core/ready/`.

Graph persistence currently rounds curve coordinates and resource samples to five decimal
places. This test bounds the audible kernel's reload error at 0.0002 absolute
for its sharp impulse fixture and requires canonical serialized content to
stabilize; undo/redo remains exact. This bounded fixture is not a guarantee for
every long or steep IR. The separately recorded precision limitation in
`audio-bugs.md` also affects the unsmoothed display curve.

## M11: physical time and playback rate

Status: Implementation, compiled-runtime regressions and rebuilt exact-app
integration complete (2026-10-21).
Preserve the mature editor's
44.1 kHz reference timebase: the existing Size sample count and editable curve
describe canonical 44.1 kHz samples. Existing curves require no new fields, rate parameters or
migration. High Pass and preview remain in that same canonical domain.

Imported resources retain their original normalized mono samples and source
rate. Direct preparation resamples those samples to 44.1 kHz before applying
Size and the existing prefilter. Model import fits that same canonical waveform
and chooses Size there; Unload retains the self-contained canonical curve.
The supported maximum Size therefore continues to bound canonical duration.

The IR domain reuses JUCE's MemoryAudioSource / ResamplingAudioSource for
offline waveform conversion, including its established antialias filter. The
IR processor's existing off-thread `prepareExecution` boundary converts the
completed canonical filtered kernel to the actual execution sample rate and
scales it by canonical/output rate to preserve convolution response gain.
Compiled configurations remain canonical and independent of device rate.
The existing processor-cache preparation signature includes execution rate;
the IR processor reuses its playback kernel until either the adopted
configuration or rate changes, then invalidates its prepared convolvers.
Source-to-canonical conversion preserves waveform amplitude so the editable
model remains normalized; it does not introduce rate-dependent geometry gain.
No resource decoding, conversion, modelling, allocation or device access enters
realtime processing or a live editor gesture.

IR conversion supports source and execution rates from 8 kHz through 384 kHz.
The IR preparation owner validates this range and bounds converted sample counts
before JUCE buffer allocation or integer conversion. Generic graph persistence
may retain other positive rate metadata; attempting to prepare such an IR fails
without publishing an invalid configuration.

The mature import trim still caps retained input at 16,384 source samples
before conversion. This limits high-rate import duration independently of
canonical Size (42.7 ms at 384 kHz versus 371 ms at 44.1 kHz); the supported-rate
range does not promise equal maximum retained source duration. This existing
limitation is recorded separately in `audio-bugs.md` and is not changed by M11.

The canonical model has a 22.05 kHz Nyquist limit; retaining a higher-rate source
does not promise preservation of that source's ultrasonic bands in the prepared
kernel. Previously imported 48 kHz experiments should reimport their retained
body source before new tonal comparisons; their old curve coordinates carry no
separate authored-rate field.

Acceptance covers known modal frequency and decay at 44.1/48/96 kHz for direct
and modelled imports, save/reload with source deletion, modelled Unload, unchanged
retained source/rate, and playback-kernel invalidation after execution-rate change.
The former copy-at-output-rate path and its automation policy claim must be
removed. The rebuilt exact app reimported and captured the saxophone body
through the corrected execution path; earlier pre-fix captures are provisional.

The initial factory-only rate tests missed a production integration defect:
GraphCompiler and GraphPresentationModel publish canonical 44.1 kHz
configurations, while the executor passes the actual rate only to processor
preparation. Moving conversion into that preparation boundary removes the
unused factory-rate API and reaches realtime, offline-capture and diagnostic
execution without adding device policy to the compiler or presentation model.

Focused evidence: `CycleV2_tests` builds with `--parallel 10`; actual prepared
processor and compiled-runtime rate tests pass 528 assertions in five cases.
The broader IR/resource/configuration/realtime-safety selection passes 957
assertions in 29 cases. Source and playback grids each cover 44.1/48/96 kHz, direct and
modelled imports, a real 600 Hz damped resonance and 18 ms decay. Acceptance is
within 3 Hz / 2 ms, and convolution magnitude within 2.5% at DC/200/600/3000 Hz
across playback rates. Tests delete source files before reload, preserve source
rate and bounded serialized sample precision, require unchanged modelled audio
after Unload, and reject a persisted `1e300` source rate, invalid execution rates
and an excessive output count before conversion. A compiled graph's real
realtime-renderer path compares dry/wet known-tone transfer for direct and
modelled modes at all three rates, within 4% of its canonical response.
A retained processor also changes rate 44.1 -> 96 -> 48 -> 44.1 kHz without
republishing its configuration. Existing IR properties, visual
analysis, channel policy, post-gain, prefilter, split-block and tail tests pass.
Receipts are under `.context/sax-reed-proud/m11/`.

Responsibility review: Analysis owns the canonical source and shared offline
conversion (196 lines); ResourcePreparation retains decode/trim/fit ownership
(134 lines); the existing effect processor owns playback-kernel conversion
inside execution preparation; the configuration factory retains canonical
publication. The two App adapters only publish the
shared policy metadata. No large production file crosses a size trigger, no
realtime process method changes, and all conversion call sites remain in
import and off-thread execution preparation. Architecture audit, code-map check
and diff whitespace checks pass. Earlier full-instrument 96 kHz differences
were measured while the actual executor still used an unconverted 44.1 kHz
kernel; they did not establish a separate source/FFT limitation. Rebuilt app
evidence in `.context/sax-reed-proud/ready-v3/` and
`.context/sax-reed-proud/analysis/ready-v3-grid-assessment.json` shows the three note probes' 96 versus
48 kHz harmonic-shape differences reduced from about 8.4 dB to 0.042–0.068 dB,
centroid differences below 0.13%, and RMS differences below 0.032 dB. The
44.1 versus 48 kHz probes differ by up to 0.349 dB harmonic shape and 1.20%
centroid. These are bounded observations for this instrument, not a universal
whole-graph rate-invariance guarantee.

## Objective

Make the Impulse Response editor's Load, Model, and Unload actions truthful,
portable, undoable document operations. Imported audio must affect the prepared
IR configuration without giving the editor ownership of graph state, DSP state,
serialization, or undo.

## Authoritative Behavior And Required Extraction

Cycle 1 is authoritative for the user-visible operations:

- `PitchedSample::load` decodes WAV, AIFF, MP3 when enabled, and Ogg audio,
  selects the first channel for the IR, and normalizes the imported audio;
- `IrModeller::trimWave` removes the inaudible tail using its established
  moving-average threshold and clamps the retained length to 64-16,384 samples;
- direct Load selects the next power-of-two IR size and convolves with the
  imported samples;
- Model applies the established 0.7 input gain and `AutoModeller` fit at the IR
  padding and reduction settings, then uses the generated editable curve; and
- Unload removes the imported audio while retaining the editable curve.

The existing implementation is not an adapter boundary. `IrModellerUI` owns
the modeller call and mutates a mesh under UI/audio locks, while `IrModeller`
owns the live sample and pending DSP actions. Extract two UI-independent pure
operations instead of copying them:

1. move the trim-length calculation into `CycleDsp::IrModel` and make Cycle 1
   call it;
2. expose the existing `AutoModeller` buffer-to-intercepts fit so both the
   legacy interactor adapter and Cycle V2 modelling use the same algorithm.

`PitchedSample` remains the mature decode adapter. Cycle V2 copies its bounded,
normalized first channel into the durable resource representation; it does not
retain the legacy object or its mutable playback state.

## Durable Model

`NodeGraph` owns immutable `AudioSampleResource` values:

- stable resource ID;
- display name only, never an authoritative external path;
- source sample rate for inspection;
- one normalized mono channel, bounded to 16,384 samples.

The graph also owns a generic node-to-audio-resource binding with an opaque
domain mode. IR uses `direct` and `modelled`; generic graph and serializer code
do not interpret those strings.

Resources and bindings serialize inside the `.cyclegraph` document. Sample
values use a portable numeric representation for this bounded first slice.
Loading a project never depends on the original file still existing. The
existing JSON snapshot history therefore restores the asset, binding, mode,
parameters, and curve together for undo and redo.

An imported resource is replaced, not mutated. The semantic command removes an
unreferenced prior resource after rebinding. Unload removes the binding and its
now-unreferenced resource. This prevents editor lifetime, file chooser lifetime,
or realtime state from becoming document state.

## Semantic Command Boundary

Add one generic atomic graph edit that receives a complete prepared resource,
binding mode, optional replacement node model, and any domain parameters that
must change with the resource. It validates the complete edit before mutation,
then returns one consolidated `GraphEditResult` with the target node, DSP,
preview, presentation, model, and resource changes.

`GraphCommandDispatcher` owns snapshot undo and publication as usual. A narrow
node-editor resource command translates the prepared value into this dispatcher
operation. It contains no file decoding, trimming, modelling, IR parameter
mapping, or node-kind switch.

The IR domain owns preparation:

- direct import creates a resource, selects the next supported Size stop, and
  binds it in `direct` mode without replacing the curve;
- model import creates the same resource, builds a new `FlatCurveModel`, selects
  the corresponding Size stop, and binds it in `modelled` mode in the same
  command; and
- unload removes only the binding/resource, so the current editable curve is
  immediately authoritative again.

The DSP configuration factory resolves a binding from the graph only for the
node being configured. `IrSignalProcessor` consumes imported samples in direct
mode and otherwise follows the existing curve rasterization path. Both paths
reuse the existing high-pass preparation and post-gain mapping. Resource IDs
and modes participate in the configuration key; no file access or allocation
occurs on the audio thread.

## UI Contract

The property rail adds a separate IR sample section below processing controls:

- **Load** imports direct audio;
- **Model** converts imported audio to the editable curve;
- **Unload** is a lower-emphasis contextual action, enabled only while a
  resource is bound;
- all three actions use compact content-sized buttons on one row; and
- while preparation is running, import actions are disabled and the initiating
  action names the operation. Failures keep the prior document state and use
  the existing editor status surface.

The graph view remains the largest region. The panel does not narrate ordinary
empty, active, or inactive state in a persistent sublabel: binding and mode are
already expressed by the available actions and the rendered editor state.
The High Pass readout uses Hz or kHz at the established 44.1 kHz editor
reference rate rather than exposing a percentage of Nyquist.
Actions use real focusable buttons, preserve host Escape behavior, and expose
stable automation IDs.

File selection is presentation-only. Decoding and modelling run outside the
audio thread; only the completed immutable result is applied on the message
thread. Closing the editor may cancel presentation of the result, but must not
leave a partial graph mutation.

## Negative Boundaries

- Do not persist an external path as the resource authority.
- Do not let the editor mutate `NodeGraph`, record undo, or publish runtime
  configuration directly.
- Do not copy trimming, modelling, rasterization, prefilter, or convolution
  algorithms into UI or command code.
- Do not retain `PitchedSample`, `FileChooser`, mutable buffers, or UI locks in
  graph state.
- Do not add IR parameter IDs or mode interpretation to generic resource,
  serializer, dispatcher, or node-editor command code.
- Do not approximate Model with a simplified curve or bless fake samples in an
  interaction test when the production decoder and modeller can be exercised.

## Expected Change And Deletion Targets

The durable resource/binding and serializer slice should remain roughly
250-450 production lines. Domain preparation, command translation, and UI are
expected to add roughly 300-500 lines. Unexpected node-kind switching, a
generic adapter above 150 lines, or more than about 900 net production lines is
evidence to stop and review.

Delete or supersede:

- Cycle 1's local trim calculation after extraction;
- the IR automation claim that resource actions are unavailable;
- the property-control TDD's resource-action blocker once all criteria below
  pass; and
- any temporary duplicated resource or model state in the editor.

## Verification

Semantic tests must prove:

- decode, normalization, authoritative trimming, supported-size selection, and
  invalid-file failure;
- graph serialization round-trip with no dependency on the source file;
- duplicate/malformed resource and dangling-binding rejection;
- direct import changes the prepared IR impulse and configuration key;
- model import uses the extracted modeller, replaces the curve, and keeps the
  resource in modelled mode;
- each import is one undo entry restoring resource, binding, Size, curve, and
  downstream refresh; redo restores them;
- unload is one undoable command and retains the curve;
- replacing an import removes its unreferenced old asset; and
- no resource decoding, modelling, or allocation occurs in realtime process
  methods.

UI tests and production automation must prove visible action hierarchy,
focusable targets, busy/error/resource states, contextual Unload, real Load and
Model command paths, first-Escape close, and a production-size screenshot with
no clipped status or stranded rail space.

## Completion Criteria

- Load, Model, and Unload are visible, truthful, and operate through one
  semantic command boundary.
- Imported audio is embedded, portable, bounded, immutable, serialized, and
  restored by undo/redo.
- Direct audio reaches the existing IR configuration and Model reaches the
  existing editable curve through extracted authoritative behavior.
- Resource replacement, unload, missing/corrupt data, and editor destruction
  have deterministic safe behavior.
- Generic infrastructure contains no IR IDs, modes, mappings, or modelling
  behavior.
- All deletion targets, focused tests, Standalone Debug build, style checks,
  automation fixture, filtered-log review, and production screenshot pass.

## Implementation Evidence

- The graph owns bounded normalized mono resources and opaque node bindings;
  serializer tests cover portable round trips, malformed data, duplicate IDs,
  and dangling bindings.
- One dispatcher edit atomically replaces the resource, mode, Size, and
  optional curve. Focused tests cover replacement, unload, undo, redo, and
  downstream editor refresh.
- `CycleDsp::irTrimmedSampleCount` and
  `AutoModeller::modelToIntercepts` are the shared authoritative paths. Cycle 1
  now calls the extracted trim operation, and its duplicated calculation is
  gone.
- Direct bindings reach `IrSignalProcessor` and participate in its
  configuration key. Modelled bindings use the editable curve path; neither
  path allocates or decodes in realtime processing methods.
- The property rail exposes compact focusable Load, Model, and contextual
  Unload actions under IR sample, without a persistent explanatory sublabel.
  Busy state stays on the initiating action and errors use the editor status
  surface. Decode and modelling run on the existing worker boundary and publish
  the completed immutable edit on the message thread.
- Focused Cycle V2 tests pass with 86 audio-resource assertions and 62 IR
  property assertions. The Standalone Debug target builds with `--parallel
  10`; the 31-command focused automation includes first-Escape close, and the
  screenshot fixture passes with no filtered-log warnings.
- The complete Cycle V2 run passes 10,416 of 10,417 assertions; its sole
  failure is the pre-existing hit-router hover-help assertion already recorded
  in `ui-bugs.md`.
- The IR sample refinement removes persistent state narration, aligns Enabled
  to the slider control column, and keeps Load, Model, and Unload in one compact
  action row. Focused property tests cover its geometry and error-status route;
  the production screenshot is
  `/private/tmp/cycle-v2-ir-compact-actions.png`.
- The High Pass control displays the shared Hz/kHz frequency format at the
  44.1 kHz editor reference rate, accepts either unit for direct entry, and
  steps by 100 Hz or 10 Hz with Shift. The visually reviewed screenshot is
  `/private/tmp/cycle-v2-ir-hertz-readout.png`.
- Final production volume is 949 added and 73 removed lines (876 net), within
  the review threshold. The new domain preparation implementation is 124
  lines; no generic adapter exceeds 150 lines, and generic graph, serializer,
  dispatcher, and editor-command code contains no IR parameter or mode logic.
- Production screenshot:
  `/private/tmp/cycle-v2-ir-audio-resources.png`. Automation reports/logs:
  `/private/tmp/cycle-v2-ir-audio-resources-report.json`,
  `/private/tmp/cycle-v2-ir-audio-resources-logs.txt`, and
  `/private/tmp/cycle-v2-ir-audio-resource-screenshot-report.json`.
