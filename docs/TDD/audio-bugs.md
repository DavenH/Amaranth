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

## Open: Stengah phase-pan swap parity no longer matches the bundled preset

Context:

- The full `CycleV2_tests` run on 2026-08-04 failed
  `TestChainedOscillatorRegionRuntime.cpp:276` in `Stengah phase layer pans
  survive spectral materialization`.
- Swapping the two authored phase-layer pans produced an L2 channel mismatch of
  `1.810257196` rather than the required value below `1.0e-5`.
- The failure reproduces in isolation and is outside the spectral traversal
  column normalization changed for probe previews.

Current status: open; reconcile the bundled Stengah phase-layer state and
spectral materialization parity before restoring this full-suite gate.
