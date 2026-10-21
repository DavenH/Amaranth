# Cycle V2 saxophone reference calibration

Status: Implemented for this bounded calibration slice. Continues roadmap slice 9
after the merged feasibility pilot; the complete expressive instrument is pending.
This is a manually guided straight-tone ablation and reference comparison,
not general search or a complete saxophone model.

## Experiment and evidence gate

Use the predeclared VCSL acquisition selection: eight objective recordings and
four reserved D3 parents. Preserve original source hashes, partition, filename
labels and unknown dynamics. Work at native 48 kHz without resampling. Iowa
scales and the continuous solo remain outside this slice: full-file reference
decoding is capped and current crops do not retrack a bounded source region.

Objective preparation through the existing reference library estimates VCSL
C2 near 130.819 Hz and C4 near 523.22 Hz, corresponding to standard MIDI 48/72;
C3 staccato is near MIDI 60. These remain inferred pitch labels. Reference
exported note numbers use Cycle's convention; synthesis uses standard MIDI.
Keep sounding frequency, filename token and both note conventions distinct.
All source/renderer pitch diagnostics describe effective tracker results,
not untouched detector observations or human-verified musical labels.

The existing `saxophone-3` probe at notes 48/72 shows body pitch movement wider
than the corresponding NV reference. The declared candidate disables only
`pitchEnvelope1.enabled`, which returns neutral 0.5/zero semitones in both
existing envelope paths. Do not use `level=0` as bypass. Keep topology, geometry,
other parameters, guides, effects and resources identical. Test this factor
alone; report any vibrato/character loss separately. No improvement in taste,
complete timbre fidelity or economy is implied by straighter tracked pitch.

Use one shared structure, explicit CC1/2/3 initialization and fixed synth
velocity as a calibration hypothesis; source velocity layers are not verified
MIDI velocities, and vibrato variation tokens are not dynamic layers. Compare
declared early attack/body windows for sustained notes, and a short trajectory
for staccato. Do not invent an observed note-off time or compare synthetic
release against a still-sustaining reference. Keep timing unaligned.

## Canonical owners and permitted changes

- Existing `ReferenceLibrary`, `ReferenceSampleAnalyzer`, `PitchedSample` and
  `PitchTracker` retain decode/tracking/folding. Use `collect_study` for source
  and first-repetition synthesis WAV analysis; no second pitch detector.
- `cycle_reference_study` retains labels/crops/provenance and cached exports;
  `render_suite` retains scheduling, fresh runtime, repeats and engine/WAV/PCM
  identities. `generate_report` retains raw/RMS comparisons and spectra.
- Existing `applyNodeEdits`, undo, save/reload and explanations retain model
  mutation/publication. Host checks one exact enabled-field delta.
- Factor the first pilot's small JSON/hash/freeze helpers into one host artifact
  owner and have both studies use it. Delete those duplicate helper bodies from
  the old runner; preserve its command line, schema and frozen artifact policy.
  Do not grow its strict one-growl/44.1 kHz content validator into a search DSL.
- A reference-calibration helper owns fixed content/source mappings and a small
  pitch-evidence reducer/plotter owns statistics over exported frequency frames.
  No C++, DSP, new node kinds, parameter mappings or realtime behavior changes.

The only numerical additions are bounded selection/reduction of existing
frequency frames into median, cents offset and 5–95 percentile spread, plus
plots against original elapsed time. Reuse existing WAV/tone helpers for a
declared octave sanity check if needed. Do not infer pitch success from centroid
or fit a pitch path with a new tracker. Suppress unsupported comparisons and
retain insecurity/fallback flags. Harmonic report evidence remains unpitched
unless its existing observed-pitch contract is independently satisfied.

## Stages and identity

Objective stage records the corpus/partition/plan/source identities before any
edit, authors the ablation through one semantic commit, verifies exact unrelated
state and undo/redo/reload, then renders objective baseline/candidate cases with
two interleaved repeats. Collect source and synthesis pitch evidence, generate
reference/intervention auditions and preserve failed hypotheses.

Freeze candidate, baseline, corpus, partition, plans, source and analysis files,
engine and every objective artifact before audit. Audit verifies the freeze,
then imports/analyzes/renders only the reserved parents with the unchanged
candidate. No audit command may edit/select a candidate. Metadata/hash checks
are permitted before audit; reserved audio is not decoded for refinement.
Reserved D3 cases test within-family interpolation, not independent-performer
generalization. Invalid inputs and unsafe output reuse fail before commands;
partial failures remain recorded and cannot become successful freezes.

## Files, size and complexity

Expected boundaries: shared artifact helpers ~65–90 Python lines extracted from
the first pilot; fixed calibration plan/validation ~150–220 lines; stage driver
~200–280 lines; pitch evidence/plots ~100–160 lines if separate ownership helps.
Prefer fewer files when responsibilities stay clear. Keep total new production
code near 500 lines plus extraction, using existing performance/reference/report
schemas. Add meaningful focused tests, content, runbook/feature route updates,
and experiment evidence. No factory/source audio changes or duplicate bank.

Bound captures to 48 rendered WAVs / 24 repeat pairs and <=160 audio seconds across
objective and audit. Track analysis/report budgets separately. Work scales with
chosen clips/frames; interaction, graph publication and realtime costs remain
unchanged. Do not add compatibility code; stable end state is two fixed-content
studies sharing existing artifact ownership and canonical domain services.

