# Cycle V2 expressive saxophone articulation

Status: Complete (core playable/technical contract). The user requests the complete core expressive set and
an embedded MIDI/controller demonstration. This continues the calibration
study, preserving its frozen evidence and the user's currently open candidate.

## Musical contract

Deliver one playable shared saxophone preset with sustained, tongued and short
notes, genuine connected transitions, live vibrato depth/rate, breath/dynamics
and a controllable growl/air texture. The user explicitly selected the core set
before extended techniques. Preserve the useful CC1 short-articulation and
CC2 brightness response heard in the first audition. An articulation control
changes attack/release character; note-off still determines a short note.

Use independent controls, then demonstrate their combinations across low,
middle and high notes. Provide an embedded `PresetPresentation.sequence` with
notes and CC lanes that demonstrates fluidity without setup. Keep musical
descriptions honest: mathematical control response alone does not establish
acoustic authenticity or human taste. Newly discovered gaps go in the existing
modelling feature queue with concrete evidence.

Initial control map: CC1 attack/release articulation, CC2 brightness, CC3
timbral onset, CC4 air/growl texture, CC5 vibrato depth, CC76 vibrato rate and
CC11 expression/breath pressure, plus CC77 periodic growl roughness. Validate each route rather than assigning a
familiar name to an untested signal. Keep pressure usable at a sensible default.

## Authoritative owners and translation

- Envelope geometry/materialization: shared `EnvelopeMaterialization` and
  `RealtimeEnvelopeMaterializer`. Reuse fixed-capacity inactive-buffer prepare
  and complete-result swap; no second rasterizer or worker preparation service.
- Envelope cursor/loop/release: shared `EnvelopePlaybackEngine`. Any normalized
  loop-phase reconciliation belongs here, using old/new views, not a V2 adapter.
- V2 adoption: `EnvelopeSignalProcessor`, `CycleEnvelopePlaybackSource` and
  `PreparedCycleEnvelopeBank` translate Voice Context policy to existing shared
  products. Preparation generations distinguish changed geometry when the
  configuration pointer remains identical.
- Voice policy and timing: `RealtimeGraphRenderer` owns voice allocation,
  event scheduling, note-source isolation and release. `OscillatorLaneCore`
  and `UnisonCore` retain phase traversal and fractional-pitch conversion.
- Graph state: registered Voice Context definitions, `GraphCompiler` and the
  existing editor/dispatcher own parameters, compilation and persistence.
- Content: reuse the current saxophone waveform/spectral structure and guide
  noise. Shape replacement and routing use existing semantic graph commands.
  Authoring model data may describe points/cubes; it must not implement a new
  interpolation, fitting, rendering or audio-analysis algorithm.
- Demonstration: `PresetMidiSequence`, `PatternPlaybackSchedule` and
  `PatternControlScheduler` already supply notes and interpolated CC lanes.
  Suites and raw/RMS audition reports retain capture/provenance ownership.

Cycle 1's note/envelope behavior remains the parity reference for existing
paths. It has no monophonic legato/portamento implementation to extract; the
voice-policy slice is an explicitly new contract, not claimed parity.

## A. Live envelope adoption

Add Voice Context `envelopeModulation` choice (`latched` default, `live` opt-in).
Resolve it per applicable execution context, including pitch and scratch
consumers. Never restore an Envelope-owned Dynamic flag or publish graph edits
at MIDI rate.

Authorize active-note use of the newer fixed-capacity shared materializer:
one changed-morph preparation per enabled envelope per audio block, from that
block's starting control values. Events within a block become eligible at the
following boundary, never before their timestamp. Note-on preparation retains
its exact event offset. This explicitly supersedes the obsolete dynamic TDD's
blanket prohibition on active-note realtime preparation for this bounded core;
general rasterizers, allocation, locks and worker queues remain forbidden.

Publish a preparation generation only after successful complete preparation.
Pitch/scratch banks validate on generation changes even when configuration
identity is stable. Keep state independent per voice/lane, preserve active
note/cursor, and freeze the last adopted geometry after note-off. A failed
preparation retains the last valid shape. Unchanged controls perform no work.
Live rate changes preserve normalized loop phase through the shared playback
owner; they do not restart the vibrato or escape release into sustain.

