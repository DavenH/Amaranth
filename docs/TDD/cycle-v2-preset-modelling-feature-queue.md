# Cycle V2 preset-modelling feature queue

Status: Active experiment log. This is an evidence queue, not an implementation
plan. Keep it updated while modelling. Each entry needs the musical task,
observed limitation, evidence, existing owner/design and a concrete acceptance
target. Distinguish confirmed gaps from workflow opportunities; reuse an
existing TDD instead of creating a competing feature or policy. Do not turn a
workaround or an unexplored control into a missing-engine claim.

Current listening direction (2026-10-21): the user finds substantially more
timbral variance in the references than the expressive preset captures and
approved individual reference fits followed by shared bridges. Convincing
Source/Synth comparisons are the immediate acceptance target in the
[anchor study](cycle-v2-saxophone-anchor-bridge.md). Earlier feedback identified
useful CC2 brightness but little other expression; the subsequent controller
and articulation work provides technical response evidence rather than acoustic
fidelity. Retain those broader expressive goals while establishing convincing
anchors. A better pitch score or more controller routes alone does not satisfy
the musical target. Inspect existing control composition before classifying
an unexplored mechanism as an engine feature request.

## M01 — Live envelope morph adoption during a held note

Status: Implemented as an opt-in Voice Context policy in the
[core articulation follow-up](cycle-v2-expressive-saxophone-articulation.md).
The original pilot did not implement this; ordinary presets still latch at
note-on. The evidence below records the original obstacle.

Musical task: vary vibrato depth/shape, pitch contour or scratch articulation
after note-on, while retaining the current note and its traversal state.
The user specifically requested recording this kind of obstacle as a feature
request during modelling.

Evidence at the pilot baseline: `SynthesisCapabilities.cpp` stated that Envelope
morph inputs latch at note-on. In `saxophone-3`, CC1 selects the volume envelope and
CC3 selects pitch/scratch envelope morphs; those routes cannot continuously
adopt a new morph during the same note. The pilot reserves a repeated-note
CC1 trajectory, but its audio is a performance probe, not an isolated proof of
every envelope's live-update behavior. Live Trimesh modulation and persistent
Envelope authoring edits already exist; neither supplies this runtime policy.

Source review after the user's audition also rules out a simple downstream
gain workaround: `GraphEdgeValidator` permits pitch envelopes only into
`VoiceContext.pitch`, and `GraphCompiler` prepares that route from a direct
Envelope source. An Add/Multiply inserted after the envelope is not a supported
live pitch-depth path. Establish the depth/rate mapping explicitly in the
follow-up; live morph adoption and independent control composition are separate
requirements even when one authored envelope can represent both dimensions.

Owner/design: [Dynamic Envelope Modulation](cycle-v2-dynamic-envelope-modulation.md)
and [Realtime Note-On Envelope Preparation](cycle-v2-realtime-note-on-envelope-preparation.md).
Live adoption belongs to Voice Context/shared per-voice traversal policy. The
former Envelope-local `Dynamic while live` option was deliberately removed;
do not restore a competing node-owned flag or publish graph edits at CC rate.

Acceptance for a follow-up TDD: a timed controller trajectory changes the
effective envelope of an active note without restarting it; loop, sustain,
release and stale-generation behavior are explicit. Preparation/adoption work
is bounded and allocation-free on the audio thread, with declared cadence and
continuity evidence at multiple rates/block partitions. Preserve the current
latched behavior where the selected voice policy requests it. Controlling
vibrato rate/depth may need upstream control composition as well; establish
that mapping separately rather than claiming live adoption alone solves it.

