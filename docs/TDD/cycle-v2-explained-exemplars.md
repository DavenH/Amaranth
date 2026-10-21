# Cycle V2 explained exemplars

Status: Implemented

## Product contract

Explain four existing factory graphs in place: `sat-bass` (time/transfer shape),
`filter-saw` (spectral mask and articulation), `reed` (guide variation and
expression routing), and `warmth` (spectral source, unison and spatial effects).
Factory edits add only `Node::rationale` and `PresetPresentation::description`;
no topology, model, parameter, guide, resource or demonstration changes. Humans
use existing preset/node explanations; agents use the same saved text and
`inspectExplanation`. There is no second note format or duplicate graph bank.

`cycle-v2/content/exemplars/index.json` references these existing graph/node/
control IDs, their synthesis-only source identities, tested/starting ranges,
limitations and one exact controlled intervention per example. Starting musical
ranges are authored hypotheses; only values actually exercised are marked
technically tested. Effective MIDI control claims require observed response.
Structural routing facts and numerical differences never claim taste, acoustic
fidelity or equal perceived loudness.

## Authoritative owners and unchanged behavior

- Existing `Node::rationale`, `PresetPresentation::description`, graph serializer,
  `setExplanation`/`inspectExplanation` own durable notes and human/agent access.
- `NodeDefinitionRegistry`/`inspectSynthesisCapabilities` own legal parameters,
  timing, scope, physical mapping and authoring codec availability.
- `inspectNodeControls`, `applyNodeEdits` and existing `setGuideParameter` own
  canonical domain payload inspection, validation, semantic commits and undo.
  The host translates four fixed content recipes; no alternate model decoder,
  graph mutator, transaction system, optimizer or search framework is added.
- `EnvelopeMarkerPolicy`, `EnvelopeReleasePolicy`, and `EnvelopePlaybackEngine`
  in `lib/src/Curve/Rasterization/` own sustain/release behavior. The filter-saw
  volume envelope marks cube index 1 as sustain; all its vertices have phase
  exactly 1 across morph dimensions. Its later cubes have phase above 1.
  The unipolar release policy starts at the intercept after sustain. Affinely
  stretching these later coordinates preserves markers/topology and changes
  post-note-off duration. The host must verify those concrete source facts,
  retaining mature rasterization/playback and proving the effect with audio.
- `render_suite` owns deterministic capture, scheduling, engine/preset/event
  identities and repeatability. `generate_report` owns raw/RMS A/B artifacts,
  interval spectra, plots, alignment policy and provenance. Reuse unchanged.
- `cycle_automation.py exec` and the canonical session client own exact-instance
  lifecycle and leases. One GUI session at a time; never reuse/stop a user app.

## Concrete interventions and evidence

1. Sat-bass: inspect waveshaper flatCurve; change vertex ID 5 Y from its factory
   value to 0.60, retaining X, curvature, other points, and pre/post gains.
2. Filter-saw: inspect Envelope state and its marked sustain cube; assert the
   uniform phase-1 boundary, then map only later vertices to
   `1 + 2 * (phase - 1)`. Preserve all other fields and marker memberships.
3. Reed: change existing `guide5.noise` to 0.15; preserve its curve/seed,
   assignments, other guides and node models. Guide variation is not automatically
   equivalent to acoustic noise or to spectral remaining-band energy.
4. Warmth: disable delay; preserve reverb, unison, source, envelope and other
   delay settings. Describe A/B as the delay contribution, not every tail's cause.

Each intervention is made on an app-opened copy, saved only to a fresh output
folder. Save the unedited annotated baseline, inspect required capabilities and
controls, apply one semantic factor, prove all unrelated serialized synthesis
state identical, undo to the exact baseline, redo to the exact variant, save/
reload, then run fixed-seed baseline and variant suites with two repetitions.
Record exact payloads and engine/source/performance/PCM hashes. Verify the
CC comparison prefix 0–180 ms exactly, retaining a declared 20 ms boundary guard
before the first 200 ms controller change and retaining complete raw audio.
Retain full 0–200 ms worst-error/count/first-difference diagnostics. A negative
control changes only moving CC1 targets to unused, non-channel-mode CC118 while
retaining initial CC1=0, note, velocity, seed and event times. Render two repeats
per example (eight extra captures; 72 total). Compare response against this
same-time control and report held-vs-control drift separately; declare response
only when a descriptor exceeds its measured drift plus 1e-10. Reused immutable
captures must match negative-control engine and effective-graph identities. Reject a
non-effective intervention rather than publishing a demonstrated-effect claim.
Use explicit held-note/release/CC intervals without automatic alignment. Notes
qualify effects not established by the measurements as authored hypotheses.

## Files, size, dependencies and deletion

- Four existing factory `.cyclegraph` files: approximately 20–25 short rationales
  plus four preset design notes; preserve existing JSON layout and synthesis.
- `cycle-v2/content/exemplars/index.json` and `performance-suite.json`: four
  manifest entries and one bounded suite covering register, velocity and timed
  CC, with controls/limits and reproducible comparison intervals.
