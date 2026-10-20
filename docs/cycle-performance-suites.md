# Cycle V2 Performance Suites

Render saved presets across controlled performances without editor gestures:

```sh
python3 scripts/render_cycle_v2_performance_suite.py \
    scripts/fixtures/cycle-v2-performance-suite.json \
    --preset cycle-v2/content/presets/old/filter-saw.cyclegraph \
    --output-dir .context/filter-saw-performance
```

The runner delegates building, exact-instance selection and exclusive session
ownership to `cycle_automation.py exec`. `--no-build` uses the coordinator's
existing built app. GUI sessions remain sequential. `--plan` prints a validated,
expanded schedule without launching an app or writing output. `--repeat 2`
overrides the manifest's repetition count.

For an already coordinated workflow, invoke the runner with `--in-session`
inside `cycle_automation.py exec`. It uses that command's environment and the
canonical shared session client. There is no raw socket or PID selector.

## Manifest

Paths to shared library patterns resolve relative to the manifest. Rendering
never edits a pattern or replaces a preset's saved demonstration sequence.

```json
{
  "schema": "cycle-performance-suite.v1",
  "id": "lead-expression",
  "repetitions": 2,
  "render": { "randomSeed": 1729, "sampleRate": 44100 },
  "budget": { "maxRenders": 8, "maxAudioSeconds": 30, "maxWallSeconds": 120 },
  "cases": [
    {
      "id": "short-soft",
      "labels": { "articulation": "short", "dynamic": "soft" },
      "note": 60,
      "velocity": 0.3,
      "noteDurationMs": 200,
      "durationMs": 800
    },
    {
      "id": "brightness-motion",
      "durationMs": 1000,
      "render": { "sampleRate": 48000, "blockSize": 127 },
      "events": [
        { "type": "controller", "timeMs": 0, "controller": 74, "value": 0 },
        { "type": "noteOn", "timeMs": 0, "note": 60, "velocity": 0.8 },
        { "type": "controller", "timeMs": 300, "controller": 74, "value": 127 },
        { "type": "channelPressure", "timeMs": 400, "value": 90 },
        { "type": "noteOff", "timeMs": 700, "note": 60 }
      ]
    }
  ]
}
```

Each case chooses exactly one performance form:

- `note`, `noteDurationMs`, optional normalized `velocity` (default 0.8) and
  `channel` (default 1). `durationMs` includes the release tail.
- `events` and `durationMs`, using `noteOn`, `noteOff`, `controller`,
  `channelPressure` or `allNotesOff`. Times are milliseconds; each event can set
  its channel. Events are stably sorted by time, preserving authored equal-time
  order. Velocity is in `(0, 1]`, pressure and controller values in `[0, 127]`.
  Empty events deliberately render silence. Pitch-wheel events are not supported
  by the existing Cycle V2 scheduled capture and are rejected.
- `pattern`: a `.cyclepattern` file from the shared factory or user library,
  plus optional `tailMs` (default 500) and `channel`. The library duration plus
  tail determines the render duration. Integer MIDI velocities are normalized
  once by the app. `exportPatternSchedule` expands controller ramps through the
  existing `PatternControlScheduler` (5 ms steps, changed MIDI values only).
  Note construction and note-off/note-on ordering share `PatternPlaybackSchedule`
  with musical playback. Exported ties put note-off, control, then note-on events
  in that order. Python retains the returned schedule without approximating it.

For an explicit event trajectory, supply bounded timed events along the desired path. Labels are
optional string-valued descriptions, not claims that a preset responds to that
CC or reproduces that articulation. Rendering the same data does not establish
timbral fidelity or musical quality.

Top-level `render` settings apply to all cases; a case's `render` overrides them.
Every request explicitly supplies the following defaults:

| Field | Default | Accepted values |
| --- | --- | --- |
| `sampleRate` | 44100 | Integer 8000–192000 Hz |
| `blockSize` | 512 | Integer 16–8192 samples |
| `channels` | 2 | 1 or 2 |
| `randomSeed` | 1729 | Signed 64-bit integer |
| `voiceDurationSeconds` | 7 | 0.001–120 seconds of normalized voice traversal |
| `outputGain` | 0.125 | 0–16 |
| `controlNoteOffset` | 0 | Integer −127–127 |
| `ratePolicy` | `native` | `native` or `legacyInternal44100` |

`voiceDurationSeconds` controls the existing renderer's voice-time traversal;
it does not replace MIDI note duration or the recording duration. Current
Envelope morph remains latched at note-on, while Trimesh controls can evolve
during a note. A seed establishes a reproducible request; repeat verification
still checks that the selected preset actually renders deterministically.

Cases are limited to 60 seconds and 2048 scheduled events, matching scheduled
capture bounds. Patterns longer than this are rejected rather than cropped.
Events must precede the recording end; reserve a tail for note-off and final
pattern controller points. There are at most 128 cases and 16 repetitions.

The default budget permits 64 renders, 120 total audio seconds and 300 wall
seconds. Explicit budgets may allow up to 1024 renders, 3600 audio seconds and
3600 wall seconds. Work exceeding counts or audio duration fails before the app
is contacted. The wall budget starts when coordinated rendering begins and bounds
new dispatch and client waits. The existing protocol cannot cancel an in-flight
capture; a timeout stops further work but does not terminate the app's command.

## Results and repeatability

Output must be a new directory. It contains the exact source preset snapshot,
the effective graph exported by the app, per-case WAVs, `commands.jsonl` and an
incrementally written `results.json`. Embedded preset audio resources travel
through the existing graph serializer. The engine record identifies the actual
app process and executable hash, plus the coordinator's run artifacts.

`--plan` retains source pattern documents and planned duration; pattern schedule
validation and its final performance hash require the app's canonical codec and
scheduler. Oversized expanded schedules fail before any audio capture.

Each normalized performance has a SHA-256 over all rendering settings and
scheduled events. Descriptive labels, output paths and repetition numbers do not
affect this fingerprint. The manifest also retains source suite/pattern hashes
and documents, the source preset byte hash, and the effective graph content hash.

Repetitions are interleaved across cases. Each `captureAudio` call builds fresh
renderer, MIDI, voice and effect state through `OfflineGraphAudioRenderer`; a
previous case's held CC, pressure, notes or effect tail cannot be inherited.
Within a case, repeated and overlapping notes share that case's renderer as
authored. Equal PCM hashes are required for repeated performances. A single
repetition reports `repeatabilityVerified: false` even when rendering succeeds.

Every WAV is checked for the requested rate, channel/frame count, 24-bit PCM and
complete payload. WAV and PCM hashes are recorded separately. A final exported
graph must match the initial effective graph. Failed commands, changed graph
content, mismatched repetitions or exhausted budgets leave `ok: false`, an error,
the completed artifacts and command diagnostics. The runner stops on first
failure. Peak/RMS metrics come from the existing capture command; perceptual
comparison and subjective review belong to downstream tools.
