# Cycle V2 expressive saxophone pilot

Status: Implemented bounded feasibility pilot for roadmap slice 9. The complete
expressive-instrument/reference-coverage gate remains open; this study cannot
validate it with its single recording. Further reference collection is a
separate continuation, not a change to the frozen experiment.

## Attainable target and evidence boundary

Use the existing `saxophone-3.cyclegraph` as one shared synthesis structure.
CC2 selects live time/magnitude mesh morphs; CC1 selects the volume envelope at
note-on. Hold CC3 fixed. Test note/register, velocity, short/sustained attacks,
controller endpoints and trajectories, and reserved combinations. These are
technical synth performances, not observations of saxophone expression.

The pilot's sole reference is `references/sax-growl.wav` (mono,
44100 Hz, 3.929 seconds). Existing copies are identical. Growl is an inferred
label; pitch, dynamics, attack, controller settings and recording provenance are
unverified. Compare one declared body interval with unpitched descriptors and
raw/RMS A/B. Do not infer an observed pitch from tracker confidence or filename.
The reference export uses Cycle's note convention (81 = 440 Hz); audition
diagnostics use standard MIDI (69 = 440 Hz). Never copy its note number into a
pitched audition declaration. Keep pitch unspecified for this study.

There is no independent held-out recording. Reserve performance combinations
before any candidate edit; they test technical generalization only. Missing
register/dynamic/attack/trajectory recordings must remain visible in the report.
Do not unblock roadmap search, claim acoustic fidelity, or claim tasteful or
elegant synthesis from numerical differences alone.

## Fixed manually guided candidate

One reviewed slower-peak hypothesis doubles only the phase coordinates of
volume-envelope peak vertices 8–15. Assert cube 1 contains exactly those IDs,
all amplitudes are 1, the original phase range is approximately .002–.032 and
the following sustain cube begins above .9. Preserve topology, amplitudes,
curvature, markers, routing, guides, parameters and other models. Increment both
codec revisions and use `inspectNodeControls` / `applyNodeEdits` with expected
document/model revisions. Verify unrelated-state identity, undo/redo and reload.
This is a declared intervention for review, not automatic candidate selection
or a promise of improvement. No held-out measurement chooses or changes it.

## Ownership and reuse

- `cycle_reference_study.read_study` / `collect_study` retain reference import,
  analysis, crops, confidence and hashes. No new decoder or tracker.
- `cycle_performance_suite.load_suite` and
  `render_cycle_v2_performance_suite.render_suite` retain schedule validation,
  fresh renderer state, seed/reset policy, repeated PCM and exact identities.
- `cycle_audition_study.load_study` / `report_cycle_auditions.generate_report`
  retain immutable source resolution, descriptors, raw/RMS WAVs and plots.
- Existing graph commands retain model validation, undo and persistence. The
  host only translates the fixed content recipe and coordinates saved runs.
- `cycle_automation.py exec` and its session/identity helpers retain exact app
  lifecycle/leases. Use the isolated child instance; no pointer checks needed.

No C++, DSP, NodeKind, graph schema, realtime path or factory graph changes.
Do not import exemplar-specific orchestration, duplicate FFT/scheduling/mesh
algorithms, mutate source JSON as an authoring mechanism, or create a generic
search framework. There is no compatibility layer or mature deletion target;
the stable end state is a small content-specific host workflow.

## Stages, identity and limits

An objective stage validates the entire plan, saves split/source identities
before editing, authors the one candidate, renders only objective performances,
collects the reference and creates separate response/intervention/reference
auditions. It freezes candidate, source, suite, split, reference and engine
identities. It must not resolve/render reserved performances at this stage.

A final audit stage requires this freeze and rejects changed inputs, candidate,
source, reference or engine before rendering. It renders reserved performances
with the same baseline/candidate and produces a separate withheld report.
Existing report partition labels alone are not a leakage boundary: withheld
audio must not be loaded during the objective stage. No candidate edits occur
in audit. Partial failures persist an explicit failed result; fresh objective
output is mandatory and successful audit output cannot be overwritten.

Use explicit scheduled MIDI, initial CC1/2/3 values in every case and a verified
unused CC118 schedule control. Keep CC1 note-on timing honest. Each graph/stage
uses two interleaved repeats; total expected renders <=64, <=120 audio seconds.
Use native 44100 Hz, same engine and seed, no alignment or time warp. Controller
response must exceed measured same-time schedule drift; preserve full boundary
diagnostics rather than assuming sample-exact onset. Nonzero finite repeated
audio is required; unexpected silence/nonrepeatability is a failure, not a score.

## Files and size envelope

- `cycle-v2/content/studies/expressive-sax/`: a fixed study plan and existing
  suite/reference schema documents; no duplicated factory presets/audio.
- `scripts/cycle_saxophone_study.py`: bounded content validation, recipe and
  structure/economy checks, approximately 150–220 lines if needed.
- `scripts/run_cycle_v2_saxophone_study.py`: stage orchestration and review
  summary, approximately 250–350 lines. Keep source/recipe policy separate if
  combining would obscure ownership; target <=550 new production Python lines.
- Focused Python semantic tests, one canonical runbook/code-map route, this TDD.

Offline cost scales with selected captures and existing analysis budgets.
Ordinary edits, interaction complexity and realtime behavior are unchanged.
Record node/guide/vertex/cube counts, especially the existing 840-vertex,
104-cube pitch envelope; node count alone cannot establish economy.

