# Audio Bug Notes

## Open: Native Envelope edit smoke exports non-finite audio samples

Context:

- The default-graph Envelope native sequence completes its editor gesture and
  causal-update assertions, then `captureAudio` serializes at least one sample
  as JSON `null`, indicating a non-finite value.
- `assert_audio_changed` consequently fails while comparing the initial and
  post-edit 2048-frame captures. The issue reproduces when the Envelope
  sequence runs alone and is independent of the Stengah Pan/cursor work.

Current status: open; locate the first non-finite producer in the default graph
after the live Envelope mesh edit and retain the audio assertion as a failing
gate rather than accepting or filtering the samples.

## Open: Stengah magnitude pan drift breaks spectral pan invariants

Context:

- The Cycle V2 suite run on 2026-08-04 failed the Stengah spectral pan-swap
  invariant and its serializer expectation.
- `magnitudeLayer1Process.pan` is currently `0.75833` in the preset while the
  tests and phase-channel swap invariant require the authored neutral value
  `0.5`. The unchanged asymmetric magnitude channel prevents left/right phase
  swaps from producing swapped output frames.
- Repro: `CycleV2_tests "Stengah phase layer pans survive spectral
  materialization"` and `TestGraphSerializer.cpp:677`.

Current status: open; resolve whether the preset pan edit is intentional, then
update the preset or redesign the invariant around isolated phase material.
