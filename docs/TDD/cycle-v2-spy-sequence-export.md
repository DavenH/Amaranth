# Automated Spy sequence export

Status: Complete — melody audio and polyphonic material glints (2026-11-03).

## Contract and ownership

Export opaque, plot-only Cyclogram PNGs at `index / fps < pattern duration`.
The preview reference note and saved Spy release position stay fixed while
pattern MIDI controllers advance and actual sounding voices carry local glints.
This is an offline presentation workflow. Defaults: default-output, 30 fps,
1280x720, first pattern note (preset note for controller-only patterns) and preset
time-surface style. Explicit named styles include Bullion.

Python owns frame iteration, source snapshots, manifests and failure reporting.
Optional `--mp4` runs installed ffmpeg after all PNGs finish, creating
`sequence.mp4` (H.264/libx264, target 8 Mb/s, slow preset, yuv420p, faststart)
at the same exact rational frame rate and image resolution. `--mp4-bitrate`
sets the target in megabits per second and is recorded in the manifest. Pad odd dimensions on the right/bottom to even dimensions.
Keep all PNGs; publish the video from a temporary MP4 only after successful
encoding. Manifest video status/dimensions distinguish encoding failures from a
failed frame, and partial videos are removed on failure or interruption.
PatternPlaybackSchedule/PatternControlScheduler remain the interpolation owners;
exportPatternSchedule retains its existing validation and event-capacity limit.
The coordinator owns isolated exact-PID launch/cleanup. Extend clean exec to use
its existing scoped lifecycle rather than implementing a second launcher.

GraphPresentationModel owns synchronous batched controller refresh, using cached
dependency roots. Single-controller callers delegate to the batch implementation.
SignalProbeDetailState owns shared expanded capture preparation, extracted from
NodeCanvas::openProbeDetail. SpyFrameRenderer coordinates that capture and the
existing NodePreviewRenderer/ScalarSurfaceMaterialEvaluator production renderer.
TimeSurfaceStyles owns all gradient IDs, labels and recipes. An explicit optional
render-profile override selects the material without changing global selection
or preset metadata; normalization and material cache identity use that override.
Automation adapters only decode requests and write PNGs.

## Melody movie extension

One shared controller-driven Cyclogram carries one material glint per actual
active synth voice. Pattern MIDI retains its authored audible pitches; the
key-scale control note is fixed for all voices and for the visualization:
explicit `--note`, otherwise the first scheduled note-on. Controller-only exports
retain their existing preset-note default and remain silent.

Reuse OfflineGraphAudioRenderer/captureAudio for PCM, voice allocation, release,
legato and stealing. Add bounded voice-age/release-age snapshots on its native
sample timeline at the requested video samples. RealtimeGraphRenderer owns event
clocks and fixed control-note override; it never allocates for telemetry. Offline
capture owns retained frame storage. Observe requested samples inside the
existing MIDI event pass and at the block end; never subdivide DSP blocks just
to observe a marker. Release retirement follows the existing blockwise engine. Do not reconstruct
a voice allocator, envelope state machine or DSP in Python.

PreviewTraversal owns mapping normalized held/release age to the shared Spy
axis, including saved note-off and release span. A focused SpyPlaybackGlint image
renderer owns the approved ridge lighting, combining overlapping influence with
a bounded maximum. Use Buffer/VecOps for image reductions and transforms; pixel
loops only unpack/pack image channels. No global colour changes or new image
algorithm in the script. Retire glints when the actual voice retires; global FX
may remain audible afterward.

The host exports `audio.wav` automatically for patterns with notes, records
voice snapshots per frame, and muxes existing audio into `--mp4` as AAC at
320 kb/s. `--cursor none` disables glints; default is material-glint. Optional
`--tail-seconds` extends the existing pattern duration; default zero preserves
current frame counts. Hold final CC values in the tail. Align WAV sample length
to the complete video frame duration and preserve it as an independent artifact.

Acceptance: prove fixed key-scale with different oscillator pitches; native
sample-aligned notes/CCs, overlapping/repeated notes, retirement/stealing/legato;
identical PCM with and without marker observation;
bounded glint overlap and unchanged pixels outside cursor bands; WAV and MP4
stream alignment; existing controller-only exports; and render and inspect a short
polyphonic melody movie and validate its non-silent audio streams. Preserve exact preset state and document history.

## Boundaries and deletion targets