## Acceptance

- Plan/splits are recorded before refinement; objective and reserved case IDs
  are distinct and belong to their declared stage. Missing/duplicate/malformed
  cases, invalid recipe/source identities and unsafe output reuse fail clearly.
- One candidate is authored semantically; exact one-factor, history/reload,
  shared structure and unchanged source files are verified.
- Two repeated nonzero finite renders per case, controller response/drift and
  intervention attack differences are exposed without taste claims. If a
  hypothesis fails, retain that outcome and do not describe it as successful.
- Audit cannot use a changed frozen input, cannot overwrite a completed audit,
  and cannot modify/select the candidate. Withheld diagnostics never enter
  objective selection/summary. No withheld reference exists or is fabricated.
- Reports expose effective control timing, missing coverage, unpitched growl
  comparison, economy and weaknesses, raw/matched audition and exact lineage.
  Roadmap acoustic/search gate remains explicitly insufficient.
- Focused tests, exact app build/coordinated end-to-end run, independent review,
  code-map/architecture and whitespace checks pass. Record incidents in the
  established ledgers and actual file sizes/reuse before marking this bounded
  pilot implementation complete.

## Completion evidence

The exact standalone app builds at merged baseline `8b1a189b`; no C++ changed.
The own-instance coordinator ran objective and audit separately on PID 69758
with executable SHA256
`6340bbc1278f7e6d21e5e2e48e8e400a59272b622c4cc6cd9730376edd89da1b`.
Only this owned process was stopped after the runs. No factory graph or source
recording changed. A fresh reference export confirmed the actual bundled audio
matches declared SHA256 `26bcd6345e46bd9d94af443367258a2e8f26db3683082f75a4b4b6984564f041`.

The candidate passed exact one-factor identity, one semantic commit, undo,
redo and reload, retaining the same shared preset explanation. Six objective
and four reserved performances produced 40 source captures / 20 exact repeat
pairs, with finite nonzero output. Reserved audio was rendered only in audit;
source, candidate, suite, split, reference, objective artifacts and engine
identities were verified first. Objective artifacts remain frozen after audit.
An independent audit rehashed all 40 captures, 90 report assets, seven frozen
inputs and 125 objective artifacts; all 60 relative audio links resolve. All
15 report cases remain unpitched. Objective commands contain only the 24
objective captures; audit commands contain 16 reserved captures and no edits.

CC2 motion exceeds the inactive CC118 control in RMS, centroid and flatness;
the control has zero measured descriptor drift. The mean absolute centroid
difference during motion is 325.747 Hz. CC1's initial-value intervention changes
attack and release; its note-on timing remains the existing engine contract.
The peak intervention changes attack RMS (anchor mean absolute difference
0.005848), with much smaller following-body/release differences. The anchor
plot was inspected. Neither these differences nor peak-coordinate scaling
establish preference. The unpitched growl-body centroid discrepancy is 345.624 Hz
for baseline and 346.345 Hz for candidate; this attack intervention does not
improve that descriptor. No candidate was selected using these measurements.

Full pre-200 ms CC2 diagnostics retain four differing stereo PCM samples,
first at 198.571429 ms, at most one 24-bit LSB. The 180 ms prefix is exact and
the held-vs-CC118 prefix is exact. The candidate retains CC2 response with an
exact held-vs-CC118 control, five pre-event samples differing by at most one LSB
and first difference at 198.752834 ms. Its 180 ms prefix is also exact.
This extends the existing controller-boundary
incident in `audio-bugs.md`; there is no sample-exact onset claim or DSP fix.

Both graphs retain 23 nodes, 28 edges, five guides, 1,176 mesh vertices and
147 cubes, including disabled geometry. The pitch envelope alone has 840
vertices and 104 cubes. The unchanged structure is inspectable, but economy
and the usefulness of that geometry remain unproven. One inferred-growl
recording cannot validate register, dynamics, attacks or expression transitions;
`acousticModelGate` and `searchGate` remain false. Additional observed recordings
and listening are required before the roadmap's next gate can pass.

Production consists of a 214-line fixed-content validator/recipe/economy owner
and a 260-line stage orchestrator (474 lines total), plus existing-schema data.
They reuse canonical reference, semantic editing, rendering, diagnostics and
report owners. No synthesis, tracking, FFT, scheduling or transaction algorithm
is copied. There are no new node kinds, realtime/UI paths, compatibility code
or mature deletion targets. The live-edit/publication cost is unchanged.

All 84 focused host checks pass: 18 pilot contracts plus 66 existing candidate,
suite, reference, audio-diagnostic, exemplar and report checks. The pilot tests
cover fixed splits, exact controls, invalid/changed source and recipe, unrelated
state, unsafe output reuse, failures, objective/audit isolation, changed nested
artifacts/engine and reference changes before freeze. Evidence in the
implementation worktree is `.context/sax-pilot-build.log`,
`.context/sax-pilot-host-tests.log`, `.context/sax-reference-audit/`, and
`.context/sax-pilot-first/index.html` with immutable reports, plots, WAVs,
`split.json`, `freeze.json` and result JSON. Source-map validation passes at
100 entries; architecture audit retains the baseline's 25 existing triggers.
Independent artifact audit is `.context/sax-pilot-artifact-audit.json`.
Independent code and artifact reviews pass with no confirmed blocker. They
verified source/reference identity, exact phase-only history, split isolation,
all WAV/PCM identities and report links, and the declared evidence limits.
