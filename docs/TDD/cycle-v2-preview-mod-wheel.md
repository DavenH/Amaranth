# Cycle V2 Preview Mod Wheel And Keyboard Placement

## Status

Implemented (2026-09-15).

## Problem

The canvas performance keyboard can select and audition a preview note, but it
cannot set the modulation-wheel input used by that audition. The keyboard also
occupies the lower-right utility column, separating the primary audition
control from the canvas centre and competing with the minimap and legend.

## Authoritative Implementations

- `PerformanceKeyboardPanel` owns transient preview transport state and emits
  its note through `MidiEventSink`.
- `MidiControlState` owns MIDI CC normalization. Mod wheel remains ordinary
  MIDI CC 1; the UI does not add another modulation formula or DSP path.
- `CanvasUtilityDock::layout()` owns the keyboard's screen-space rectangle.
  The widget remains presentation-only and does not enter graph state, undo, or
  serialization.
- `AmaranthMidiKeyboard` and JUCE remain authoritative for piano-key geometry,
  hit testing, and drag transitions.

## Design

Add a vertical, non-springing mod wheel to the left of the existing keyboard
controls. Its full column is the hit target. Pointer position maps from CC 1
value 127 at the top to 0 at the bottom; Up/Down provide one-step keyboard
adjustment. The slot fill and a contrasting thumb hairline show the exact
position without enlarging the piano keys.

The panel stores an integer preview mod-wheel value from 0 through 127. A
wheel adjustment emits ordinary CC 1 immediately so a sounding preview can
respond. Starting preview playback emits the stored CC 1 value immediately
before note-on, guaranteeing that spacebar audition uses the selected value
even after renderer or graph lifecycle changes.

The same adjustment also updates the transient presentation snapshot. Preview
audio and direct node previews receive the normalized CC 1 value through their
existing control contexts, then the established preview invalidation path
refreshes downstream compact previews and signal-probe spies. Expanded spy
capture reads the same snapshot value. This is presentation state only: it does
not mutate, serialize, compile, or revise the durable graph.

At normal canvas sizes, `CanvasUtilityDock` centres the complete keyboard panel
at the top margin. The panel widens only enough to accommodate the wheel while
retaining 25-pixel white keys. The top-left status width is capped before the
keyboard. At compact widths where the centred keyboard would overlap the
right-side minimap/legend column, it moves below that column while remaining
horizontally centred and fully contained.

## Complexity And Boundaries

- Pointer movement, value publication, playback start, and layout remain O(1).
- Preview recomputation is bounded by the existing downstream preview product;
  it does not compile, publish an audio plan, or prepare durable resources.
- No graph clone, serialization, compilation, resource preparation, or durable
  mutation occurs during a wheel drag.
- The panel translates a transient value to an existing MIDI CC message only;
  `MidiControlState` and modulation-source evaluation remain unchanged.
- The implementation adds no node-kind branch or compatibility adapter.

## Completion Criteria

- A complete pointer sequence changes the visible mod wheel value and emits
  CC 1 with the matching 7-bit value.
- Preview start emits CC 1 before the selected note-on; stop still emits the
  matching note-off.
- Two successive wheel updates refresh node previews and connected spies with
  the matching normalized CC 1 value without compilation or audio publication.
- Automation exposes and can drag the mod wheel through the real keyboard
  target contract.
- Normal layout centres the panel at the canvas top, preserves 25-pixel white
  keys, and does not overlap status, minimap, or legend.
- Compact layout remains contained and non-overlapping.
- Focused tests, standalone build, automation fixture, screenshot review,
  style checks, and production-diff review pass.

## Implementation Evidence

- `PerformanceKeyboardPanel` owns a compact vertical wheel, publishes ordinary
  CC 1 during adjustment, and republishes the retained value immediately before
  preview note-on. Its focused test covers pointer down/drag, one-step keyboard
  adjustment, controller-before-note ordering, and note-off.
- `CanvasUtilityDock` places the 489-by-140 preferred panel at the top centre.
  At 500 by 300 it resolves a centred 464-by-117 compact panel below the right
  utilities while preserving 25-pixel white keys and non-overlap.
- Seven focused CTest cases pass, including the complete mod-wheel/audition
  sequence and normal/compact geometry. The standalone target builds with
  `--parallel 10`.
- `cycle-v2-agent-preview-transport.json`,
  `cycle-v2-agent-performance-keyboard.json`, the canvas-chrome fixture, and
  the OS-screenshot fixture pass with no failed commands or launch assertions.
  Automation reports CC 1 value 95 after the drag and panel position x=620,
  y=18 on the 1728-pixel canvas.
- The production-size OS capture is
  `/private/tmp/cycle-v2-mod-wheel-after.png`; the focused panel capture is
  `/private/tmp/cycle-v2-preview-transport.png`. Visual review confirms a clear
  top-centred hierarchy, a recognizable non-springing wheel, and no collision
  with status, minimap, or legend.
- `git diff --check` and the hot-loop scalar-math review pass. `clang-tidy` is
  unavailable in this environment. The full 1,065-test CTest run retains 40
  unrelated preset/audio failures recorded in `audio-bugs.md`; all keyboard
  and layout tests pass within the same discovery.
- Wheel changes now enter the presentation snapshot as normalized CC 1 for
  both audio traversal and direct node previews. Invalidation starts only at
  modulation-source configurations that consume the mod wheel (including MIDI
  CC 1 mappings) and propagates through the existing downstream preview graph.
  Those roots are cached when DSP configurations publish, so pointer movement
  does not scan unrelated graph nodes to rediscover them.
- The focused presentation test performs two wheel updates and verifies the
  modulation preview, compact spy, and expanded spy all change, while an
  unrelated modulation branch's process count, compilation count, and audio
  plan revision remain unchanged. The three focused CTest cases pass with 52
  assertions total, and the standalone target builds with `--parallel 10`.
- The real pointer-path preview-transport fixture passes all 20 commands with
  a final wheel value of 95 and no filtered launch-log errors.
