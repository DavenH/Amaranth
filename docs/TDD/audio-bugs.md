# Audio Bug Notes

## Resolved: Native Envelope edit smoke exported non-finite audio samples

Context:

- The default-graph Envelope native sequence encounters non-finite audio before
  its editor gesture. Automation capture now rejects the buffer explicitly
  instead of serializing samples as JSON `null`.
- The original diagnostics inspected only the primary channel and therefore
  misidentified `reverb` as the producer when its stereo mix propagated a
  non-finite secondary channel.
- `assert_audio_changed` consequently failed while comparing the initial and
  post-edit 2048-frame captures. The issue reproduces when the Envelope
  sequence runs alone and is independent of the Stengah Pan/cursor work.

Resolution:

- Resolved linked Trimesh outputs now initialize both concrete stereo channels.
- FFT now expands a linked input across both channels when its outputs resolve
  to a stereo pair, including traversal grids.
- Automation finite checks now inspect both channels. The native Envelope
  sequence passes both audio captures and proceeds to its later UI assertions.

Current status: resolved on 2026-08-16.

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

## Addressed: Curve FX processor tests omitted required model state

Context:

- The complete `CycleV2_tests "[cycle-v2]"` run on 2026-08-21 reports eight
  failures at `TestNodeAudioProcessor.cpp:206`: the Waveshaper and IR tests
  call `NodeDspConfigurationFactory::create` with no typed model and receive
  `nullptr`.
- The failures cover the existing Waveshaper/IR processor tests, not Guide
  resources; focused Guide graph tests and the migrated Baroque Flute guide
  runtime test pass.

Current status: addressed on 2026-08-21 by supplying canonical typed
Waveshaper/IR models in the shared fixture helper; the full Cycle V2 suite now
passes.