Implemented evidence (2026-10-21): Voice Context `envelopeModulation=live`
adopts one changed preparation per envelope/block from starting controls,
preserves normalized loop phase and freezes released geometry. Complete paired
display/cycle preparation and generation checks retain the prior valid shape
on failure; the cycle bank stores value-only old loop spans across delayed
consumption. The saved expressive preset independently maps CC5 depth and CC76
rate. Shared playback tests verify neutral pitch, approximately ±30 cents,
0.2-second delay and 4.5/7.5 Hz endpoints. Eight live envelope voices measured
0.157 ms maximum for seven cubes and 1.451 ms for 104 cubes at 48 kHz/256
frames in Debug, with zero allocations/locks. This is bounded block/cycle
adoption, not partition-independent sample timing or perceptual certification.

## M02 — Reviewable note regions from scales and continuous performances

Status: Region-level analysis is a confirmed workflow gap; automatic boundary
proposals remain an opportunity. Inspect reusable tools before implementation.

Musical task: use both isolated notes and the connected attacks, slurs, bends
and transitions in a solo. The Iowa collection contains 18 chromatic-scale
recordings without per-note markers; the supplied Ivan Renta video is a
continuous performance. Manually identifying every stable note and transition
would dominate a larger reference study.

Existing behavior: `cycle_reference_study.observation` accepts authored crops
and selects existing pitch frames/cyclogram columns without retracking.
Audition selectors already crop immutable source audio and retain the source
origin. This request is for proposed boundaries, review/correction and
region-specific labels/confidence, not a second file decoder or cropping store.

During real-reference calibration, source review found that
`PitchedSample::load` caps decoded input at 2,097,152 frames (about 47.55 seconds
at 44.1 kHz, 43.69 seconds at 48 kHz). Most Iowa scales and the supplied solo
exceed this. Full-file tracking also corrects toward a dominant pitch; exported
periods are rebuilt from the effective edited pitch path. Selecting a crop from
that cached analysis does not establish a separately tracked note region.
Use the isolated VCSL notes for the current calibration rather than silently
truncating or treating a full chromatic scale as one reliable pitched sample.

Reuse lead: `OscAudioProcessor::detectOnsets` already detects level rises, but
it is product-private, runs after input saturation and timestamps block ends.
Automatic reference proposals would need a shared extraction and a reviewed
timing contract, not a copied host detector or sample-accuracy claim.

Acceptance direction: preserve original recording hashes and time offsets;
offer editable region proposals with uncertain/overlapping/slurred boundaries
visible. Keep attack, steady note and complete transition regions distinct.
All derivatives inherit the parent's objective/reserved group. A phrase's
cropped notes are not independent takes, and inferred pitch/control labels
must not silently become observed facts. Retain whole-phrase auditions so
segmentation cannot hide poor transitions.

## M03 — Behaviour-checked economy and shape simplification

Status: Already planned in roadmap slice 10; modelling evidence motivates it.
Implementation remains in that slice after the pilot's evidence gate.

Evidence: the pilot's unchanged shared structure contains 23 nodes, 1,176 mesh
vertices and 147 cubes; its pitch envelope alone has 840 vertices/104 cubes.
These counts do not prove redundancy or justify deleting that contour.
The VCSL calibration subsequently bypasses only that pitch envelope and obtains
straighter tracked NV pitch at objective and reserved notes, while losing
variation present in vibrato references. It preserves all geometry. This
demonstrates why an ablation needs behavior-specific acceptance: a smaller
straight-tone error alone would reward removing useful expressive capability.

The first D3 anchor bridge uses fewer nodes (16), but its corresponding
48-harmonic stereo fits contain 4,624 vertices and 928 cubes. The raw levels,
harmonic evolution and continuous endpoint bridge now provide concrete
behavior for a simplification study. Reduced node count does not establish
economy, and this first regular grid is not an elegance claim. Preserve both
source matches, intermediate control responses and the held sweep while
testing fewer time knots, insignificant partials and shared channel structure.

Owner: [Preset Authoring Roadmap](cycle-v2-agent-preset-authoring.md), constrained
search/simplification slice. Compare ablations and bounded shape reductions
across notes, expressive combinations and trajectories after the evidence gate
is met. Require retained behavior and explain each component's contribution;
an isolated-note fit or a smaller file alone is insufficient.

