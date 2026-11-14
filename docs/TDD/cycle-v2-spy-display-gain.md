# Spy display gain

Status: Complete.

## Contract and owners

Improve visual clarity with +12 dB on magnitude spectrogram Spies and +6.02 dB
on cyclogram Spies. Apply only to copied rendering values in NodePreviewRenderer,
before existing colour/log mapping; retain raw preview/capture amplitudes,
waveforms, phase, mesh editor scales, Gain and actual audio. Shared heatmap
rendering covers compact, expanded, comparison and PNG export.

The user's Baroque Flute working-tree preset has gridPeak 0.641991 and waveformPeak
1.422447 at MIDI 60, zero CCs. SignalProbeWaveformCapture's new stereo mean is
not used by the cyclogram. OscillatorRegionTraversalRenderer separately removes
DC and multiplies cycle columns by 0.5 before unison mixing. Distinct phase/time
sampling prevents assuming exact 2:1 peaks. Do not change domain traversal or
compensate the DSP merely to brighten a view.

## Verification

Extend the existing raw-grid heatmap regression to check an explicit boosted
render against the shared surface renderer, retaining logarithmic axis mapping.
Cover positive/negative time values and exact +12 dB magnitude scaling before
log mapping, unchanged phase/non-Spy rendering and immutable source data.
Capture Baroque Flute before/after with matching controls and note; assert raw
preview peaks/fingerprints unchanged; capture audio without changing its path. Build Debug app/tests and
Release app; run focused render and Spy export coverage, audit, size, code-map
and whitespace checks. Preserve all incoming modified presets.


## Outcome

Both visual boosts apply in the shared heatmap renderer, including the harmonic-
lane spectrogram path. Existing phase mapping, mesh rendering and waveform source
amplitudes are untouched. Baroque Flute's raw grid/waveform statistics and all
node preview fingerprints remain identical before/after. PNG comparison confirms
stronger cyclogram contrast; native expanded spectrogram and waveform captures
are in .context/spy-clarity/. The before-evidence script's second PNG request
was rejected because its path already existed; the earlier successful cyclogram
and snapshot remain the before evidence. No before/after PCM equality is claimed.

The dedicated display-gain, exact spectral-mapping, frequency-lane, gain-difference
and Spy export checks pass. Three older time-contrast cases fail identically
with the exact unchanged HEAD renderer, independently rebuilt and recorded in
ui-bugs.md; they assume different default material/normalization behavior.

Debug app/tests and Release app build. Architecture audit, code-map and whitespace
checks pass. NodePreviewRenderer grows 1025->1034 lines: the existing presentation
mapper owns display gain and now also owns harmonic-lane value mapping, removing
the alternate mapping branch from image orchestration. NodeCanvasAutomationInspector
grows 1362->1365 lines only to report raw grid peak in its existing reduction;
no DSP, graph-edit, allocation or preparation policy is added to either owner.