Response latency is bounded by one audio block plus existing control sampling;
there is no partition-independent sample-cadence claim. Tests span multiple
rates/block sizes and nonzero note-on offsets. Measure repeated preparation at
eight voices using both compact authored envelopes and representative complex
ones; capacity bounds alone are insufficient performance evidence.

## B. Connected notes and portamento

Add Voice Context `noteMode` (`polyphonic` default, `legato` opt-in). Existing
`portamento` enables a real pitch glide for overlapping legato notes; add
`portamentoSeconds` with a bounded seconds value (default 0.06, range 0–0.5).
Non-overlapping notes articulate afresh. Defaults preserve existing presets.

Renderer synth voices represent the whole graph. The existing
`GraphTopologyValidator` already rejects multiple active Voice Contexts; retain
that authoritative boundary and select the active context's policy, ignoring
disconnected contexts. Do not add a competing partial multi-context policy or
silently collapse unrelated polyphonic regions.
Keep separate held-note stacks per MIDI source/channel, bounded and preallocated,
with last-note priority. Overlap retargets one active voice without note-on or
oscillator/envelope reset; releasing an older note does not release the current
one; releasing the current note returns to the latest held predecessor. Final
release, source cancellation, all-notes/all-sound-off and voice stealing clear
the appropriate state. Repeated same-pitch events have a defined matched count.

Retarget at the scheduled event offset, not by changing the whole block's note
number early. Reuse existing execution segments or explicit timed pitch data;
preserve oscillator phase and envelope traversal. Glide is linear in semitones
with bounded preallocated state. Fractional pitch uses existing shared
conversion. Rendering and diagnostics must expose no phantom active voices.
Fixed-time spectral playback reads pitch per sample; the existing cycle-based
spectral/chained renderers consume it at a generated-cycle boundary and may
already have a cycle buffered. Preserve their cadence and prohibit early
retargeting; do not promise sample-exact audible frequency adoption for them.

## C. Preset and embedded phrase

Create a new explained preset rather than rewriting user factory edits or the
frozen calibration candidate. Replace the large pitch contour with a compact,
neutral-at-zero-depth looping envelope: separate depth/rate morph axes and a
short delayed onset. Preserve CC1 articulation/CC2 brightness; use a free
volume morph dimension for expression and existing guide-noise geometry for a
genuine independent roughness/air axis. Verify audible results before calling
that endpoint convincing growl. Keep the graph compact and interconnected.

Embed a 20–30 second phrase with isolated demonstrations followed by musical
combinations: sustained swells, tongued/staccato notes, vibrato development,
connected runs and expressive held notes across registers. Initialise all used
CC lanes at time zero and return them to sensible defaults at the end. Reuse the
shared scheduling owner for both in-app preview and exported demonstration.

## D. Verification, scope and completion

Domain tests cover default latching/polyphony, opted-in repeated updates,
configuration-stable generation changes, loop-phase continuity, release/reset,
independent voices/lanes, timed retargeting, held-note return, same-note repeats,
source isolation, existing multiple-context rejection and real zero-time/finite-time glide. Runtime
tests assert allocation/lock freedom and no persistent graph mutation.

Render a bounded suite with exact repeats: straight sustain, short/tongued,
brightness, pressure, independent vibrato depth/rate, texture, combinations,
connected ascending/descending notes, held-note return and separated attacks.
Inspect event boundary prefixes, release tails and raw/RMS auditions. Use shared
playback tests for vibrato rate; smoothed effective reference pitch frames alone
cannot certify it. Capture the embedded phrase through the same schedule used
by in-app playback. Preserve source/candidate/engine/performance identities.

VCSL's twelve recordings are now developmental evidence, not a fresh holdout.
Do not decode Iowa's reserved parents or repartition consumed D3 examples to
claim independent validation. Untested acoustic techniques remain labelled as
such; the core playable contract still requires every listed control and
transition to work. Keep user-facing notes and a clear control map in the
preset. Native app playback and final user audition are part of the handoff.