## M04 — Non-blocking capture of UI-scheduled performances

Status: Source-level workflow limitation found while validating the embedded
articulation phrase; full phrase capture uses the existing offline schedule.

Musical task: capture the actual in-app demonstration while its controller
lookahead, keyboard animation and audio-device callback continue normally.
`CycleV2Automation::captureLiveAudio` currently calls
`StandaloneAudioEngine::captureLiveAudio`, which synchronously waits for
completion on the command/message thread. The keyboard's 60 Hz timer supplies
controller lookahead in `PerformanceKeyboardPanel::updatePlayback`. Blocking
that timer for a capture can exhaust the prepared lookahead, so this command
cannot certify uninterrupted UI-scheduled controller trajectories. A nonzero
captured sustain alone would not establish otherwise.

Owner/direction: reuse the engine's existing live capture buffer and completion
state, with asynchronous command completion or begin/status/fetch commands.
Do not add another recorder, sequencer or audio-thread allocation. Acceptance:
capture a complete saved phrase while timer/controller scheduling advances;
preserve exact-instance provenance and expose dropouts/overruns. Until then,
use exported `PatternPlaybackSchedule` for repeatable phrase WAVs and separate
native Play/state checks for the app workflow.

## M05 — Canonical spectral coordinates and authoring coefficients

Status: Implemented and verified for the anchor/bridge fitter. The exact app
build passed and the focused spectral-authoring test passed 201 assertions.

Musical task: turn measured harmonic amplitudes and phases into corresponding
mesh poles at one pitch, then retain harmonic identity while testing a nearby
pitch and a shared expression bridge.

Evidence: scalar physical-parameter conversion and mesh replacement existed,
but inspection did not expose harmonic display coordinates or additive/phase
operand scaling. The runtime samples `LogRegionMapping(midiNote + 12)` at a
fixed 44.1 kHz mapping, applies a graph-dependent additive magnitude transfer,
and scales phase by harmonic index. Treating mesh amplitude as an FFT magnitude
or interpolating arbitrary frequency positions produces an incorrect fit.

Owner/resolution: `SynthesisCapabilities::spectralAuthoring`, reached through
`inspectNodeControls` with a standard `midiNote` and explicit render
`cycleFrames`, delegates to existing mapping, FFT and spectral-layer owners.
It exports positions, harmonic cap, key position, unipolar additive unit
scale/exponent and bipolar phase scales, including node gain/range. Host fitting
uses these observations instead of duplicating DSP mapping algorithms.

Acceptance: exported MIDI-62 coordinates and coefficients agree with canonical
cores; authored target magnitudes survive the actual transfer; a smaller FFT
caps the returned harmonics while retaining their coordinates; invalid notes
and unsupported frame sizes fail explicitly. No runtime state or policy changes.

## M06 — Semantic cable-panning authoring from automation

Status: A semantic cable-panning command is a workflow opportunity. The initial missing-node
interpretation is withdrawn: panning belongs to cables, and the user explicitly
requires avoiding retired Stereo Split/Join nodes.

Musical task: retain each reference channel's harmonic model using hard-panned
IFFT branches summed through the ordinary time-domain Add operation.

Evidence: iteration 01's `addNode` request for `spectralLayer` fails, but this
type is the internal transport for cable panning, rather than a missing
standalone synthesis node. `NodeCanvasAuthoring::insertPanIntoEdge` creates
that internal node and splices it into an existing cable through graph
commands and one compound undo. Scene presentation collapses its two durable
edges into a single cable with an inline headset. `removePanFromCable` reverses
the operation. Existing generic parameter commands can set an installed pan's
`pan` value; automation does not expose cable insertion/removal semantically.

Owner/direction: expose a cable-panning command through the existing authoring
service, retaining its splice, validation and undo behavior. Capability
discovery should distinguish cable transport and retired serialization types
from ordinary creatable nodes. Do not add `spectralLayer` to generic node
creation or restore Split/Join as the remedy. Acceptance: agents can add,
adjust and remove panning on an addressed compatible cable, with undo/reload
and the existing inline presentation; incompatible cables reject atomically.

