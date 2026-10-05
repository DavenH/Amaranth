# Cycle V2 preview stop releases sounding notes

Status: Implemented.

The Space key reaches `PerformanceKeyboardPanel::stopPlayback`. That method
invalidates future pattern events and asks `StandaloneAudioEngine` to release
the pattern source. The current source release sends MIDI All Notes Off. JUCE
represents that as controller 123, and the queue classifies generic controllers
first. The message never reaches the renderer's source-release path. Long
release tails would also remain audible after a natural All Notes Off.

The realtime queue and graph renderer remain the authoritative MIDI scheduler.
Add an explicit All Sound Off event for pattern Stop, mapped to a sample-offset
Reset lifecycle event for only that source. Ordinary note-off and All Notes Off
keep their natural release semantics. The reset event must reach the voice
processor during the current audio block and leave manual keyboard voices
untouched. The existing Space handler calls the same transport toggle as the
play button. A focused renderer test covers a sounding pattern note and a held
manual note; a transport test covers toggle to Stop and source release. Queue
tests cover both MIDI controller variants. All focused tests passed in the
standalone Debug build on macOS.