Expected boundaries: live adoption/generation/phase changes ~200–400 production
lines; new note policy and timed glide ~200–400; parameter/editor/capability
wiring ~100; bounded host authoring/demonstration orchestration ~200–350 plus
content. Review unexpected growth; extract cohesive existing owners rather than
expanding large runtime/compiler files with independent policy. No new node kind,
sample database, fitter, detector, sequencer or compatibility façade.

Deletion targets: obsolete pointer-only adoption test, obsolete blanket live
preparation prohibition for the bounded core, unused portamento runtime promise,
and any temporary prototypes after shared owners are integrated. This TDD stays
In Progress until the runtime, preset, embedded phrase, evidence, architecture
review and negative boundaries are complete. Commit coherent slices and continue
through the core contract; do not stop after another calibration-only result.

## Implementation review and evidence

The existing Portamento toggle was tested before changing runtime behavior.
Repeated overlapping 60/67-note captures were byte-identical with it off/on
(`.context/existing-portamento-probe-2/summary.json`). Compilation retained the
flag but neither oscillator nor voice policy consumed it. The new path consumes
that control only in the explicitly selected Legato mode.

Live preparation stages both the display envelope and optional cycle guide
before adopting either. Failed preparation keeps the complete previous result.
The cycle bank stores the previous loop's value-only span: retaining a view into
the materializer's two buffers would become unsafe if several host blocks
updated geometry before a low-frequency oscillator consumed its next cycle.
A regression performs four preparations before consumption and checks phase.

High-to-low slurs retain a spectral frame floor from the existing materializer's
cycle size. The first implementation exposed the streaming Hermite resampler's
small-padding assumption at large frame/period ratios: `Resampling.cpp:281`
asserted, with 20 counted allocations including assertion reporting
(`.context/articulation-domain-second.log`). Opted-in legato uses the existing
shared periodic Hermite sampler on composed frames instead. Polyphonic presets
retain their original path. A 2048-to-32 sample regression covers the ratio and
fractional cycle boundary; both spectral clocks cover descending slurs.

Size/responsibility review: `GraphCompiler.cpp` remains the existing compilation
coordinator (1535 to 1548 lines); its added policy resolver reuses authoritative
voice-context assignment. `GraphAudioExecutor.cpp` grows only one line to pass
that compiled fact. No second admission or ownership policy was added.
`NodeDefinition.cpp` remains the registration owner (new parameters only), and
`NodeWorkspace.cpp` delegates sequence edits to the existing canvas/dispatcher
before refreshing its keyboard. `CycleV2Automation.cpp` adds dispatch only;
validation stays in `PresetPresentationCodec`, scheduling in the existing
pattern owners. `RealtimeGraphRenderer.cpp` stays below 800 lines; bounded
held-note/glide state lives separately in `VoiceNoteState`. The content module
describes geometry; its 251-line host driver only orchestrates semantic edits,
canonical conversion, scheduling, capture and existing audition reports.

Native checks on the isolated app clicked Legato, Portamento and Live, dragged
Glide time from .06 to .139 seconds, adjusted it to .149 with the arrow key and
undid both edits. `.context/native-voice-context-results.json` records the
observed values; `.context/voice-context-after-native.png` records the 520 by
488 editor. The user's earlier app instance and edited graph were preserved.

An independent ownership/lifecycle review confirmed fixed storage, source
isolation and shared sampling/phase ownership. It found that a repeated
same-pitch note during an active glide could restart the glide duration; the
regression must cover Portamento enabled, not only the immediate-pitch case.

Verification on the final production engine:

- Exact `CycleV2`, `CycleV2_tests` and `AmaranthLib_tests` targets built in
  `standalone-debug`. The combined focused domain selection passes 87 cases /
  28,269 assertions, with no JUCE assertions or leaks. Shared adoption/periodic
  regressions pass four cases / 58 assertions. The shipped preset's two pitch
  tests also pass after the final content copy. The unrelated legacy offline
  Voice Context fixture remains absent; it is already in the audio ledger and
  was excluded from the focused selection.
