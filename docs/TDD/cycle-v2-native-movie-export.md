# Native Spy movie export

Status: Complete (2026-11-11).

Extend the completed script exporter with File > Export Movie. No Cycle1 movie
export exists. Keep the CLI script supported; product export must run without
Python, a checkout, macOS automation sockets, or audio-device startup.

Ownership: PatternPlaybackSchedule expands timed note/CC events once (extract
from PatternScheduleAutomation, which becomes an adapter). SpyMovieExport owns
validated settings, frozen graph/phrase job files, offline audio/voice capture,
exact frame times, PNG/WAV/MP4 writing, status and ffmpeg invocation. Reuse
OfflineGraphAudioRenderer, GraphPresentationModel and SpyFrameRenderer unchanged.
The same executable handles --export-movie-job before normal app initialisation.
Worker state and presentation are process-local; no live document/history edits.
Saved release positions and first-note key-scale reference are preserved.

UI: a nonmodal 620px export window owns the child process and job files; snapshot
current document and resolved linked phrase when Export is pressed. Include gradient,
resolution, fps, bitrate, tail, destination and ffmpeg path. Defaults: 1080p/30fps,
8Mbps, 1s tail, saved gradient. Progress, cancellation, errors, reveal destination.
Use the shared AmaranthLookAndFeel for the window and all controls, including
the numeric stepper buttons. Keep its lifetime scoped to the export window.
Find ffmpeg via PATH and common platform locations; allow explicit executable.
No shell command construction: JUCE ChildProcess receives argument arrays.

Worker checks cancellation between phases/frames and while encoding, kills its
owned encoder on cancel, retains completed frames and failure status. Audio
capture is bounded by the existing 120s limit; cancellation during capture completes
that synchronous phase before stopping. Parent closure requests cancellation.
Validate output directory is new; never replace user files. Bound dimensions,
fps, total frames and duration; reject missing notes rather than silent movies.

Verify portable job parsing and invalid settings, shared scheduler parity,
real native worker PNG/WAV/MP4, dimensions/stream duration, initial CCs and voice
markers, missing ffmpeg, failed writes and cancellation, parent editing isolation.
Build exact app/tests --parallel10, inspect actual export UI, architecture/code
map/diff checks. No algorithm copies, alternate synth or screen capture.

## Completion evidence

- Debug and Release CycleV2 and Debug CycleV2_tests built with --parallel 10.
- Focused movie, scheduler and keyboard tests: 36 cases / 868 assertions.
  Existing script exporter tests: 11 passed.
- Release executable acceptance: a real PNG/WAV/H.264/AAC export with PATH empty,
  spaces in paths, exact dimensions, consecutive frames and duration; rejected
  existing folder, missing encoder, invalid dimensions and blocked parent;
  encoder failure retains frames; frame and encoding cancellation terminate
  owned work without publishing a final movie. All eight cases passed.
- Production UI: export 36 frames while changing and undoing a parameter in the
  parent; the frozen preset is unchanged. A second export cancels through the
  real button, retaining two frames. Snapshot is captured on Export, not when
  opening settings. Actual UI screenshots reviewed at production size.
- Three matching native/script PNGs are byte-identical; WAV PCM is identical.
  This covers saved Icy Hot Raw material, saved release position, initial CC,
  note reference, production voice markers and audio publication.
- Architecture audit, code-map checker and diff whitespace checks passed.

Output ownership uses exclusive directory creation; a failed claim never writes
a manifest into an existing folder. PNG streams close before atomic publication,
so renaming does not depend on Unix open-file semantics. The ffmpeg subprocess
receives an argument array; no shell or Python is needed by product export.

## Responsibility and size review

New worker and window sources remain below 400 and 250 lines respectively.
PatternPlaybackSchedule owns expanded event ordering for both exporters; the
automation module only serializes it. No copied scheduling/rendering algorithms
remain. Main adds only menu routing and early worker dispatch. NodeWorkspace
adds window composition and a read-only snapshot callback; export policy stays
in the new files. NodeCanvas.h already exceeds the header-size trigger; its
single new accessor delegates to the existing document serializer and adds no
new state or policy. Extracting document access is deferred to a broader canvas
ownership change. Existing automation routing and workspace size triggers were
reviewed; their small additions remain adapters to the new boundary.
