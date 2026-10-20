# Cycle V2 audition reports

Status: Implemented

## Purpose and contract

Create a host-only, reviewable comparison of explicitly paired saved performance
renders and labelled reference-study observations. Report phase and trajectory
differences without editing presets or claiming to judge taste. Reference
similarity and human preference remain separate evidence.

A `cycle-audition-study.v1` manifest selects 1–32 cases. Each case has an `id`,
`partition` (`objective` or `withheld`), `a` and `b` artifact selectors, and
explicit `intervals` with `id`, `role` (attack/sustain/release/trajectory),
`startSeconds` and `endSeconds`. A selector names either `performanceResults`,
`caseId` and optional `repetition`, or `referenceStudy` and `observationId`.
Relative paths resolve against the manifest. Reference observation crops define
the local origin; performance captures start at zero. Optional selector `crop`
selects a smaller interval relative to that origin. Compared intervals use the
same elapsed times, with no stretching or endpoint interpolation.

Verify saved source hashes before analysis. Preserve input/report/audio hashes,
preset/engine/performance identities, labels with their observed/inferred origin
and confidence, selected crops, reference tracking confidence and fallback, and
analysis settings in immutable report JSON. Reject incomplete suites, missing
selectors, altered artifacts, invalid crops, rate mismatches, existing output
directories and exhausted limits. Support mono/stereo pairing through channel
energy, preserving stereo audition and channel level evidence.

## Existing owners and permitted changes

- `scripts/cycle_audio_diff.py` owns WAV decoding, RMS, fixed-lag correlation,
  spectrum/FFT and harmonic projection. Reuse `read_wav`, `rms`, `alignment`,
  `average_spectrum` and harmonic helpers unchanged.
- `scripts/cycle_performance_suite.py` owns suite JSON/hash/validation helpers;
  `render_cycle_v2_performance_suite.py` owns saved suite artifact structure.
- `scripts/cycle_reference_study.py` owns reference study/observation provenance
  and source hashing. Reuse its saved reports rather than importing/retracking.
- New adapters resolve saved artifacts, validate limits, reduce existing spectra
  into explicitly named descriptors, and produce local review artifacts.

Do not call `analyze_pair`: it assumes pitched sustain and implicitly aligns
and fits gain. Do not duplicate FFT, resampling, pitch tracking, cyclograms,
scheduler or synthesis algorithms. No C++/app/GUI changes or cloud dependency.

## Analysis policy

Raw and frame RMS use mean energy across channels; anti-phase stereo must not
become apparent silence. Spectral descriptors use channel-averaged power from
the existing spectrum owner. Frame traces include RMS, spectral centroid,
flatness and approximate declared-harmonic-band/remaining energy. Remaining
energy is not measured noise or HNR. Descriptor differences remain separate;
this slice has no weighted taste or optimization score.

Pitch defaults to unknown. Harmonic evidence requires explicit fixed-pitch,
observed, confident, monophonic evidence, sufficient FFT-bin separation, and no insecure/fallback reference
tracking. Inferred/unknown/noise/polyphonic cases remain unpitched, with the
reason recorded. No tracker is invented. Varying-pitch cases need narrower
fixed-pitch intervals.

Default alignment is none. Optional fixed-lag alignment requires a calibration
interval and a maximum at most 10 ms. Reuse existing bounded correlation once,
apply the same lag throughout, expose overlap/discarded edges and retain raw
unaligned traces and differences. Never align each phase independently or warp
time. Alignment evidence based on a mono channel is explicitly identified;
anti-phase cancellation cannot silently produce a valid calibration.

Optional audition level matching uses one positive RMS scalar per full selected
crop, attenuating toward the lower pair RMS and sharing extra headroom
attenuation if needed. Silence has no available match. Retain raw A/B WAVs and
levels. Call this RMS matching, never LUFS or perceived loudness matching.
Analysis remains at raw levels; matching does not normalize each phase.

Withheld cases appear separately and never enter objective aggregates. Every
interval retains its full trace so matching endpoints cannot conceal a poor
recorded transition. The report contains actual A/B players, standalone plots,
raw numerical evidence and exact artifact identities. No report claim validates
an unobserved transition or proves subjective elegance.

## Files, budgets and lifecycle