## M07 — Construct a complete valid topology through semantic automation

Status: Confirmed incremental-construction deadlock; complete content import
avoids blocking the current fit.

Musical task: build two spectral IFFT branches, hard-pan their output cables,
sum them, apply the shared volume envelope and terminate at Voice Output using
semantic commands.

Evidence: iteration 04 rejects `multiply.out -> voiceOutput.time`. Iteration 05
retries every remaining connection, including explicitly typed magnitude/phase
mesh inputs and Voice Context attachments; none is accepted. Only
`globalInput.time -> output.time` succeeds. Commands, failures and the configured
disconnected nodes are retained under `.context/sax-anchor-fit/iteration-05/`,
including `incomplete-graph.cyclegraph` and `results.json`.

Source diagnosis: `GraphAudioValidationFacts` collapses all unconsumed linked
stereo voice outputs into one `AmbiguousVoiceOutput` issue: `Voice audio path
does not terminate at Voice Output`. `GraphValidator::acceptsProposedIssues`
requires a strictly smaller issue count while any issues remain. Each partial
voice connection leaves that same aggregate issue before and after, so even a
valid connection fails until several edges exist together. The global edge
succeeds because it removes global reachability issues. This is an authoring
transaction limitation rather than a domain or synthesis limitation; changing
connection order does not resolve this graph's remaining aggregate constraint.

Current workaround: export the canonically configured nodes, declare all
topology edges in a new content fixture, open it through the existing graph
loader/validator, then apply fitted models through `applyNodeEdits`. Preserve
the rejected experiments. Current routing declares the existing hidden
`spectralLayer` cable-pan transport on each IFFT-to-Add cable; it uses no
Stereo Split/Join nodes. No production UI or engine graph mutation is added.

Owner/direction: graph command transactions and automation result encoding.
Provide a complete semantic topology edit whose full candidate is validated
before commit, using existing graph validation/undo/publication owners. Keep
rejection atomic and expose structured validation causes through automation
instead of only `Could not connect`. Acceptance: this two-branch topology is
constructible without manual JSON assembly; invalid final graphs still fail
without document changes, and successful construction supports undo/reload.

## M08 — Gain factoring changes release shape below the sustain floor

Status: Confirmed unexpected interaction with the shared legacy sustain-floor
policy; engine changes are outside the anchor-fit slice.

Musical task: factor measured note amplitude into a volume envelope and a
normalized harmonic model without changing the resulting attack/body/tail.

Evidence: iteration 07's layer-2 anchor matches the body but drops roughly
6–8 dB just after the selected 6.75-second note-off, while the source tail
decays gradually. Its authored sustain amplitude is 0.22281961. Layer 3's
0.7656227 sustain has no comparable mismatch at its selected 5.82-second
note-off. Presets, source identities and raw captures remain under
`.context/sax-anchor-fit/iteration-07/`.

Source diagnosis: `EnvelopeSustainPointPolicy` inserts a post-sustain point at
`sustain.x + 0.0001`, raising its amplitude to at least 0.5 for every non-bipolar
envelope. `EnvelopeReleasePolicy` begins unipolar release at `sustainIndex + 1`;
the playback engine normalizes the remaining release against that inserted
point. Layer 2 therefore scales its later authored tail by
`0.22281961 / 0.5 = 0.44563922`, or −7.0203 dB. Layer 3's inserted point retains
its original amplitude, producing unity release scale. This is shared mature
behavior, not a new saxophone-specific rule or a measured acoustic technique.

Current authoring workaround: normalize each anchor envelope by its own peak
RMS and transfer that peak gain into the corresponding harmonic magnitudes.
The endpoint product is retained while both sustain amplitudes exceed 0.5;
recapture endpoints and test the resulting shared bridge. A workaround does
not establish gain-factorization invariance in the engine.

