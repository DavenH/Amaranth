# UI Bug Notes

## Open: Cycle V2 agent wrapper falls back to a non-GUI launch and aborts

Context:

- A focused hover-help fixture on 2026-08-26 failed to open the built app via
  LaunchServices with `kLSNoExecutableErr`, despite the bundle executable being
  present and executable.
- The wrapper then launched the executable directly; AppKit aborted while
  registering the application on the main thread.
- Repro artifacts: `/private/tmp/cycle-v2-hover-help-logs.txt` and
  `/private/tmp/cycle-v2-hover-help-logs.txt.ips`.

Current status: open; investigate the LaunchServices bundle resolution and do
not use direct executable fallback for GUI automation.

## Open: Envelope native smoke does not restore curve state on undo

Context:

- `python3 scripts/test_cycle_v2_native_edit_smoke.py envelope` now completes
  both finite audio captures, then fails at `envelope_sequence` line 987.
- After the curve-sharpness gesture and undo, the exported Envelope model state
  does not exactly equal the pre-gesture state.
- The failure is downstream of and independent from the linked-stereo audio fix.

Current status: open; compare the restored model fields and determine whether
the gesture commits an incomplete undo snapshot or the fixture includes
non-semantic state in its equality assertion.

## Open: Guide dock launch capture logs existing settings assertions

Context:

- The Guide/Spy dock capture on 2026-08-21 rendered successfully but logged two
  `juce_String.cpp:327` assertions followed by `Settings.cpp:225` and `:226`.
- Repro artifacts: `/tmp/cycle-v2-guide-dock.png` and
  `/tmp/cycle-v2-guide-dock.log`.
- The native OpenGL Guide-preview capture on 2026-08-22 reproduced the same
  settings-write assertions at shifted lines `Settings.cpp:227` and `:228`;
  all fixture assertions and rendering completed successfully. Repro log:
  `/private/tmp/cycle-v2-guide-opengl-final-logs.txt`.

Current status: open; inspect settings-map initialization separately from the
Guide resource UI work.
