# Cycle V2 explained exemplars

Status: In Progress

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
Record exact payloads and engine/source/performance/PCM hashes. Reject a
non-effective intervention rather than publishing a demonstrated-effect claim.
Use explicit held-note/release/CC intervals without automatic alignment. Notes
qualify effects not established by the measurements as authored hypotheses.

## Files, size, dependencies and deletion

- Four existing factory `.cyclegraph` files: approximately 20–25 short rationales
  plus four preset design notes; preserve existing JSON layout and synthesis.
- `cycle-v2/content/exemplars/index.json` and `performance-suite.json`: four
  manifest entries and one bounded suite covering register, velocity and timed
  CC, with controls/limits and reproducible comparison intervals.
- `scripts/render_cycle_v2_exemplars.py`: approximately 200–260 lines, fixed
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