Owner/direction: shared envelope materialization, sustain insertion and release
playback, with a concise discovery description for agents. Preserve intended
legacy behavior before proposing a policy change. Acceptance: equivalently
factored amplitude envelopes reproduce the same physical release across
sustain values below/above 0.5, or discovery explicitly exposes the exception
and its supported representation. Track the audible regression evidence in
[audio-bugs.md](audio-bugs.md); do not silently fit away the source tail.

## M09 — Colored Guide noise with explicit correlation

Status: Workflow opportunity supported by the anchor listening revision;
per-Guide filtering is absent in the current parameter surface. Existing voice
EQ and the shared reed/body IR can supply output-frequency shaping.

Musical task: reproduce phase-local grain and breath, including stronger onset
irregularity, without replacing it with uniform spectral-magnitude hiss.
The user identifies `saxophone-3`'s time-domain noise as a useful precedent and
suggests filtered Guide noise as a possible missing tool.

Evidence: the factory time mesh assigns Guide 4 to a narrow component segment.
`WaveformBakePolicy::bakeGuideCurve` substitutes the Guide between neighboring
phase poles; pole sharpness and component gain control depth. Shared
`GuideCurveTableDsp::sampleDownAddNoise` adds uniform seeded noise after sampling
the prepared Guide table. Guide parameters expose noise, DC/phase offsets and
seed, but no noise-color or correlation control. Changing a Guide's drawn
shape therefore does not itself filter this added noise. An initial isolated
grain/EQ trial established the mechanism. The compact reed prototype now puts
the same Guide on both closing edges and filters the whole excitation through
its shared body IR; the abandoned neutral-baseline branch is not its source.

The refined instrument also adds a quiet two-pole aspiration mesh after the
harmonic reed edge and before the same body. Independent grain/air ablations
change actual PCM. This supplies the current breath bed with existing tools;
larger aspiration settings over-roughen upper notes. A new colored-noise feature
remains optional, rather than a prerequisite for this instrument.

Owner/direction: shared Guide table/noise preparation and immutable resource
snapshots. First distinguish correlation along waveform phase from variation
between cycles and across note age; these have different musical consequences.
If current component gating plus EQ cannot achieve the intended grain,
consider explicit Guide-noise color/correlation parameters. Reuse mature
filter kernels, seed/state owners and preparation boundaries; never allocate
or build filter resources on the audio thread. Acceptance should compare
source/body/onset band balance, cycle variation, seeds, pitch and frame-size
changes, with listening examples. This is not yet a new DSP implementation.

## M10 — Assign a Guide to authored components through automation

Status: Confirmed automation gap; validated content import is the current
workaround, alongside M07's complete-topology import.

Musical task: attach the existing factory grain Guide to four closing-edge
segments of the compact reed mesh. Guide property editing and assignment
inspection exist, but the automation command surface has no addressed
component-assignment command. Assigning the final segment indices before the
new mesh exists correctly fails Guide validation.

Current workaround: import the Guide resource without assignments, replace the
mesh through `applyNodeEdits`, then import that exported graph with only the
declared Guide assignments added. The graph loader and existing assignment
validator remain authoritative; no production graph mutation is introduced.
Future automation should delegate to the existing Guide-assignment command
service with node/component identities, validation and undo. Acceptance:
assign/unassign the same Guide on this mesh without JSON assembly, reject
invalid or stale targets atomically, and preserve save/reload and audio output.

## M11 — Preserve physical body-filter resonances across output sample rates

Status: Resolved in canonical IR import/configuration preparation. Direct and
modelled resonance, decay and response gain are verified at 44.1/48/96 kHz;
the exact app and reimported saxophone body use the corrected path.

Musical task: model a saxophone body with a short impulse response whose modal
frequencies remain fixed in Hz while the reed source changes pitch, and retain
those frequencies when the device sample rate changes.

