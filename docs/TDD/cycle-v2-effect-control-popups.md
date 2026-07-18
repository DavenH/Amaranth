# Cycle 2 Effect Control Popups

## Status

In progress: product behavior is implemented; realtime allocation regression
verification remains open.

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
- Focused semantic tests and a Cycle 2 automation fixture cover editor
  publication, scaled readouts, and downstream audio behavior.
- Processing performs no allocation, parsing, logging, or locking after
  preparation.

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
- `cycle-v2-agent-equalizer-editor.json` verifies popup publication and all ten
  default EQ readouts in the running application.
- Standalone Debug builds both Cycle applications, proving Cycle 1 and Cycle 2
  use the extracted Equalizer core.

Remaining verification defect:

- The existing prepared-graph allocation tests report six allocations per
  demo render and one in the dynamic Envelope graph after introducing explicit
  stereo payload storage. The effect-specific DSP and popup tests pass, but the
  no-allocation completion criterion is not yet satisfied.

Crash regression evidence:

- EQ response previews use the generic runtime trace path, not the Effect2D
  mesh-model renderer. The focused add/open fixture completes without an
  assertion or crash; report:
  `/private/tmp/cycle-v2-eq-crash-regression-report.json`.
