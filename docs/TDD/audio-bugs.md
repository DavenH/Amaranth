# Audio Bug Notes

## Open: Native Envelope edit smoke exports non-finite audio samples

Context:

- The default-graph Envelope native sequence encounters non-finite audio before
  its editor gesture. Automation capture now rejects the buffer explicitly
  instead of serializing samples as JSON `null`.
- Per-node diagnostics identify `reverb` as the first non-finite producer and
  `out` as the downstream propagation point.
- `assert_audio_changed` consequently fails while comparing the initial and
  post-edit 2048-frame captures. The issue reproduces when the Envelope
  sequence runs alone and is independent of the Stengah Pan/cursor work.

Current status: open; diagnose Reverb initialization/rendering in the
`with-spies.cyclegraph` offline capture and retain the audio assertion as a
failing gate rather than accepting or filtering the samples.

## Open: Stengah phase-pan swap parity no longer matches the bundled preset

Context:

- The full `CycleV2_tests` run on 2026-08-04 failed
  `TestChainedOscillatorRegionRuntime.cpp:276` in `Stengah phase layer pans
  survive spectral materialization`.
- Swapping the two authored phase-layer pans produced an L2 channel mismatch of
  `1.810257196` rather than the required value below `1.0e-5`. After restoring
  pitch-log Trimesh sampling on 2026-08-09, the mismatch remains open but is
  reduced to `0.612827003`.
- The failure reproduces in isolation and is outside the spectral traversal
  column normalization changed for probe previews.

Current status: open; reconcile the bundled Stengah phase-layer state and
spectral materialization parity before restoring this full-suite gate.
