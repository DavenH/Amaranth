# Cycle 2 Effect Control Popups

## Status

Implemented.

## Authoritative Implementations

- Reverb parameter and stereo mixing semantics: `cycle/src/Audio/Effects/Reverb.cpp`.
- Delay timing, spin, feedback, wet, and stereo-pan semantics:
  `lib/src/Audio/CycleDsp/CycleDelay.cpp` as used by Cycle 1.
- Equalizer mapping, filter topology, smoothing, and response semantics:
  `cycle/src/Audio/Effects/Equalizer.cpp`.
- Expanded-editor hosting, transactions, and invalidation:
  `cycle-v2/src/UI/NodeEditorHost.*`.

Cycle 2 may translate graph parameters, component events, immutable
configuration ownership, and channel-planar payloads at these boundaries. It
must not copy the effects' algorithms into popup components or canvas code.

## Goal

Provide clean expanded editors for Reverb, Delay, and Equalizer whose
normalized controls display meaningful domain values and drive the shared
effect DSP. Add Equalizer as a complete graph node and make Reverb Width and
Delay spin panning operate on real stereo payloads.

## Design

- Shared parameter mapping functions are the single source for DSP conversion,
  display formatting, defaults, and tests.
- Signal payloads own explicit planar channels. Channel layout declares how
  those planes are routed; it is not a substitute for channel data.
- Equalizer filter design and execution are application-neutral shared DSP.
  Cycle 1 and Cycle 2 retain their own UI, publication, and lifecycle adapters.
- Effect editors are normal JUCE child components registered through the node
  editor host. Generic editor commands own parameter gestures and compound
  undo; the canvas has no effect parameter ids.
- The Equalizer response is computed by the shared filter design and consumed
  by both compact and expanded renderers.

## Completion Criteria

- Reverb, Delay, and Equalizer expose every Cycle 1 control with corrected
  seconds, beats, iteration, percent, dB, and Hz/kHz readouts.
- Reverb Width and Delay spin produce Cycle 1-equivalent stereo output.
- Equalizer exists in the palette, serializes, previews its response, and uses
  the shared Cycle 1 filter core for audio.
- A continuous drag creates one undo entry and updates DSP and previews while
  dragging; enable/reset actions are independently undoable.
- Stereo split/join and blockwise unary/binary routing preserve explicit
  channel planes without an internal legacy compatibility path. Diagnostic
  traversal grids remain preview data and are outside the realtime stereo
  contract.
- Focused semantic tests and Cycle 2 automation fixtures cover editor
  publication, scaled readouts, and downstream audio behavior.
- Processing performs no allocation, parsing, logging, or locking after
  preparation.
- Double-click opens every hosted effect editor regardless of whether its
  compact preview is runtime-backed or qualitative.
- Expanded EQ response rendering samples the shared filter response at display
  resolution rather than enlarging the compact preview trace.
- Reverb and Delay compact previews communicate their parameterized decay,
  feedback, timing, and stereo character and update with parameter edits.
- Reverb compact and expanded previews display a cached spectrogram produced
  from an actual Dirac convolution through the prepared stereo kernel and wet,
  width, damping, high-pass, and size configuration. Analysis must remain off
  the audio and paint paths.

## Deletion Targets And Negative Boundaries

- Delete the Cycle 2 mono-only Reverb and Delay state assumptions.
- Do not add effect-kind branches to `NodeCanvas`.
- Do not retain duplicate Equalizer mapping or filter-design formulas in UI or
  Cycle 2 runtime code.
- Do not treat the current Cycle 2 graph format as a public migration contract;
  update repository fixtures directly.

## Verification

- Shared mapping and Equalizer core tests cover inversion, defaults, channel
  isolation, and configured response.
- Cycle 2 runtime tests cover EQ processing, Reverb stereo width, and split
  semantics.
- Reverb, Delay, and Equalizer editor fixtures verify scaled readouts,
  parameter publication, graph save/reload persistence, and crash-free popup
  hosting in the running application.
- OS-level screenshots verify all three OpenGL-backed popup layouts. The EQ
  fixture also verifies its compact and expanded response displays.
- Standalone Debug builds both Cycle applications, proving Cycle 1 and Cycle 2
  use the extracted Equalizer core.
- Prepared-graph and dynamic Envelope allocation guards pass with zero
  realtime allocations. Payload grid clearing preserves prepared vector
  capacity for subsequent blocks.
- Continuous effect gestures produce one undo transaction. Discrete Boolean,
  keyboard, wheel, and double-click-reset changes publish independently and
  remain independently undoable.
- Reverb preview analysis convolves a one-sample Dirac through the prepared
  stereo kernels with `ConvReverb`, then caches a windowed FFT magnitude grid
  for compact and expanded rendering. The focused runtime test verifies the
  spectral grid contract; the standalone fixture verifies editor opening,
  compilation, and crash-free rendering. Final screenshot and report:
  `/private/tmp/cycle-v2-reverb-spectrogram-2.png` and
  `/private/tmp/reverb-spectrogram-report-2.json`.

Crash regression evidence:

- EQ response previews use the generic runtime trace path, not the Effect2D
  mesh-model renderer. The focused add/open fixture completes without an
  assertion or crash; report:
  `/private/tmp/cycle-v2-eq-crash-regression-report.json`.

Final runtime artifacts:

- `/private/tmp/cycle-v2-reverb-editor-final-report.json`
- `/private/tmp/cycle-v2-delay-editor-final-report.json`
- `/private/tmp/cycle-v2-equalizer-editor-final-report.json`
- `/private/tmp/cycle-v2-reverb-editor-os.png`
- `/private/tmp/cycle-v2-delay-editor-os.png`
- `/private/tmp/cycle-v2-equalizer-editor-os.png`

The full test discovery run passed 496 of 498 tests. The two repeatable failures
are unrelated existing expectations: `GuideCurveOffsetSeeds` expects vertical
seed `4` but receives `16`, and the rich mesh-view test expects width `1080`
while the registered presentation scale produces `972`.

Preview-quality follow-up evidence:

- Fresh-process pointer fixtures double-click the compact Reverb and Delay
  nodes and assert their expanded editor IDs. Both complete without failed
  commands or filtered-log errors.
- `/private/tmp/cycle-v2-effect-previews-final.png` shows parameterized Reverb
  decay and Delay echo/stereo previews alongside the EQ response preview.
- `/private/tmp/cycle-v2-equalizer-quality-final.png` verifies the expanded EQ
  response is sampled from the shared filter core at display resolution.
- `/private/tmp/cycle-v2-delay-stereo-time.png` verifies the compact and
  expanded Delay views share a left/right stereo axis, horizontal time axis,
  parameterized ping spacing, pan position, and feedback decay.
- `/private/tmp/cycle-v2-delay-dsp-accurate-final.png` verifies beat-grid
  alignment, a full sine-pan revolution, wet-scaled ping radii, feedback decay,
  and unclamped viewport clipping. Delay time dragging snaps through the shared
  inverse mapping to exact integer beats, while Spin Length reports the full
  pan-LFO cycle duration in beats.
- `/private/tmp/cycle-v2-delay-tall-symmetric.png` verifies the final expanded
  Delay layout with a taller plot, centered stereo axis, equal vertical plot
  margins, and an in-plot time label.
- The follow-up discovery run passed 496 of 499 tests. The concurrent Envelope
  exchange failure passed immediately on isolated rerun; the two repeatable
  unrelated failures remain the seed-array and mesh-scale expectations above.
