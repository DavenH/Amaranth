# UI Bug Notes

## Fixed: Canvas overlay faded while dragging nodes

Context:

- Node drags repainted the JUCE node/cable overlay without explicitly updating
  the OpenGL canvas underlay. Live composition could therefore present the
  whole overlay at greatly reduced opacity until mouse-up caused another GL
  frame.
- Node movement now requests the GL underlay frame alongside its JUCE repaint.
  A native held-drag capture preserved fully opaque node shells and cable cores;
  the measured gesture delivered 14 GL renders for 8 JUCE paints, with GL
  renders averaging 0.59 ms.
- The native authoring sequence now asserts this synchronization contract while
  the drag is still held.

Current status: fixed on 2026-08-29.

## Open: Palette icon registry asserts during Trimesh performance session

Context:

- The Cycle V2 Trimesh/Spy external performance session on 2026-08-27 rendered
  and completed its edit sequence, but logged
  `NodePaletteEntryIconRenderer.cpp:27` because an icon source name did not
  resolve through `NodeDefinitionRegistry`.
- The same session subsequently logged the already-observed
  `juce_String.cpp:327` assertions. The node-layer cache does not alter palette
  registration or icon lookup.
- Repro log: `/private/tmp/cycle-v2-node-sprite-cache.log`.

Current status: open; identify the unmatched `NodeIconData` source name and
align it with the authoritative node definition id.

## Open: Canvas edge hover test returns unstable help text

Context:

- The full `standalone-debug` CTest run on 2026-08-26 failed test 415,
  `Node canvas hit routing preserves action edge and palette placement semantics`,
  at `TestNodeCanvasHitRouter.cpp:66`: `edgeHelp` did not start with
  `Time signal from`.
- The focused rerun failed identically and also logged a
  `juce_String.cpp:327` assertion. The canvas performance instrumentation does
  not modify hit routing, scene edge construction, or hover-text generation.
- Repro output is in
  `build/standalone-debug/Testing/Temporary/LastTest.log`.

Current status: open; inspect hover-text String lifetime/initialization and the
scene edge selected at the cable midpoint.

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
