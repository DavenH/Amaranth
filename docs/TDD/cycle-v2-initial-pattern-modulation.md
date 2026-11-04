# Initial pattern controller timing

Status: Complete (2026-11-04).

PerformanceKeyboardPanel owns transport startup, PatternControlScheduler owns
interpolation, and RealtimeGraphRenderer consumes events by timestamp then queue
sequence. Initial automation is already queued before notes, but live wheel seeds
were stamped with the later enqueue wall clock. They could overwrite the first
score CC after note-on, lasting until the next changed automation value.

Stamp transport wheel seeds at the requested playback start, before the existing
scheduler queues its initial values and notes. Keep manual wheel gestures on the
normal immediate path. Preserve seek origins, fixed-key audition, cancellation,
source ownership and audio publication. No DSP/control-frequency workaround,
preroll, interpolation copy or audio-thread allocation is needed.

Regression: mapped CC1/73 have stale live values and nonzero initial automation;
require seed timestamps equal the scheduled origin and score values before the
first note. Check seeking and existing MIDI/audio scheduler tests. Build exact
tests/app with --parallel10, audit architecture and code map, then use the fixed release app for the separately requested movie exports.

PerformanceKeyboard.cpp remains a cohesive keyboard/transport orchestrator;
startup is the sole policy site. Existing event ordering and schedulers are reused.


Verification: the added regression fails before the fix because wheel seeds use
immediate delivery (recorded timestamp -1), and passes after both mapped seeds
share the requested origin. Focused transport/scheduler/seek/control run:38 cases,
874 assertions pass. Exact CycleV2_tests and standalone-release CycleV2 builds
use --parallel10. Architecture audit, code map and diff checks pass. Panel size
1048->1057 lines; the startup ordering decision stays with transport and neither
MIDI interpolation nor rendering changes. Broader unrelated graph bundle fixture
failures are recorded in ui-bugs.md. Export inputs are frozen per job and checked
against their linked pattern; the first score CC is already correctly ordered in
the offline exporter, so no render-only timing workaround was added.