No duplicated scheduler interpolation, DSP, normalization or recipe algorithms.
Remove duplicated expanded-capture preparation from NodeCanvas. No graph/model
copy, serialization, compilation, document/history edit, or audio-plan publication
per frame. Controller refresh scales with changed roots and required downstream
products, independently of unrelated graph content. Image rendering scales with
the requested plot and existing bounded recipe filtering. No realtime allocations.
No new UI controls, live capture or other Spy modes. Audio uses the established
offline renderer; no alternative synthesizer or voice-state simulation.

NodeCanvas and GraphPresentationModel are pre-existing large orchestration roots;
the former becomes smaller through capture extraction, and the latter retains
controller invalidation ownership. New export policy belongs to focused services.

## Acceptance

- [x] CLI exports exact timestamps, consecutive filenames and recoverable manifest.
- [x] Catalog listing and case-insensitive ID/label selection, including Bullion.
- [x] Saved/default style inheritance and explicit override without mutation.
- [x] Shared expanded capture and plot parity for authored/default-output Spies.
- [x] Batch refresh, history/publication stability and unrelated-state cost proof.
- [x] Invalid targets/domains/arguments and output failures report cleanly.
- [x] Two real app sweeps have identical corresponding PNGs and changing content.
- [x] Focused tests, exact-target build, code-map and architecture checks pass.
- [x] Optional ffmpeg MP4 preserves frames, rate/count, and handles odd sizes/failures.
- [x] Melody audio retains actual pitches with fixed key scale and per-voice glints.
- [x] Voice observation preserves PCM; repeated notes, legato and stealing use the synth lifecycle.
- [x] Local glints preserve captured signal data and bound overlapping illumination.
- [x] A real ten-second polyphonic movie includes aligned stereo audio and unchanged sources.

## Evidence

- Exact CycleV2 and CycleV2_tests targets build with `--parallel 10` in
  standalone-debug and tests presets respectively.
- Focused C++ selection `[spy-export],[preview-controller],[surface-program],
  [pattern-schedule]`: 14 cases, 818 assertions. Covers all catalog IDs/labels,
  material parity, note override, saved release timing, disconnected/spectral
  targets, PNG publication failures, document/history/publication stability,
  no graph/mesh copies or serialization, and two changed controller roots with
  1 versus 80 unrelated nodes. Log: `.context/spy-export-cpp-tests-final.log`.
- Host/export/coordinator/session Python suites: 29 tests pass. Include exact
  boundaries, multiple lanes, nonintegral frame rates, failure/interruption
  artifacts, and isolated coordinator arguments. Log:
  `.context/spy-export-mp4-all-tests.log`. MP4 coverage includes worker flag
  forwarding, success/failure/interruption reporting, partial-file cleanup, and
  real ffmpeg/ffprobe verification of fractional rate, frame count and odd sizes.
- Real `--mp4` export: `.context/spy-export-mp4/sequence.mp4`, H.264/yuv420p,
  six frames at 6 fps, one second; 321x201 PNGs retained and padded to 322x202
  for video. Encoding status and dimensions are recorded in its manifest.
- High-quality ten-second Honerism-3/Bullion loop:
  `.context/honerism-bullion-10s-1080p/sequence.mp4`. ffprobe verifies exactly
  300 frames at 30 fps, 10 seconds, 1920x1080 matching all PNGs, and 8.33 Mb/s
  measured bitrate against the 8 Mb/s target (10.4 MB). The saved pattern eases
  CC1 from 0 to 127 and back. Proof: `video-info.json` beside the movie.
  Bitrate default/override forwarding, validation and manifest metadata pass
  all 29 Python tests in `.context/spy-export-hq-all-tests.log`.
- Two isolated honerism-3/Bullion sweeps at 6 fps, 640x400: all six corresponding
  PNGs are byte-identical and all six frames within each sweep differ. Source
  files retain their original hashes. Proof: `.context/spy-export-repeatability.json`;
  outputs `.context/spy-export-bullion-a2` and `.context/spy-export-bullion-b`.
- Real authored probe2/Greyscale export, preset-inherited Bullion export, full
  15-entry catalog listing, and Stengah Spy/B0 export complete. Example:
  `.context/spy-export-stengah-bullion/frame-000001.png`.
- Existing opened/pinned Spy continuity regression passes all 53 commands:
  `.context/spy-export-refresh-regression.json`. Expanded waveform, spectrum,
  root-note and undo/redo behavior continue through the shared capture path.