Original evidence: IR resources retained their source sample rate, but
`prepareImpulseResponseSource` copies direct samples or rasterizes the editable
curve at the selected sample count. Neither path receives the output rate or
converts source-rate coordinates. A 48 kHz kernel replayed at 44.1 kHz therefore
changed physical modal frequency by the 44.1/48 ratio. This was separate from
the intentionally fixed body response across played notes. Canonical import
and automation now report the corrected preparation/playback conversion policy.

Resolution: the existing IR resource/preparation/configuration boundaries use
canonical 44.1 kHz curve time and JUCE's offline resampler. Imported original
samples/rate remain embedded; the completed kernel is converted to execution
rate with response-gain compensation. No conversion enters realtime processing.
See [IR Audio Resources](cycle-v2-ir-audio-resources.md) for policy and evidence.
Acceptance: direct and modelled versions of a known physical resonance retain
its frequency and decay at 44.1/48/96 kHz, survive save/reload, and allocate no
conversion resources on the audio thread.

## M12 — Commit Guide edits before capturing the current audio plan

Status: Resolved publication/lifecycle defect, verified in focused tests and the
rebuilt owned app.

Musical task: ablate factory Guide grain in the shared-body reed model and
compare the changed sound against the same deterministic note. The exported
resource stored noise zero, yet capture retained bit-identical noisy PCM;
reopening and repeating the edit could also suppress configuration preparation.
The source/capture commands and hashes are in the shared-body verification
artifacts; the focused `[guide-reopen]` PCM regression fails before the fix.

Existing owner/resolution: the presentation scheduler resets request identities
alongside prepared-product caches after compilation, retaining gesture state
and cancellation baselines. Committed Guide automation uses the existing
synchronous canvas refresh boundary. Guide assignment, snapshot preparation
and mature noise DSP are reused. No new grain renderer or render-time graph
mutation is needed. Details and acceptance evidence are in the
[presentation lifecycle TDD](cycle-v2-presentation-cache-lifecycle.md) and
[audio ledger](audio-bugs.md).

Acceptance: the same edit after reopening changes actual PCM, agrees with a
freshly compiled current graph, and performs no extra topology compile for the
Guide edit. Immediate automation capture must observe the committed value;
active graph gestures must still commit once, undo once and cancel correctly.
The focused reset/PCM/gesture checks pass 67 assertions in three cases and
adjacent checks pass 502 assertions in 38 cases. The owned app's immediate and
reopened zero-noise captures are bit-identical and differ from the noisy
baseline; `.context/sax-reed-core/ready/verification.json` passes without the
temporary export/reopen workaround.

## M13 — Discover narrow-source sampling and frame-size policy

Status: Confirmed authoring constraint; peak alignment is the current content
workaround. This is not a newly introduced oversampling regression.

Musical task: author two narrow impulses with broad harmonic support while
retaining level and shape across notes. In the polyphonic spectral region,
the source frame follows pitch (roughly 1024/512/256 at the tested notes),
rather than always using the displayed `cycleFrames=2048`. That minimum is
currently passed only for monophonic-legato contexts. `TimeFrameSourceRenderer`
uses point sampling with integral preparation disabled; a crest at phase .001
falls between progressively coarser samples. The actual three-note probe
retains a nearly flat harmonic series but loses fundamental level sharply.

Aligning crests at 0/.5 removes the accidental peak miss for this preset.
It does not establish area invariance for every sub-grid pulse. Existing mature
integral/area sampling is available, while V2 point-only preparation was an
explicit performance choice. Oversampling limitations are already documented
in the audio ledger and oscillator-region performance design. A quality-policy
change should reuse that sampler and compare harmonic/level stability, aliasing
and runtime work; do not introduce another oscillator algorithm.

Discovery should report effective source frame length, sampling policy and
whether a requested frame minimum/oversampling value affects the chosen path.
Evidence: `.context/sax-reed-refinement/narrow-canvas-old-body/exciter-probe/`
and the aligned candidate captures. The final D3 clean source has H40/H1 at
−0.82 dB; the doubled source's settled missing bins remain suppressed.

## M14 — Shared register-dependent reed-edge/body relationships