- The isolated eight-envelope benchmark at 48 kHz/256 frames has zero
  allocations/locks and maxima of 0.157 ms for seven cubes and 1.451 ms for
  104 cubes, against a 5.333 ms block. The eight-voice full-audio slur benchmark
  at 48 kHz/512 frames also has zero allocations/locks. Compact mean/max is
  5.755/6.214 ms at interval 64 and 2.429/2.685 ms at 256; factory mean/max is
  7.777/8.303 ms at 64 and 3.110/3.580 ms at 256, against 10.667 ms.
  Logs: `.context/live-envelope-benchmark.log` and
  `.context/legato-performance-final.log`. These are Debug observations, not
  worst-case latency guarantees for arbitrary graphs or machines.
- The final host run `.context/sax-articulation-refined/` passes 30 captures /
  168 seconds / 15 exact PCM repeat pairs. It includes each advertised control,
  ascending and 84-to-48 descending connected notes, held-note return,
  combinations and the embedded 28-second phrase. Canonical schedule export
  contains 1,718 events, within the existing 2,048 queue capacity. The preset
  embeds 21 notes and 77 control points. All captured audio is nonzero/finite;
  maximum peak is 0.110954.
- Independent artifact review verifies source/script/engine/preset/effective
  graph identities, WAV headers, PCM hashes, schedule count and new card
  non-overlap (`.context/sax-articulation-refined-audit.json`). Candidate SHA256:
  `ed6f50624730d60833ee4635320091dfd86da1f475037b6a604dd7c069a07bd6`;
  engine SHA256:
  `4e68f21ab64dd43b9b28e23247d2e9b6c468c47d191a9fa235449bca10a9b417`.
  Raw/RMS auditions and full phrase WAV remain next to the frozen evidence.
- The first CC4 endpoint was weak: its body difference was only 0.352% of
  baseline RMS. A bounded component-only refinement raises its red endpoint
  weight from .20 to 1.0 and time-guide gain from .6745 to .8. Difference rises
  to 4.20%; all 12 zero-air cases remain PCM-identical across both repetitions.
  Shared guide resources and assignments are unchanged. This is evidence of
  a stronger independent axis, not proof of audible realism or taste.
- Semantic authoring verifies atomic shape undo/redo, declared node moves,
  save/reload and malformed sequence rejection without history/content changes.
  Twenty-two Python tests cover content and host orchestration. Canonical
  sequence decoding now rejects coerced/missing/fractional MIDI fields; factory
  and library sequences retain round-trip coverage.

The result remains one shared 26-node, 57-cube graph (448 mesh vertices), using
five existing guides. Pitch geometry falls from 104 cubes to seven; growl adds
one seven-cube envelope and one multiplier. No new node type, fitter, sample
bank, sequencer or detector was introduced. The temporary allocation trace was
deleted. The prior pointer-only adoption test and obsolete worker-only live
policy were replaced. Portamento is now consumed under the explicit policy;
duplicate same-pitch notes preserve an in-progress glide. Source release sends
the existing reset message across all 16 MIDI channels, verified through the
real engine callback without opening audio devices.

Acoustic growl remains a labelled periodic-roughness hypothesis. Taste,
source-control inference, extended techniques and an acoustically complete
saxophone model are outside this core contract. The existing twelve VCSL takes
remain developmental; no reserved Iowa source was decoded. A further live
capture workflow limitation is recorded as feature-queue M04: synchronous
message-thread capture can starve the UI's controller lookahead timer.

Final native handoff passes on the actual audio device at 44.1 kHz/512 frames.
The saved preset reports 21 notes and 77 CC points, with no external pattern
reference. A native click starts Play; snapshots at 0.5, 6.5, 12.5 and 20.5
seconds show progression/audio callbacks and sounding voices. After the whole
28-second phrase, playback stops, active voices return to zero and dropped MIDI
events remain zero (`.context/native-sax-phrase-results.json`). The source app
was preserved; the isolated app remains open on the new preset with the Voice
Context editor showing Legato, Portamento and Live. Independent runtime and
completion reviews found no remaining material core-contract gap. Human
audition remains the acoustic/taste handoff, not a claimed qualification.
