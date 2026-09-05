# Cycle V2 Performance Keyboard Proportions

Status: Implemented

The follow-up side-chevron geometry in
`cycle-v2-live-output-meter-and-keyboard-octave-controls.md` supersedes this
slice's panel geometry while retaining its 25-pixel white-key minimum and
recognizable-proportion intent.

## Objective

Make the fixed performance keyboard read immediately as a piano keyboard while
keeping its existing one-octave range, live MIDI behavior, and lower-right dock
ownership. The current keys are easy to acquire but so wide and shallow that
the familiar object is visibly distorted.

## Current Failure

At the normal 1728-by-962 workspace, the 420-by-126 panel gives the keyboard an
active area of 408 by 86 pixels. Eight white keys are therefore 51 by 86 pixels,
or only 1.69 times as tall as they are wide. The black keys inherit the same
ratio at 35.7 by 60.2 pixels. Stretching every key to fill the panel created a
large global control without adding notes, precision, or information.

Cycle 1 is the mature visual and interaction reference. Its
`MidiKeyboard` uses the shared `AmaranthMidiKeyboard` geometry with a 12-pixel
white-key width in an 82-pixel key area, a 6.83 height-to-width ratio. Cycle V2
already reuses that authoritative component for key ordering, black-key
placement, overlap, hit testing, drag transitions, note state, and painting;
this slice changes only the containing panel budget and the resulting shared
key scale.

## Geometry Contract

The normal utility dock allocates a 212-by-150 panel:

- six-pixel panel insets leave 200 pixels for the key row;
- eight white keys are 25 pixels wide, preserving a practical pointer target;
- the existing header and row spacing leave a 110-pixel key height;
- white and black keys therefore share a 4.4 height-to-width ratio;
- the panel occupies about 42% less area than the current 420-by-126 panel,
  despite being taller, because unused horizontal stretch is removed;
- C3 through C4, octave controls, labels, black-key priority, and all pointer
  velocity semantics remain unchanged.

The 4.4 ratio deliberately remains wider than Cycle 1's 6.83 reference. A
12-pixel key is appropriate in Cycle 1's wide multi-octave strip but too narrow
as a directly playable one-octave floating control. Twenty-five pixels is the
minimum normal white-key width for reliable mouse acquisition in this design.

At constrained window heights, the keyboard yields height before collapsing
the cable legend below 30 pixels. The compact 500-by-300 layout keeps the
212-pixel width and a 126-pixel panel height, producing an 86-by-25 white key
with a 3.44 ratio. This is the documented compact adaptation, not a second
preferred geometry.

## Interaction Contracts

The four UI contracts remain separate:

- **Visual footprint:** 25-by-110 preferred white keys and proportional black
  keys inside a 212-by-150 panel.
- **Hit target:** the mature JUCE key rectangles remain the real event targets;
  no overlapping invisible key targets are introduced.
- **Manipulation mapping:** vertical pointer position continues to determine
  velocity and drag transitions continue through `MidiKeyboardComponent`.
- **Value indication:** pressed, hovered, and octave-labelled states continue
  through the existing shared painter and exact MIDI state.

## Architecture And Boundaries

- `AmaranthMidiKeyboard` remains authoritative for the familiar object and
  complete gesture. Do not copy key geometry or interaction into Cycle V2.
- `CanvasUtilityDock` remains authoritative for screen-space placement and now
  owns the preferred panel budget and the 30-pixel compact legend floor.
- `PerformanceKeyboard::resized()` continues deriving one shared key width
  from its actual content bounds; it does not independently stretch black and
  white keys.
- Do not change graph content, MIDI routing, audio-device ownership, voice
  allocation, serialization, undo, or expanded-editor occlusion.
- The fixed dimensions recorded by
  `cycle-v2-performance-keyboard-and-realtime-audio.md` and
  `cycle-v2-canvas-utility-dock.md` are superseded only for visual geometry;
  their ownership, realtime, and interaction contracts remain authoritative.

## Verification

- Layout tests assert the 212-by-150 normal panel, right alignment, dock
  containment, non-overlap, and the 30-pixel legend floor at 500 by 300.
- Component tests assert preferred white-key width and height, the 4.4 ratio,
  proportional black keys, endpoint containment, and the compact adaptation.
- Existing note-on, drag-to-key, note-off, octave, audio-output, pan/zoom,
  editor-occlusion, and note-release sequences remain green.
- Automation exports panel and key geometry so the focused fixture can assert
  the production bounds rather than merely compare a screenshot.
- Native before/after captures use the same workspace size and appearance.

## Completion Criteria

- Normal white and black keys have a 4.25--4.6 height-to-width ratio and white
  keys remain at least 25 pixels wide.
- The keyboard remains exactly one octave from C through C and retains the
  complete live MIDI gesture.
- No utility overlap or new small-window collapse is introduced.
- Production-size evidence reads as a compact piano keyboard rather than a row
  of broad pads.
- Focused tests, the standalone build, automation fixture, style checks, and
  production-diff review pass; the full Cycle V2 suite introduces no new
  failure.

## Implementation Evidence

- `CanvasUtilityDock` now owns the 212-by-150 preferred keyboard panel and the
  30-pixel compact legend floor. Its 500-by-300 layout resolves to a
  212-by-126 keyboard without overlap.
- The existing `PerformanceKeyboard` resize path produces 25-by-110 white keys
  and proportional black keys at the normal production size. No key-placement,
  hit-testing, drag, MIDI, audio, graph, or serialization behavior was copied
  or changed.
- Automation exposes the resolved white-key width, height, and aspect ratio.
  `cycle-v2-agent-performance-keyboard.json` passed all 25 commands, including
  note down, live audio, drag to a second note, release, and voice decay. The
  production state reported a 212-by-150 panel, 25-by-110 white keys, and a
  4.4 aspect ratio.
- Native production-size evidence is at
  `/private/tmp/cycle-v2-keyboard-proportions-after.png`; semantic and pointer
  evidence is at
  `/private/tmp/cycle-v2-keyboard-proportions-after-os-report.json`. Visual
  review confirms that the keyboard reads as a compact piano and no longer as
  a row of broad pads.
- The focused keyboard suite passed 52 assertions in 6 test cases. The
  standalone Cycle V2 target built successfully with `--parallel 10`.
- The full Cycle V2 suite ran 527 test cases: 526 passed and the sole failure is
  the pre-existing `TestNodeCanvasHitRouter.cpp:66` help-text assertion already
  recorded in `docs/TDD/ui-bugs.md`; this slice introduced no new failure.
- `git diff --check` passed. The production change is limited to dock layout
  constants and geometry plus three read-only automation properties; it adds no
  node-kind branch, adapter, domain algorithm, or scalar math hot loop.
