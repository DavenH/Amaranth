# Cycle V2 audition reports

Compare saved [performance suites](cycle-performance-suites.md) with each other
or with [labelled reference studies](../cycle-v2/README.md#reference-studies).
The report runs offline; it does not launch Cycle, change presets or rank taste.

Install the standalone plotting dependency in a host virtual environment:

```sh
python3 -m venv .context/audition-venv
.context/audition-venv/bin/pip install -r scripts/requirements-auditions.txt
.context/audition-venv/bin/python scripts/report_cycle_auditions.py \
    my-audition-study.json --output .context/audition-report
open .context/audition-report/index.html
```

Use `--validate-only` to inspect a resolved plan without installing matplotlib or
writing output. Output directories must be new; keep the complete report folder
when sharing it. Audio links and plots are relative to that folder.

## Explicit case mappings

```json
{
  "schema": "cycle-audition-study.v1",
  "name": "Attack and expression comparison",
  "rmsMatch": true,
  "cases": [
    {
      "id": "short-soft",
      "partition": "objective",
      "a": {"performanceResults": "baseline/results.json", "caseId": "soft-short", "repetition": 1},
      "b": {"performanceResults": "candidate/results.json", "caseId": "soft-short", "repetition": 1},
      "pitch": {"midiNote": 60, "origin": "observed", "confidence": 1, "monophonic": true},
      "intervals": [
        {"id": "onset", "role": "attack", "startSeconds": 0, "endSeconds": 0.1},
        {"id": "tail", "role": "release", "startSeconds": 0.15, "endSeconds": 0.4}
      ]
    },
    {
      "id": "reserved-transition",
      "partition": "withheld",
      "a": {"referenceStudy": "references/report.json", "observationId": "growl"},
      "b": {"performanceResults": "candidate/results.json", "caseId": "repeated-expression"},
      "intervals": [
        {"id": "motion", "role": "trajectory", "startSeconds": 0, "endSeconds": 0.3}
      ]
    }
  ]
}
```

The fixture `scripts/fixtures/cycle-v2-audition-study.json` is a starting point;
replace its paths and case selectors with existing saved artifacts. Paths resolve
relative to the audition manifest. Selectors require exactly one source kind.
Reference observations begin at their saved crop start. A selector can add
`"crop": {"startSeconds": 0.1, "endSeconds": 0.5}` relative to that observation
or performance origin. All case intervals use elapsed seconds from the selected
crop origin. A and B use the same elapsed intervals; separate crop starts can
establish an explicit correspondence. Whole crop durations may differ, and the
interval must fit both. No resampling or time stretching occurs.

Roles are `attack`, `sustain`, `release` and `trajectory`. Windows are authored
claims about corresponding phases; the tool does not invent segmentation or
infer unobserved transitions. Withheld cases are rendered in their own section
and excluded from `objectiveSummary`. The summary gives separate raw descriptor
means with equal weight per objective interval; there is no combined score.

## Reading the evidence

Raw and frame RMS average channel energy. Spectral descriptors average channel
powers, so opposite-phase stereo remains audible and retains its energy. The
existing Cycle diagnostic FFT supplies a 2,048-sample Hann spectrum every 512
samples; final short frames are padded only for spectral analysis. Plots use
window centers. Short onsets have limited spectral resolution, visible through
the recorded window settings.

Descriptors include RMS, spectral centroid and flatness. Explicit observed,
fixed-pitch, monophonic evidence with confidence at least 0.8 also enables
approximate harmonic-band fractions for harmonics 1–16 and the upper harmonics.
Each harmonic band extends 1.5 FFT bins on either side. Fundamentals at or below
three FFT bins lack sufficient harmonic separation and stay unpitched, with a
resolution reason; fundamentals at or above Nyquist also stay unpitched.
Energy outside these bands is **not measured noise or HNR**. Unknown/inferred
pitch, polyphonic
material, and insecure or fallback reference tracking stay unpitched; reasons,
original labels and confidence are retained. A successful tracker alone does
not establish monophony. Varying-pitch recordings need shorter fixed-pitch
comparisons or unpitched descriptors.

A/B players retain the authored timing and original channels. Raw cropped audio
is exported as 24-bit PCM, with original source hashes retained. Optional RMS
players apply one attenuation gain per complete crop toward the lower pair RMS;
shared headroom attenuation prevents clipping. This preserves within-crop attack
and trajectory dynamics. Silence has no available match. RMS matching does not
claim equal perceived loudness, and numerical analysis always uses raw levels.

Alignment defaults to none. An optional top-level policy enables a single fixed
lag, never phase-by-phase alignment:

```json
"alignment": {
  "maxLagMs": 1,
  "calibration": {"startSeconds": 0.1, "endSeconds": 0.12}
}
```

Calibration uses the highest-energy channel on each side and requires positive
correlation of at least 0.5. The maximum lag is 10 ms and calibration is limited
to eight million correlation sample pairs. Use a known timing calibration rather
than a phase-ambiguous sustained tone. The report keeps unaligned traces, the
fixed lag, calibration channels, discarded edges and actual overlap. Aligned
traces are additional evidence; audio playback stays unshifted.

`report.json` stores raw/aligned interval traces, exact source/crop identities,
labels, confidence, historical engine/preset/performance provenance and hashes
of generated A/B/plot artifacts. Saved source-preset snapshots, effective graphs,
embedded suite documents, reference analysis and decoded source audio are
checked against their saved identities. The original executable, source preset
or original manifest may have changed or disappeared; these paths are historical
metadata and do not invalidate an immutable captured result.

Record subjective choices in a copy of `human-preferences.template.json`, which
names the exact report and artifact hashes. Keep it separate from numeric
reference similarity. A preference does not certify a reference match.

Limits are 32 cases, 16 intervals per case, 120 aggregate selected audio seconds,
4,096 total raw/aligned descriptor windows (conservative preflight estimate), 64 MiB per input WAV/JSON and two million frames per
source WAV. WAVs must be uncompressed PCM, mono/stereo at 8–192 kHz. Cross-rate
pairs are rejected. The tool decodes one pair at a time and caches repeated
interval spectra within that case. It does not allocate app or audio-thread
resources.
