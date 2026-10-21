# Expressive saxophone feasibility pilot

This study uses the existing `saxophone-3.cyclegraph` as one shared synthesis
structure. It tests control response and one manually declared edit. It does
not establish acoustic saxophone fidelity, musical taste, or a complete
expressive instrument, and does not open the roadmap's autonomous-search gate.
Record newly encountered capability gaps in the
[modelling feature queue](../../../../docs/TDD/cycle-v2-preset-modelling-feature-queue.md).
Subsequent [external reference acquisition](REFERENCE-SOURCES.md) is preserved
separately from this experiment's frozen inputs and results.

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
