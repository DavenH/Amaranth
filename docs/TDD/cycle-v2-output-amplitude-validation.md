# Output waveform amplitude validation

Status: investigation complete; display/output level mismatch remains open.

## Question and scope

Do factory presets whose output waveform extends past the displayed +/-1 bounds
actually reach sample amplitude 1 (0 dBFS)? Measure saved working-tree presets,
without changing their gain or saving them. Preserve incoming preset edits.

## Method

Build the exact Debug CycleV2 target. Run an isolated coordinator instance and
load each preset, select MIDI 60 and zero controllers, then select the default
output Spy's continuous waveform. Inspect its actual sample peak and count of
samples beyond +/-1 through additive snapshot diagnostics. The waveform source
is unnormalized; its default camera displays +/-1. Values outside that range
are visually cropped, rather than proving clipped audio samples.

Render a single full-velocity note using the existing captureAudio owner at
44.1 kHz, 256-frame blocks, stereo, seed 1729, one-second voice duration, with
note-off at the preview's note-off time and a full preview release tail. Preserve
all preset gain/FX settings and the production 0.125 output conditioning gain.
Repeat with only the final conditioning gain overridden to 1 for diagnosis;
this second capture still has the final hard limiter, so its 1.000 peak does not
measure the unlimited graph peak. The override is isolated offline state.

Separately capture the actual audio-device callback for 1.2 seconds after a
full-velocity MIDI 60 note-on, retaining normal preset voice timing. This is a
separate live check, not a sample-identical comparison with the one-second
preview audition. Callback capture can miss the first attack block.

## Results

All ten tested waveforms contain samples above 1; none of their normal-output
single-note renders reaches 1. The first five presets include the user's
incoming working-tree edits; these results describe those exact files.

| Preset | Displayed waveform peak | Actual audio peak | Actual dBFS | Conditioning gain 1 peak |
| --- | ---: | ---: | ---: | ---: |
| bright-lead-2 | 3.079 | 0.197 | -14.1 | 1.000 |
| bright-lead-3 | 2.232 | 0.255 | -11.9 | 1.000 |
| bright-lead-4 | 4.896 | 0.327 | -9.7 | 1.000 |
| astral-3 | 2.036 | 0.160 | -15.9 | 1.000 |
| trombone | 1.134 | 0.074 | -22.6 | 0.593 |
| fuzz-square | 3.122 | 0.195 | -14.2 | 1.000 |
| thick-square | 2.345 | 0.154 | -16.3 | 1.000 |
| sitar-pad | 2.801 | 0.194 | -14.2 | 1.000 |
| subbass | 4.344 | 0.272 | -11.3 | 1.000 |
| filter-saw-2 | 2.218 | 0.164 | -15.7 | 1.000 |

Live callback peaks: Subbass 0.262, Fuzz Square 0.196, Bright Lead 4 0.302.
No live capture reaches unity either.

## Explanation and ownership

- RealtimeGraphRenderer applies a fixed outputGain of 0.125 (-18.06 dB), then
  clips actual device channels to +/-1. OfflineGraphAudioRenderer uses the same
  renderer and default gain. The canvas Gain node is a separate stage.
- SignalProbeWaveformCapture observes graph-node output before that conditioning
  and adds stereo L+R without halving. Identical channels therefore display
  twice a single channel's amplitude (+6.02 dB).
- DefaultOutputProbeResolver taps before terminal Reverb, Delay and Equalizer;
  it is a pre-wet monitor, not the final device signal. These effects, audition
  timing and stereo differences account for nonconstant ratios in other presets.
- ReferenceWaveformPreview's signal coverage uses +/-1 as its default viewport;
  it does not normalize this signal source. This is display-range overflow,
  not evidence of final output clipping.

Fuzz Square and Subbass show essentially the exact combined factor of sixteen:
3.122 / 0.195 and 4.344 / 0.272. Simply removing headroom is not a safe correction:
the gain-1 diagnostic hits the final limiter in nine of ten presets.

The display should distinguish a pre-wet graph signal from calibrated final
output amplitude. Choosing that presentation/signal contract is follow-up work;
no production audio or gain behavior changes in this investigation.

## Evidence and review

Artifacts: .context/output-amplitude/results.json, live-results.json, WAVs,
per-preset snapshots and waveform screenshots. Read-only reproducible drivers:
.context/output-amplitude/probe.py and live.py; run through
scripts/cycle_automation.py exec --product cycle-v2 --clean --no-build.

Only production change: NodeCanvasAutomationInspector adds waveformPeak and
waveformOverUnitySamples alongside existing probe diagnostics. This measures
actual retained preview samples, not a reconstructed approximation. Debug build,
actual-app measurement, whitespace and architecture audit complete. No new unit
test suite: these diagnostic fields were exercised directly by the investigation.

Size review: NodeCanvasAutomationInspector grows 1351->1362 lines. It remains the
existing snapshot encoder; the loop is diagnostic-only and is neither audio-thread
work nor a new DSP policy. Extracting this small reduction would not remove an
owner or responsibility; no domain behavior moves into the encoder.
