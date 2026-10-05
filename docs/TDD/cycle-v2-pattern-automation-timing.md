# Cycle V2 pattern automation timing

Status: Implemented.

The piano roll stores timed MIDI controller points in `PresetMidiSequence`.
`PerformanceKeyboardPanel` currently interpolates the primary controller and
enqueues its current value from a 60 Hz UI timer. Notes and other controller
points already use the timestamped `MidiEventSink` path. The realtime queue,
renderer, and `ModulationSource::renderAudioBlock` support sample offsets, so
UI timer cadence is the limiting boundary.

Move primary-controller interpolation into a small playback scheduler. It
samples the stored linear curve at 200 Hz, emits only changed 7-bit MIDI values,
and schedules each transition with an absolute timestamp 500 ms ahead. The
existing timer replenishes this bounded lookahead and updates visual progress;
it does not decide when a controller value reaches the audio thread. The
realtime queue remains the scheduling authority and the existing modulation
source applies events at their sample offsets. Stop cancels pending pattern
events through the source generation already used by the transport. A UI stall
shorter than the lookahead does not delay controller delivery; longer stalls
can still postpone replenishment.

The scheduler's work scales with the visible lookahead and is independent of
unrelated graph content. It allocates only on the UI thread. The timer
interpolation state and immediate MIDI send were deleted. The focused sequence
tests pass with 258 assertions, including 5 ms interpolation sampling,
values, absolute timestamps, and transport cancellation. The existing
`ModulationSource` sample-offset render test passes. The standalone Debug app
builds on macOS; `git diff --check` and the architecture audit pass. The
903-line `PerformanceKeyboard.cpp` retains keyboard/transport orchestration
while interpolation now belongs to the 65-line scheduler. No graph mutation
or DSP policy was added to the UI panel.
