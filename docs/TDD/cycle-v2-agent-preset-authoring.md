# Cycle V2 Agent Preset Authoring Roadmap

## Status

The first wave is merged:
[preset/node explanations](https://github.com/DavenH/Amaranth/pull/185),
[synthesis capabilities](https://github.com/DavenH/Amaranth/pull/183),
[performance suites](https://github.com/DavenH/Amaranth/pull/184), and
[reference studies](https://github.com/DavenH/Amaranth/pull/186).
[Semantic candidate authoring](https://github.com/DavenH/Amaranth/pull/188) and
[audition reports](https://github.com/DavenH/Amaranth/pull/187) are merged, along
with [factory preset refinements](https://github.com/DavenH/Amaranth/pull/189).
[Group explanations and cards](https://github.com/DavenH/Amaranth/pull/192) are
merged, with native input verification and screenshot review complete.
[Explained exemplars](https://github.com/DavenH/Amaranth/pull/191) are merged with
controlled audition evidence.
The bounded [saxophone pilot](cycle-v2-expressive-saxophone-pilot.md) is
implemented. Its single recording permits exploratory comparisons; the
expressive-reference and autonomous-search evidence gate remains open.
Additional reference collection is the next step.
Remaining slices are proposed.
Expand each nontrivial slice with the exact files, rough size envelope, lifecycle
contract, and negative tests required by [the TDD policy](README.md) before
implementing it. Dependencies below describe required behavior, not a mandatory
single stack of branches.

## Product Goals

- Humans and agents can inspect and author explanations of a preset and its
  components, through the same persisted annotation data.
- Agents can discover valid synthesis operations and evaluate reproducible
  candidates without relying on editor visibility or pointer automation.
- Reference collections constrain one expressive instrument across notes,
  articulations, expression combinations, and transitions. Fidelity to an
  isolated recording is insufficient.
- Synthetic exploration retains surprising, useful instruments. Reference
  resemblance, technical behavior, playability, taste, and complexity remain
  distinguishable judgments.
- Prefer compact, connected mechanisms. Selected-cycle fitting is an optional
  local initializer, with no assumption that it generalizes across timbre.

## Authoritative Sources And Shared Boundaries

| Responsibility | Reuse and ownership |
| --- | --- |
| Type capabilities | `NodeDefinitionRegistry` and `ParameterDefinition` own types, ports, constraints and impacts. |
| Physical parameter values | `lib/src/Audio/CycleDsp/EffectParameterMapping.h` owns existing conversions. |
| Graph/model edits | `GraphCommandDispatcher` owns semantic edits, transactions, undo and publication. |
| Persistence | `GraphSerializer` owns graph records; `PresetPresentationCodec` already persists preset descriptions. |
| Note UI | Existing console hover text and JUCE callouts/text editors supply reading and editing patterns. |
| Reference lifecycle | `ReferenceLibrary` and `ReferenceSampleAnalysis` own import, pitch analysis and reference state. |
| Render execution | `OfflineGraphAudioRenderer` owns offline execution; automation already parses scheduled MIDI and seeds. |
| Audio diagnostics | `scripts/cycle_audio_diff.py` owns existing alignment, gain, spectrum, harmonic and cyclogram comparisons. |
| Local shape fitting | Shared `AutoModeller` owns fitting; Cycle 1 selected-cycle modelling is the mature behavior reference. |

Keep experiment scheduling, search, analysis and artifact management in host-side
tools where possible. C++ additions expose existing domain facilities through
narrow adapters. Do not add a second renderer, mesh fitter, parameter-conversion
schema, or UI-based synthesis algorithm.

## Proposed PRs

### 1. Add editable preset and node explanations

Deliver optional node rationale and reuse the existing preset description.
Include an information action within the node header, hover reading through
the existing console, a shared text-editing interaction, and direct agent
read/write commands in the same PR. Do not duplicate console explanations in
hover popups or position the information control outside the node's bounds.
Console reading preserves authored line breaks and wraps at a readable size.
Keep generic type help in the definition registry. Empty notes need no expanded
editor area.

Done when a human and an agent can create, inspect, edit, save/reload and undo
notes. Node duplication and deletion/undo preserve sensible note ownership.
Typing and committing notes do not prepare meshes, compile the graph, reset
processors, or copy unrelated graph content on every keystroke.

### 2. Add group explanations and attached information cards

Depends on 1. Extend the annotation model to group rationale. Make attached
canvas cards an optional view of the same text shown by badges and editors;
store their placement separately. A free card may reference multiple stable
node IDs if that workflow is included in the slice design.

Done when cards follow movement, group collapse, deletion/undo and save/reload
predictably. They remain document presentation data and introduce no audio
ports, processing attachments, runtime steps or DSP node-kind branches.

### 3. Expose synthesis capabilities to agents

Independent of 1-2. Export node types, ports, defaults, constraints, model
schemas, processing scopes and existing physical mappings from their canonical
owners. Include known control timing: continuous, note-on latched, or static
authoring configuration. Add concise guidance for supported operations without
claiming a universal perceptual control such as "make brighter."

Done when an agent can discover valid edits and interpret parameter values
without opening an editor or reading C++. Exported values agree with existing
validation and conversions. Unknown perceptual effects remain unspecified.

### 4. Complete semantic candidate authoring

Use existing graph/model commands to support validated curve/mesh replacement
or bounded bulk edits, parameter changes, and atomic multi-edit candidates.
Agents can save a baseline, create candidates, and discard a failed candidate
without pointer gestures or direct JSON mutation. Capability export should
advertise the resulting operations.

Done when one script creates two different valid, reloadable candidates and
invalid input leaves no partial durable edit. Domain codecs and command
services retain validation and undo ownership. Do not introduce a competing
document or snapshot system.

### 5. Render repeatable performance suites

Independent of the annotation UI. Define a compact host-side suite over the
existing renderer: notes, velocities, articulations expressible by scheduled
MIDI, CC trajectories, seeds, rates and render settings. Write WAVs and
manifests identifying the exact preset content and performance. Use the shared
musical pattern library for authored demonstrations where appropriate.

Done when repeated controlled runs reproduce results, each case resets runtime
state, and attacks, tails, repeated notes and mid-note CC changes are captured.
Execution has explicit budgets and does not require GUI gestures per candidate.
GUI automation remains sequential; independent isolated renders may batch.

### 6. Make labelled reference collections inspectable

Expose direct file import and numerical analysis export through the existing
reference owner. Add host-side study manifests referencing library recordings,
crops, pitch, articulation, dynamics, expression labels and confidence. Keep
observed labels distinct from inferred ones and unknown controls unspecified.

Done when a study can collect several recordings without manual file choosers
and retain their labels and analysis. References remain independent of preset
audio nodes. Unsupported or insecure pitch assumptions are visible. Do not
duplicate recordings or analysis merely to place them in a study.

### 7. Compare instrument responses and produce audition reports

Depends on 5-6. Extend existing diagnostic comparisons with time-varying
envelope, spectral/harmonic and noise descriptors; make alignment, gain
matching and confidence explicit. Report attack, sustain and release separately
where applicable. Compare collections and trajectories while keeping withheld
cases outside the optimization objective.

Produce matched-loudness A/B artifacts, plots and a small reviewable report.
Keep raw levels available. Record human preferences against exact artifacts
separately from reference similarity. Never silently time-warp away an
articulation mismatch or force noise and polyphonic material through an
unreliable pitched analysis.

Done when known timbral, attack and transition differences produce useful
diagnostics, and matching endpoints does not hide a poor recorded transition.
Unobserved transitions can receive technical/subjective review but cannot be
reported as verified reference matches.

### 8. Publish a small explained exemplar bank

Uses 1, 3 and 5. Existing comparisons suffice for initial before/after evidence;
7 can enrich the reports without blocking this slice. Curate representative
examples covering time/spectral shaping, articulation, noise/guides, expression
routing and selected effects. Document sonic intent, component responsibilities,
effective controls, useful range and weaknesses. Include exact controlled edits
with before/after renders to substantiate important causal explanations.

Done when agents can inspect and modify unfamiliar examples using their notes
and capabilities, and the claimed effects have demonstration evidence. Mark
interpretations as hypotheses where unverified. Do not require annotating the
entire factory bank before this slice is useful.

### 9. Build a first expressive saxophone study

Uses 1 and 4-7, with 8 as useful source material. Build a deliberately limited,
manually guided pilot with one shared synthesis structure. Start with a few
expressive controls supported by the available reference coverage, rather than
promising the complete saxophone expression space.

Evaluate multiple registers, dynamics, attacks, control combinations and
trajectories. Reserve reference cases and performance combinations before
refinement. Explain which shared mechanisms account for the observed changes.
Assess useful expression and economy as well as individual timbre matches.

The study must report failures and missing capabilities. Current Trimesh
modulation is live, while Envelope morph is latched at note-on; do not promise
continuous envelope adoption through authoring automation. If evidence requires
new runtime state, modulation composition or DSP, design it as a separate
follow-up PR with a reproducer and continuity/realtime contracts.

Gate: establish an attainable modelling target and useful comparisons before
building general autonomous search around it. A documented failure of the
current synthesis vocabulary is actionable evidence, not a successful model.

### 10. Add constrained search and simplification

Depends on 3-7 and the pilot's evidence gate. Search declared parameters or
compact shape coordinates within a selected architecture. Keep budgets, seeds,
candidate lineage and checkpoints. Select finalists before evaluating held-out
cases; withheld scores must not enter search fitness or candidate selection.
Retain alternatives across fidelity, playability and complexity; use ablation
and simplification to identify components that earn their cost.

Done when a bounded run improves an attainable benchmark, reproduces its
candidates, explains the semantic edits, and exposes held-out regressions.
Extra gain or unnecessary geometry must not win by default. Keep unrestricted
topology invention and learned inverse-synthesis training outside this PR.

### 11. Build a discovery archive for synthetic presets

Uses 5, 7-8 and 10. Explore without requiring a literal target recording.
Maintain several promising instruments across audible behavior descriptors,
with parentage, annotations, control maps and musical demonstrations. Human
preferences and musical usefulness guide selection alongside diversity.

Done when a run yields distinct, reviewed playable discoveries, with register,
velocity and control-trajectory evidence. Novelty concerns rendered behavior,
not parameter distance. Criteria must permit intentional grit, roughness and
unusual regimes rather than rewarding conventional smoothness by default.
Discovery is relative to the available library/archive, not a claim of global
originality.

### Optional 12. Add a calibrated audio-critic adapter

Depends on 7. Expose bounded listening requests to a suitable audio-capable
model, independent of the coding agent. Start with blind, order-balanced A/B
comparisons and controlled perturbations, then compare its judgments with human
preferences. Keep provider/model identity and disagreements with each result.

Done when the adapter demonstrates useful discrimination on the intended
Cycle tasks and reports its limits. Its critiques remain separate from numeric
measurements and authored intent. No mandatory cloud dependency or model
training is needed for the preceding PRs.

## Sequence And Parallel Work

- Start 1, 3, 5 and 6 independently. Coordinate shared automation registration
  edits; independent branches do not imply simultaneous GUI automation.
- Follow 1 with 2; build 4 alongside capability discovery using existing domain
  command contracts. PR 2 does not block experiments.
- Follow 5-6 with 7. Begin 8 as soon as notes and render artifacts are usable.
- Complete the bounded saxophone study before committing to general search.
  Design only demonstrated engine gaps as additional PRs.
- Add constrained search, then the synthetic discovery archive. The optional
  critic can be evaluated after 7 without blocking either human-led pilot.

Each PR carries one focused end-to-end demonstration and the proportionate
semantic, audio or UI evidence. Apply the normal architecture audit and size
review to nontrivial C++ changes. This roadmap does not mark the complete
saxophone model, autonomous taste, or novel-preset quality as solved.

## First Wave Verification

The four independent slices merge cleanly and build together as the exact
`CycleV2` and `CycleV2_tests` targets. The combined domain run passes 19 cases
and 42,485 assertions; the two host suites pass 24 tests. The annotation branch
also passes seven explanation cases without JUCE lifecycle diagnostics, its
29-command fixture, and native click/type/Save/undo/preset/Escape checks.
The combined code map has 100 entries and the architecture audit retains the
existing 24 size triggers. Local aggregate evidence is recorded in
`.context/integration-build.log`, `.context/integration-domain-tests.log` and
`.context/integration-state.json`; each slice TDD records its runtime evidence.