## Acceptance

- Source identities/parent splits and inferred note/rate mappings validate;
  reserved decoding cannot occur in objective. Unknown source controls remain
  unknown and source files retain original level/timing.
- Candidate changes only the enabled flag and shared authored notes, with exact
  one-factor/history/reload proof and unchanged geometry/topology counts.
- Source and synthetic pitch statistics/traces retain original time and
  confidence; an NV claim names the measured pitch-spread change. Vibrato and
  staccato limitations/regressions remain visible. Reports do not claim taste.
- Exact repeats, finite nonzero audio, source/analysis/report hashes and relative
  audition links verify; withheld reports never enter objective selection.
- Changed frozen source/plan/candidate/engine/artifact or output reuse rejects
  audit before commands. The original pilot and its tests remain unchanged in
  behavior after shared-helper extraction.
- Exact app/coordinated end-to-end proof, focused tests, independent code and
  artifact review, code-map/architecture/whitespace checks pass. Record actual
  sizes, reuse, evidence limits and newly discovered feature requests before
  marking this slice complete.

## Experiment results — 2026-10-21

Exact `CycleV2` build and coordinated objective/audit runs completed in the
implementation worktree. Local evidence is `.context/sax-calibration-first/`:
`index.html` links source/baseline, source/candidate and baseline/candidate
auditions for both partitions, with raw/RMS audio, spectra and pitch traces.
`split.json`, `freeze.json`, command logs and saved graphs retain lineage.
The candidate passed one-factor, semantic revision, undo/redo and reload checks;
the audit used it unchanged. The factory preset and source recordings are intact.

The observed reduction concerns effective pitch-path spread, using inclusive
5–95 frame percentiles over the declared body window. Filename note tokens
remain distinct from inferred standard MIDI. Here, layers are source tokens,
not matched velocity settings:

| Partition / source | Reference spread (cents) | Baseline | Pitch bypass |
| --- | ---: | ---: | ---: |
| Objective NV C2 vl2 / vl3 | 1.159 / 1.015 | 13.857 | 0.479 |
| Objective NV C4 vl2 / vl3 | 1.090 / 0.738 | 14.523 | 0.108 |
| Reserved NV D3 vl2 / vl3 | 0.564 / 0.405 | 12.907 | 0.169 |

All six NV comparisons move closer in this narrowly defined statistic. The
reserved D3 median offsets are +0.106/−0.074 cents in the source, −0.265 in the
baseline and −0.034 with bypass. These tracker-derived measurements do not
establish perceptual equality or independently observed musical labels.

The same bypass loses variation present in vibrato references: source spreads
are 11.240/25.550 cents at objective C2/C4 and 11.959 at reserved D3, while
bypassed synthesis remains below 0.5 cents. Baseline spread alone does not prove
correct vibrato shape/rate either. Staccato spread remains diagnostic only:
short, attack-contaminated windows are excluded from straight-tone success.
This is useful ablation evidence, not a universal preset improvement. Live
envelope adoption and region-level source analysis remain feature-queue M01/M02.

Structure stays at 23 nodes, 28 edges, five guides, 1,176 vertices and 147 cubes.
The disabled pitch envelope still owns its 840 vertices/104 cubes; bypassing it
does not establish a safe simplification or an economy improvement. CC response,
dynamic matching, connected-note behavior, timbre fidelity, playability and
human taste remain unvalidated. Acoustic-model and search gates remain closed.

The three portable acquisition records (50,168 bytes) are tracked under
`vcsl-corpus/`; its recipe reconstructs the pinned corpus from public original
URLs with SHA checks. No audio is committed. Shared helper extraction removes
the old runner's duplicate JSON/hash/freeze mechanics; stable ownership is
reference collection, suite rendering, audition reporting, fixed calibration,
pitch reduction and immutable artifact boundaries. New production sizes are
131 lines for pitch evidence, 164 for fixed calibration, 250 for orchestration
and about 65 for extracted artifact ownership. There are no C++ changes or new
runtime/state-machine decisions; the architecture audit retains 25 existing
triggers and the code map remains at 100 entries.

Review found and repaired missing analysis SHA/reference-ID checks and missing
end-of-audit revalidation. Regression cases reject corrupted analysis and
candidate mutation during a stage. The real objective evidence was independently
verified against recorded hashes before audit; the fixes change validation,
not captured audio or the selected candidate. Independent artifact proof lives
outside the frozen output at `.context/sax-calibration-artifact-audit.json`.

Final verification: 105 relevant host tests pass, including the original pilot,
performance/reference/audition/exemplar owners and new calibration boundaries.
Independent code review approves the repaired identity checks and shared
ownership. Independent artifact review reports no findings: 65 frozen inputs,
242 objective artifacts, 48 native 48 kHz/stereo/24-bit captures forming 24 exact
PCM pairs, and 132 audio seconds (88 objective, 44 audit). It reproduces all
36 pitch summaries and verifies 216 audition asset hashes and 232 local HTML
links. The 88 objective commands decode no reserved parent; the 48 audit
commands contain no authoring and decode no objective parent. All eight source
and sixteen synthetic objective observations, plus four source/eight synthetic
audit observations, match recorded analysis/reference/audio identities.
Whitespace, code-map and architecture checks pass. The two stages share the
same executable identity; only the owned coordinated app is stopped afterward.
