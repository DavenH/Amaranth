# Explained factory exemplars

Four existing factory presets now explain their overall design and selected
components through the normal preset/node explanation actions. Agents read the
same text with `inspectExplanation`. Their synthesis, routing, guides and saved
musical demonstrations are unchanged.

| Example | What to inspect | Controlled demonstration |
| --- | --- | --- |
| [Sat-bass](../cycle-v2/content/presets/sat-bass.cyclegraph) | Per-voice time waveform and amplitude, then global transfer shaping | Change only waveshaper transfer vertex 5 Y to 0.60 |
| [Filter-saw](../cycle-v2/content/presets/filter-saw.cyclegraph) | FFT magnitude mask, preserved FFT phase and volume release | Stretch only post-sustain Envelope phase coordinates by two |
| [Reed](../cycle-v2/content/presets/reed.cyclegraph) | Guide attachment to local waveform phase and additive spectral character | Change only guide 5 Noise to 0.15 |
| [Warmth](../cycle-v2/content/presets/warmth.cyclegraph) | Direct magnitude/phase source, unison, delay then reverb | Disable only delay; downstream reverb remains |

The compact [bank manifest](../cycle-v2/content/exemplars/index.json) names graph,
node, guide and parameter addresses. Notes live only in canonical graph
`rationale`/`description` fields. The manifest's `synthesisSha256` excludes those
two text fields and protects every other graph value, including resources,
previews, patterns and guide assignments.

Musical intent and starting ranges are authored design choices. A `storedRange`
is explicitly a stored normalized parameter range, not dB, seconds or the MIDI
range. Use `inspectSynthesisCapabilities` and `convertSynthesisParameter` for the
canonical physical mapping; do not interpolate mapping examples. Input MIDI
notes, normalized velocity, and CC1 values are named separately in the suite.
Sat-bass plays one octave below input. The range suggestions remain
`starting-hypothesis`; the fixed recipe demonstrates only the exact exercised
settings, registers, dynamics and trajectories.

## Reproduce and inspect the evidence

Inspect the four source identities, selected nodes and declared coverage without
launching an app:

```sh
python3 scripts/render_cycle_v2_exemplars.py --plan
```

Generate immutable baseline/candidate captures and local audition reports:

```sh
python3 -m venv .context/exemplar-venv
.context/exemplar-venv/bin/pip install -r scripts/requirements-auditions.txt
.context/exemplar-venv/bin/python scripts/render_cycle_v2_exemplars.py \
    --output-dir .context/exemplar-evidence
open .context/exemplar-evidence/index.html
```

The canonical coordinator builds/selects the exact app and owns its session.
`--no-build` intentionally uses an already verified exact build. An existing
coordinated workflow can invoke the runner with `--in-session` through
`cycle_automation.py exec`; lifecycle remains with that coordinator. GUI
sessions are sequential. The runner never saves over a source factory preset or
user preset. Choose a new output directory for each run.

The shared suite covers input notes 48, 60 and 72, velocities 0.35 and 0.7, and a
held-note CC1 trajectory 0 -> 64 -> 127. Every capture uses explicit fixed-seed
settings and two repetitions. Each example runs the baseline and one changed
factor: eight captures per version, 64 baseline/variant captures for the bank. One
additional two-repeat negative-control performance per example gives 72 captures
in total. The fourth
performance uses the same input as the middle held note until CC1 changes;
their first 180 ms must match exactly before any response is claimed. The negative control preserves the note, velocity,
seed and event times, changing only the 200/450 ms CC1 targets to unused CC118;
initial CC1=0 remains. CC118 is below the MIDI channel-mode range and must not
appear in any of these graphs' controller references. A recorded
20 ms guard leaves the controller/control-block boundary outside this prefix
proof; the complete raw audio is retained. Full 0–200 ms diagnostics still record
worst sample error, differing sample count and first difference time; this does
not claim a sample-exact controller onset.

Each output example includes the canonical baseline/variant, actual capabilities,
command payloads, exact unrelated-state/undo/redo/reload checks, performance
results and an [audition report](cycle-audition-reports.md). Reports show attack
0–80 ms, body 80–600 ms and release 600–2,200 ms, plus a separate baseline CC1
comparison against the same-time negative control and a held-vs-negative
schedule-drift case: six report cases per example. Descriptor response is marked
demonstrated only where it exceeds that descriptor's measured control drift
plus a 1e-10 numerical floor. No weighted score is formed. Negative captures
must match the baseline executable and effective-graph hashes. Source audio stays at its raw level; optional A/B players use one
whole-crop RMS attenuation gain. There is no implicit alignment, time warp,
per-phase normalization or taste score. The reports stay unpitched because
this bank does not independently establish fixed-pitch monophony across every
unison, nonlinear, guide and effect setting.

The filter-saw release recipe checks the actual marked sustain cube before
editing. Its uniform phase-1 sustain boundary is reviewed against the mature
`EnvelopeMarkerPolicy` and `EnvelopeReleasePolicy`; the host does not invent an
alternate envelope clock or playback rule. Coordinates after that boundary are
stretched while amplitudes, topology and loop/sustain membership remain intact.
Held-note and post-note-off audio provide the behavioral evidence.

Read `results.json` for exact source, engine, performance and report identities,
repeatability and CC1 response metrics. A routed controller is only a structural
fact until its comparison actually differs after the authored controller change. An ineffective route is reported
without claiming a demonstrated response. Guide Noise is a local guide
variation amount, not a calibrated acoustic breath-noise level. Delay bypass
still includes unchanged reverb. These examples teach causal editing choices;
listening remains the way to decide musical preference or instrument likeness.
