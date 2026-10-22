# Expressive saxophone studies

This study uses the existing `saxophone-3.cyclegraph` as one shared synthesis
structure. It tests control response and one manually declared edit. It does
not establish acoustic saxophone fidelity, musical taste, or a complete
expressive instrument, and does not open the roadmap's autonomous-search gate.
Record newly encountered capability gaps in the
[modelling feature queue](../../../../docs/TDD/cycle-v2-preset-modelling-feature-queue.md).
Subsequent [external reference acquisition](REFERENCE-SOURCES.md) is preserved
separately from this experiment's frozen inputs and results.

The follow-up [VCSL calibration](../../../../docs/TDD/cycle-v2-saxophone-reference-calibration.md)
uses twelve pinned CC0 isolated notes: eight objective recordings and four
reserved D3 parents. `vcsl-calibration.json` declares one pitch-envelope ablation
of the same factory structure. It compares source and synthesized pitch traces
and raw/RMS auditions, without changing geometry or inventing source controls.
Run its objective and audit stages with
`scripts/run_cycle_v2_saxophone_calibration.py`; the runbook describes the local
corpus layout.

## Reference anchors and first bridge

The first anchor pair fits the VCSL tenor Non-Vibrato D3 layer-2 and layer-3
recordings. These source layer labels are not calibrated dynamics. Both are
developmental inputs now; the calibration's earlier reserved partition remains
historical. Sounding pitch is approximately 293.66 Hz, standard MIDI 62.

Open [tenor-anchor-layer-2.cyclegraph](../../presets/tenor-anchor-layer-2.cyclegraph)
or [tenor-anchor-layer-3.cyclegraph](../../presets/tenor-anchor-layer-3.cyclegraph)
for a fixed reference model. Their embedded note plays the fitted full contour;
CC2 is inactive. Open [tenor-anchor-bridge.cyclegraph](../../presets/tenor-anchor-bridge.cyclegraph)
for the shared CC2 interpolation and press Play for its 13-second MIDI/controller
phrase. The graph uses independent channel harmonic shapes, cable panning, an
ordinary Add and a shared live volume envelope. It contains no Split/Join nodes
or sample playback assets.

Both raw full-note levels match within 0.14 dB, and settled harmonic-envelope
errors are substantially lower than the earlier expressive preset. This first
fit retains 48 harmonics per channel, nine magnitude time knots and static
settled phases. Geometry simplification, fine pitch fluctuations, breath/noise
and broader register fidelity remain open. See the
[anchor TDD](../../../../docs/TDD/cycle-v2-saxophone-anchor-bridge.md) for metrics
and limits; acoustic resemblance and taste still need listening acceptance.

With the pinned VCSL downloads in the documented local corpus, collect the two
originals through the canonical reference library. Use a fresh output directory
for each collection and fit. Build CycleV2 before passing `--no-build`. Python
measurement/authoring requires `scripts/requirements-auditions.txt`.

```sh
python3 scripts/cycle_reference_study.py --no-build \
  --study cycle-v2/content/studies/expressive-sax/anchor-references.json \
  --output .context/sax-anchor-source-analysis
python3 scripts/cycle_saxophone_anchor_analysis.py --plot \
  --analysis-report .context/sax-anchor-source-analysis/report.json
python3 scripts/create_cycle_v2_saxophone_anchors.py --no-build \
  --analysis .context/sax-anchor-analysis/anchor-analysis.json \
  --output-dir .context/sax-anchor-fit/run
python3 scripts/create_saxophone_reference_audit.py --include-reserved \
  --comparisons .context/sax-anchor-fit/run/comparisons.json
open .context/saxophone-reference-audit/index.html
```

The listening page keeps every original and adds Source/Synth buttons on the
two fitted rows. Search `D3` and select source `VCSL` to find them. The bridge
strip auditions endpoints, intermediate settings, a held sweep, nearby notes
and the embedded phrase. Notes retain hashes, timing/level policy and known
residuals; browser annotations and exports preserve their existing format.
The fit runner records engine/input/script identities, atomic model undo/redo,
reload, raw source/baseline comparisons and a repeated nine-case bridge suite.
Expected-frequency bands on nearby notes are diagnostic, not independent pitch
or acoustic validation. It writes local presets; installing a selected result
in the factory directory is a separate content step.

## Playable articulation preset