- `scripts/render_cycle_v2_exemplars.py`: approximately 320 lines including measured-boundary negative-control QA, fixed
  recipe translation and orchestration over the existing domain APIs/runners.
- Focused host content/recipe tests, `docs/cycle-explained-exemplars.md`, README
  and existing code-map feature route. No C++ or DSP dependency is introduced.

The source identity excludes only rationale and preset description; it protects
all remaining graph data, including previews, guide assignments and patterns.
Content checks resolve every referenced node/guide/control, verify note access
and fixed recipe preconditions, and prove recipes leave source payloads and
unrelated fields unchanged. App code remains the authority for actual semantic
edit validity and model interpretation.

Work is host-side and bounded to four presets, one candidate each and the shared
small suite. Existing suite/report limits apply; allocate no audio-thread state.
No interaction/publication complexity changes. Stable end state is data and a
thin fixed-content host recipe; there is no temporary compatibility layer or
mature-code deletion target. Stop for architectural review if reuse requires
copying rasterization, envelope playback, guide generation or DSP behavior.

## Acceptance

- Human and agent access exposes the same design/component notes for four
  unfamiliar examples; source synthesis-only hashes remain unchanged.
- Control ownership, timing and legal ranges agree with canonical capabilities;
  tested coverage is distinct from musical starting hypotheses and limitations.
- One-factor changes, exact undo/redo/reload, unchanged unrelated state and
  nonzero repeatable A/B evidence pass for all four examples.
- Reports retain raw levels and actual attack/body/release/trajectory windows,
  make no taste/fidelity claim and safely retain unpitched descriptors as needed.
- Focused tests, canonical load/validation, exact app/coordinated demonstration,
  code-map and whitespace checks pass. Record new incidents in existing ledgers.
- Before marking Implemented, record production sizes, mature owners reused,
  branch/adapter review, compatibility/deletion state and concrete evidence.


## Completion evidence

- Production: one 323-line fixed-content host runner, two small data manifests,
  23 node rationales and four preset descriptions (27 added factory JSON lines).
  No C++/DSP/model/topology/parameter/resource/guide/pattern change. Source and
  decoded synthesis identities remain unchanged for all four annotated graphs.
- The runner reuses canonical capabilities, explanation inspection, node/guide
  commands, undo/save/reload, suite rendering and audition-report owners.
  Four recipe branches translate reviewed content edits; no playback,
  rasterization, FFT, scheduling or transaction algorithm was copied. No
  compatibility layer or deletion target remains. Ordinary edit/publication
  and realtime complexity are unchanged.
- 66 focused Python checks pass, including nine exemplar content/recipe/control
  cases and existing candidate, performance, reference, audio-diagnostic and
  audition-report contracts. Canonical GraphMigrator loading validates all four
  annotated sources and preserves decoded synthesis/notes.
- Real app proof uses an independent snapshot of the exact merged engine built
  at `984d1ff9` (C++ unchanged at data-fix baseline `609e1bea`), executable SHA
  `1fa1fde130e2b86347dc4ee88520e7586e5036b9a400c10fdd7ba08dafee5122`.
  Own-profile coordinator runs opened explicit source paths. No user app or
  preset was touched. All 27 explanations matched inspection exactly; all four
  interventions passed unrelated-state identity, exact undo/redo/save/reload.
- Audio evidence contains 72 source captures / 36 exact repeated pairs, all
  nonzero and finite. Original 64 baseline/variant captures remain immutable;
  eight later CC118 controls match their baseline executable/effective-graph
  identities. An independent reviewer rehashed all 72 WAV/PCM artifacts.
- Every one-factor edit produces measured differences. Filter-saw held-note PCM
  remains exact through 600 ms; its edited release increases following-window
  channel RMS from 0.0125507 to 0.0188471 with the same delay chain. This does not
  claim the complete effect tail duration doubles.
- All CC118 motion descriptor drift is zero. Actual CC1 motion mean-absolute
  centroid differences are 197.838 Hz (sat-bass), 1170.670 Hz (filter-saw),
  84.235 Hz (reed), and 182.495 Hz (warmth), exceeding their measured control
  drift. Full-prefix differences before 200 ms remain documented for reed and
  warmth; all 180 ms proof windows are exact. The known boundary issue remains
  open in `audio-bugs.md`; no sample-exact onset or DSP correction is claimed.
- Four standalone reports contain six cases each, 96 raw/RMS A/B WAVs and 48
  PNG/SVG plots. All 144 generated artifacts were rehashed; relative player
  links, immutable original audio, one-factor state and history proof pass.
  The filter release plot was visually inspected.
- Evidence in this worktree: `.context/exemplars-verified/index.html`,
  `.context/exemplar-artifact-audit.json`, `.context/exemplar-focused-tests.log`,
  `.context/exemplar-engine-snapshot.json`. Coordinated own child processes were
  stopped and leases released. The extra controls used background semantic
  automation while the Mac was locked; no native UI screenshot proof is claimed
  or required for this content-only change.
- Code-map and whitespace checks pass. Architecture audit retains 24 existing
  C++ size triggers; no C++ source changes were introduced. The incidental
  shipped-fixture canonical repairs landed separately as PR #190.