Expected production boundary: `scripts/cycle_audition_study.py` (~190 lines),
`scripts/cycle_audition_analysis.py` (~190), and
`scripts/report_cycle_auditions.py` (~220); target <=600 lines. Add focused tests,
a sample fixture, a short workflow doc and an existing code-map feature route.
Use matplotlib for standalone plots, documented as a host-only dependency.

Limits: 32 cases, 120 aggregate selected audio seconds, 4,096 total raw/aligned descriptor windows (conservative preflight estimate),
64 MiB per source WAV/JSON, two million frames per source WAV, PCM mono/stereo and finite samples. Decode one selected
pair at a time with a cache of repeated interval spectra within each case. Work is offline, O(selected samples plus
windows × FFT complexity); no interaction, publication or realtime path changes.
Output requires a fresh directory. No temporary compatibility layer or deletion
target exists: stable end state is a host adapter over saved immutable artifacts.
If numerical reuse would require copying an algorithm, stop for review.

## Acceptance and verification

- Known slower attacks, added harmonics/noise, and changed middle transitions
  despite identical endpoints produce the corresponding phase/trace differences.
- Anti-phase stereo has correct nonzero RMS and spectra and available RMS A/B.
- Alignment defaults to none; a bounded calibrated lag retains raw discrepancy
  and reports overlap. Global RMS matching preserves envelope ratios.
- Unknown, inferred, noise/polyphonic, insecure and fallback pitch stays
  unpitched. Silence remains finite and explicitly unmatched.
- Withheld cases are present but excluded from objective aggregate evidence.
- Changed hashes/incomplete suites/out-of-range windows fail; source labels,
  confidence and identities survive into JSON and working relative report links.
- Generate a report from existing repeated candidate captures, moving vs constant
  CC captures, and factory/reference study artifacts without new GUI automation.

Before marking Implemented, record production sizes, reuse and absence of
kind/DSP duplication, remaining compatibility/deletion state, focused tests,
end-to-end artifact evidence and code-map/whitespace checks below.


## Completion evidence

- Production: 519 added Python lines, no production deletions; resolver 187,
  analysis 145, report 187 lines. Three focused responsibilities remain separate.
  No new node-kind branches, C++ ownership, app lifecycle or realtime changes.
- Existing WAV decoding, RMS, fixed-lag alignment and spectra/FFT are imported
  unchanged from `cycle_audio_diff.py`; suite validation/hash helpers and
  reference hashing are reused. Harmonic fractions are new bounded reductions
  over those spectra, with explicit low-frequency resolution rejection.
- Input provenance validates saved preset snapshots/effective graphs, embedded
  suite/reference documents, selected reference analysis and source WAV hashes.
  Historical executable/original file paths remain metadata. Inconsistent
  copied observation confidence/labels/crops cannot enable pitched analysis.
- No compatibility code remains and no mature algorithm was replaced or copied.
  Ordinary app edits/publication are unaffected; offline work scales with the
  selected pair and explicitly bounded analysis windows. Calibration is capped
  at eight million correlation sample pairs.
- 53 focused Python tests passed across audition reports (14), existing audio
  diagnostics, suites and reference studies. Signals cover attack/release,
  middle-transition differences despite equal endpoints, harmonic/noise
  perturbations, anti-phase stereo, default/fixed lag, one global RMS gain,
  silence, unknown/inferred/polyphonic/insecure/fallback/under-resolved pitch,
  legal full-scale quantization, above-Nyquist pitch, saved identity corruption,
  withheld exclusion, output reuse and actual report-relative A/B artifacts.
- End-to-end report generated from existing saved suite/reference artifacts:
  four cases, 16 A/B WAVs and eight standalone PNG/SVG plots. Exact repeated
  candidate captures and pre-change CC intervals have zero descriptor
  difference. Constant vs moving CC transition differs by RMS 0.00353003 and
  centroid 147.004 Hz; transition traces retain that evidence. Growl and
  insecure noise references appear separately as unpitched withheld cases.
  Artifact hashes/dimensions and RMS-match equality were verified, and the CC
  plot was inspected at production size. No new app launch or GUI lease needed.
- Local evidence: `.context/focused-tests.log`, `.context/e2e-final/index.html`,
  `.context/e2e-verification.json`. Code-map and whitespace checks pass. The
  architecture audit retains its 24 existing C++ triggers; no C++ files changed.