Open [expressive-saxophone.cyclegraph](../../presets/expressive-saxophone.cyclegraph)
and press Play in the keyboard to hear its embedded 28-second MIDI/controller
phrase. It starts with isolated controls and continues with connected runs,
held-note returns and combinations across registers. Overlapping notes share
one legato voice per source/channel; separated notes articulate afresh.

| Controller | Purpose |
| --- | --- |
| CC1 | Attack/release character; note-off determines short-note duration. |
| CC2 | Brightness through the existing shared mesh morphs. |
| CC3 | Existing timbral onset/scratch contour. |
| CC4 | Air/rasp from component noise; some guide jitter remains at zero. |
| CC5 | Delayed vibrato depth: neutral at zero, approximately ±30 cents at full. |
| CC11 | Live expression gain, with a 0.35 floor for uninitialised controllers. |
| CC76 | Vibrato rate, with 4.5 and 7.5 Hz endpoints. |
| CC77 | 25 Hz amplitude roughness; acoustic growl remains a listening hypothesis. |

The pitch envelope uses seven cubes in place of the factory's 104. The shared
graph has 26 nodes and 57 cubes, including the added roughness envelope and
output multiplier. Voice Context selects Live envelope morphing and Legato,
with the existing Portamento control enabled at 0.06 seconds. The preset's
explanation and node notes describe the design in the app.

`scripts/create_cycle_v2_saxophone_articulation.py` reproduces the preset through
semantic commands and generates independent-control captures and raw/RMS
auditions. Its technical checks establish playable control response and
continuity; acoustic authenticity and taste require human listening. This
extends the core instrument without changing the frozen pilot/calibration
evidence or reclassifying the consumed VCSL recordings as new holdouts.

The remaining sections describe the original growl pilot.

The plan and performance suites beside this file separate objective cases
from reserved performance combinations before editing. Objective rendering
must finish and freeze its candidate and input identities before the separate
audit renders reserved cases. Reserved results never select or refine the
candidate. They measure technical generalization across synth performances;
there is no independent held-out saxophone recording.

## Existing controls and declared intervention

- CC1 selects the volume envelope's morph position at note-on. Its envelope
  selection is latched; a sustained-note CC1 sweep does not demonstrate live
  envelope morphing.
- CC2 drives live time/magnitude mesh morphs. Endpoint and trajectory renders
  expose its response without assigning unverified saxophone-expression names.
- CC3 stays at zero in every case. Its effects are outside this pilot.

Every performance initializes all three controllers. A verified unused CC118
schedule provides a same-time drift comparison. Nonzero finite repeated audio,
registers, velocities, note durations and control combinations are technical
checks, not observed instrument labels.

The single candidate doubles the phase coordinates of volume-envelope peak
vertices 8–15. This is a slower-peak hypothesis, not a selected improvement.
Topology, amplitudes, curvature, markers, routing, guides, parameters and other
models remain fixed. The workflow verifies the source shape and applies the
edit through the existing revision-checked graph command. It preserves failed
or unhelpful outcomes instead of treating any numerical change as progress.

## Reference coverage and pitch conventions

`references.json` uses the existing labelled-reference-study schema and the
factory recording `../../references/sax-growl.wav`. It is one mono 44.1 kHz,
16-bit recording lasting approximately 3.929 seconds. The Cycle 1 copy is
byte-identical and supplies no additional coverage. The articulation label
"growl" is inferred. Pitch, dynamics, attack, performer, recording conditions
and controller settings are unverified; attribution and licensing provenance
are unspecified in the available recording metadata.

The manifest observes the whole recording. Comparison intervals declared by
the plan select observations without creating new independent recordings or
retracking their pitch. The exported folded analysis is peak-normalized and does not retain source
recording level. Reports retain raw WAV comparison levels and offer a separate
RMS-matched audition, without calibrated acoustic-loudness claims.

Cycle's reference pitch export uses the internal convention 81 = 440 Hz;
audition diagnostics use standard MIDI 69 = 440 Hz. Exported note 57 therefore
corresponds to approximately 110 Hz, or standard MIDI 45. Tracker confidence
does not verify a musical label. This pilot leaves comparison pitch unspecified
and makes no pitched harmonic-match claims.

## Missing evidence

A later expressive study needs independently labelled recordings spanning
register, dynamics, attack styles and controlled expression trajectories, with
independent takes reserved before refinement. Record sounding pitch or measured
frequency, articulation, relative dynamic intent, recording conditions and
source provenance; preserve the distinction between observed and inferred
labels. Growl/no-growl contrasts and combinations with other controls need
their own reference coverage. This pilot's stress tests and reference crop
cannot substitute for that collection.