- Code-map validation, architecture audit and diff whitespace checks pass.
  Runtime export logs contain no Cycle assertion or crash.

Size/responsibility review: NodeCanvas.cpp 3776 -> 3745 lines removes expanded
capture mechanics; NodeCanvas.h 493 -> 496 adds only typed service routing.
CycleV2Automation.cpp 1130 -> 1147 adds command delegation only; its existing
legacy command surface remains outside this slice. GraphPresentationModel.cpp
716 -> 734 retains cached-root invalidation and shared synchronous refresh.
SignalProbeDetailView.cpp 265 -> 301 owns expanded capture/result preparation
alongside its existing detail state. New SpyFrameRenderer (67 lines) owns plot
composition; SpyFrameAutomation (133 lines) owns decoding/catalog/PNG transport.
No duplicated DSP, interpolation, material evaluation or normalization remains.


## Melody extension verification (2026-11-03)

- Exact CycleV2 and CycleV2_tests targets build with `--parallel 10`.
- Focused export/control/material/scheduler/movie filter passes 20 cases and
  18,574 assertions. Voice observations leave PCM unchanged, distinguish
  repeated same-pitch notes, follow legato identity, and expose actual eight-voice
  stealing/retirement. Fixed key scale remains independent of oscillator and
  legato pitches. Glint tests prove local influence and bounded overlap;
  production style/capture tests retain identical underlying content revision.
  Evidence: `.context/spy-melody-focused-tests.log`.
- Host tests pass 30 cases: ten sequence/audio/MP4 tests plus twenty coordinator
  and session cases. A fractional-rate ffmpeg regression catches early audio
  truncation: constrain encoding by full video duration, rather than stopping
  all muxing when the final video frame is encoded.
- Broad `[modulation],[realtime]` plus focused exports: 102/114 cases pass.
  The twelve failures are missing factory fixtures, empty demo/spectral signal
  and telemetry expectations, and bundled modulation routes. These categories
  and cases are already recorded in `audio-bugs.md` (2026-10-24 runtime cursor
  verification and realtime fixtures) and `ui-bugs.md` (modulation bundles).
  They are outside this export slice. Evidence: `.context/spy-melody-cpp-tests.log`.

- Final ten-second Honerism-3/Bullion melody: 300 consecutive 1920x1080 PNGs at
  30 fps, 274 distinct images, seven played pitches and up to three simultaneous
  glints. Reference/key-scale note stays MIDI 60. Video measures 7.71 Mb/s against
  the 8 Mb/s target; AAC is 320 kb/s target. Both streams are exactly 10 seconds.
  Independent WAV is 48 kHz, stereo, 24-bit, 480,000 samples; peak 0.146,
  RMS 0.0505. Source preset/pattern match their original hashes. Production-size
  frames 75 and 220 inspected for local ridge lighting and shared geometry.
  Artifact: `.context/spy-polyphonic-melody-final-1080p/sequence.mp4`.
  Machine verification: `verification.json` beside the movie. No app assertions
  or crash appeared in the isolated export log. Code map, architecture and diff
  checks pass.
- Two isolated 1.5-second polyphonic exports produce byte-identical matching
  PNGs and WAV files while frames change across the automation. Evidence:
  `.context/spy-melody-repeatability.json` and `.context/spy-melody-repeat-a/b`.


Responsibility review: RealtimeGraphRenderer.cpp grows 779 -> 822 lines. It
retains prepared-graph/voice lifecycle, MIDI dispatch, voice mixing and metrics;
new bounded observation runs within that existing event pass and delegates no
rendering or export policy into the engine. Its collaborators remain
VoiceNoteState, MidiControlState, GraphAudioExecutor and playback exchanges.
A broader extraction of event scheduling/mixing remains separate; moving the
observation out would duplicate the actual allocator. OfflineGraphAudioRenderer
227 -> 251 lines owns offline storage and passes caller-owned observation spans;
there is no second allocator or DSP path. AudioProcessTypes.h 290 -> 291 retains
its typed signal/event/control contract. OfflineAudioCaptureAutomation 407 -> 456
lines adds literal validation/encoding only. SpyFrameRenderer 67 -> 79 remains
composition; SpyPlaybackGlint (118 lines) owns one image lighting operation.
Glint influence scales with eight bounded voices and image columns, and pixel
work uses vector operations on only the enclosing affected image span. No
telemetry allocations, model mutation, new document publication or duplicated
preview/material algorithms are introduced.