Status: Implemented with existing spectral/Guide controls. Remaining acoustic
balance is a preset-design issue, rather than a confirmed missing engine feature.

Musical task: keep the full-harmonic impulse source, then explain the reference
family's register changes with a few shared controls. A stronger fixed-Hz upper
mode restores the high register but over-brightens lower notes; broad fixed-Hz
damping does the reverse. The D3-focused lower resonance improves H2/H1 while
remaining too dark on the high probe. No single-note metric is acceptance of
the complete instrument.

An offline conditional fit suggests one smooth harmonic-relative reed-edge
cutoff near 6.5 harmonics could reduce low-note upper bands while preserving
high-note lower harmonics. Every gain stays positive; this is not an odd/even
comb. Expose it as an interpretable shape alongside the fixed-Hz body. Existing
key modulation and Guide rails should be tried before proposing runtime work.
Validate the actual spectral mapping across notes; a Guide calibrated in Hz at
one inspected note must not be described as a fixed-Hz filter at every note.
Evidence: `.context/sax-reed-refinement/shared-edge-hypothesis.json` and the
actual aligned-flat/modal/focused assessments. The final refinement instead
uses a rounded H3→H4.5 pressure-dependent edge, six harmonic landmarks and six
canonical key Guides, shared across every note. Combined with five broad body
controls, actual weighted shape error improves 4.19→3.22 dB across six probes.
Quiet D3 is still bright. A further broad key bell was rejected for marginal
overall gain and a loss of loud D3 character; no per-note spectra were added.

## M15 — Send live controllers through the interactive MIDI command

Status: Confirmed automation gap. Offline capture and embedded patterns accept
controllers, and the native keyboard exposes the mod wheel; `sendMidi` accepts
only noteOn, noteOff and allNotesOff.

Musical task: hold a native note while an agent moves breath, vibrato depth/rate
or reed instability without restarting a capture or editing a pattern. The
current command explicitly rejects a controller event. A persistent socket or
MCP wrapper alone does not add this missing semantic operation.

Owner/direction: extend the existing interactive MIDI adapter and event queue,
reusing controller validation and voice delivery. Acceptance: CC1/2/3/5/76
changes reach an already-held native voice, independent unused CCs leave its
sound unchanged, and note-off/all-notes-off retain their current behavior.
Do not implement a second MIDI transport or audio renderer.

## M16 — Reproduce modelled IR authoring from the same source

Status: Measured small import/modelling variation; saved-preset seeded playback
is repeatable. The cause of repeated-fit variation has not been isolated.

Musical task: compare a deliberate body change without an unrelated fitted-curve
change. Ready-v3/v4 import the same body WAV SHA and IR parameters, but their
28-point modelled curves differ (maximum y delta .00138). Register captures
change .038–.050 dB in RMS, with .27–.62% residual after one best constant gain.
The source-only probe is exact while noise-free filtered PCM differs. The
separate live explicit/implicit context edit produces exact PCM, ruling it out
as the cause. Evidence: `.context/sax-reed-proud/analysis/ready-v3-v4-regeneration-comparison.json`.

Owner/direction: shared AutoModeller/import preparation. Establish which fitter
inputs or random state vary; offer a deterministic authoring recipe or retain
the canonical fitted result when replaying an experiment. Do not change mature
fitting behavior or add a parallel modeller merely to stabilize this preset.

## Related defects, tracked separately

- Flat-curve coordinate rounding changes a sharp modelled IR slightly after
  save/reload; the bounded test and numerical evidence are in [audio-bugs.md](audio-bugs.md).
- Reference exports label Cycle-internal note values as MIDI; see
  [audio-bugs.md](audio-bugs.md). Do not compensate with a hidden pitch guess.
- Tiny controller-response differences precede scheduled events in the current
  rendered output; retain full boundary diagnostics in the same audio ledger.
- Oversized collapsed-group cable detours are recorded in
  [ui-bugs.md](ui-bugs.md); this is a UI defect, not a synthesis capability.
