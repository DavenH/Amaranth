# Cycle V2 Live Output Meter And Keyboard Octave Controls

Status: Implemented

## Objective

Make the two global playback affordances behave and read like finished audio
software: the Output node must visibly follow the audio-device signal, and the
performance keyboard must use integrated side chevrons instead of small
header-level minus and plus buttons.

## Current Failures

The Output node paints the compile-time diagnostic preview even while the
standalone audio engine is running. The realtime renderer already measures the
device output, but that state stops at `NodeWorkspace` automation diagnostics.
The meter also maps linear amplitude directly to height. A normal live peak of
about 0.06 therefore occupies only six percent of the scale and appears stuck
in the lowest of twelve segments.

The keyboard's minus and plus buttons sit in a separate shallow header. Their
text glyphs, small square footprint, and detached placement make them look like
generic form buttons rather than octave navigation belonging to the keybed.
The `Keyboard` title repeats what the familiar object already communicates.

## Authoritative Implementations And Boundaries

- `RealtimeGraphRenderer::publishMetrics()` is authoritative for post-mix,
  post-headroom, post-clipping device output. It will retain independent left
  and right peaks without adding another audio traversal or allocating on the
  realtime thread.
- `NodeWorkspace` already crosses from the audio engine's atomic status into
  the message thread at 30 Hz. It will forward the measured peaks to
  `NodeCanvas`; UI code will never inspect audio buffers.
- `OutputMeterPresentation` remains authoritative for meter geometry and adds
  the audio-amplitude-to-display mapping. A -60 dB to 0 dB scale gives ordinary
  signals useful travel while exact zero remains dark.
- Message-thread meter ballistics use immediate attack and a short release so
  callback-to-callback valleys do not make the display flicker. They do not
  modify, feed back into, or claim to measure the audio signal.
- `AmaranthMidiKeyboard` remains authoritative for key placement, hit testing,
  pointer velocity, drag transitions, labels, and MIDI state.
- The existing `WorkspaceDock` chevron presentation is the visual reference
  for the two octave controls. The controls remain real JUCE buttons with
  tooltips, focus, and the existing octave-shift callbacks.

## Meter Contract

- Realtime diagnostics expose left peak, right peak, combined peak, and RMS.
- With an audio device ready, the Output node uses the live per-channel peaks;
  without one, it retains the compile-time diagnostic preview.
- Amplitude 0 maps to no fill, 0.001 (-60 dB) maps to the floor, 0.01 maps to
  one third, 0.1 maps to two thirds, and 1.0 maps to full scale.
- Values below the floor remain dark and values above full scale clamp.
- Attack is immediate. Release retains enough history to remain perceptible
  across UI frames and reaches the floor after silence rather than sticking.
- The Output node repaints only when its displayed live state changes.
- Automation exposes the exact raw and display levels used by the production
  widget so a held-note fixture can distinguish live movement from the static
  preview.

## Keyboard Geometry Contract

The normal panel becomes 276 by 150 pixels. Six-pixel outer insets leave a
264-by-138 row containing:

- a 28-by-138 previous-octave button;
- a four-pixel gap;
- the existing 200-by-138 one-octave keybed;
- a four-pixel gap;
- a 28-by-138 next-octave button.

Eight white keys remain exactly 25 pixels wide and become 138 pixels high, a
5.52 height-to-width ratio. The side controls therefore improve integration
without sacrificing the key acquisition budget. Their visible footprint and
actual hit target are the same tall rectangle. The redundant title, header
divider, minus glyph, and plus glyph are deleted.

At a 500-by-300 compact canvas the panel remains 276 pixels wide and yields
height before reducing the cable legend below its 30-pixel floor. Both octave
buttons always equal the keybed height.

## Verification

- Renderer tests prove both channel peaks are published and their maximum is
  the existing combined peak without realtime allocation or lock regressions.
- Presentation tests prove the dB landmarks, clamping, exact-zero state, and
  attack/release sequence.
- The live keyboard fixture holds a note long enough to assert a nonzero
  production meter level; the focused ballistics sequence verifies decay after
  release without depending on automation's blocking idle wait.
- Keyboard component and dock tests assert all row dimensions, containment,
  side ordering, equal heights, key width, and preferred/compact bounds.
- Automation clicks both real octave buttons and verifies the C3-C4 range
  advances and returns.
- Native production-size captures review idle and held-note states.
- Focused and full Cycle V2 tests, standalone build, `git diff --check`, hot-loop
  review, and production-diff review complete before the final commit.

## Completion Criteria

- The Output node visibly responds to held live audio and returns to silence.
- Its movement is based only on measured device output and uses a useful audio
  scale rather than fabricated activity or linear-amplitude compression.
- Stereo measurements remain independent through the realtime/UI boundary.
- The keyboard presents tall left/right chevrons immediately beside the keys,
  with no header buttons or redundant title.
- Key proportions, octave behavior, utility separation, and compact layout
  remain correct.
- No graph, serialization, undo, MIDI-routing, or audio-rendering semantics are
  changed.

## Implementation Evidence

### Live output meter

- `RealtimeGraphRenderer` now publishes independent post-output left and right
  peaks alongside its existing combined peak and RMS. The existing
  allocation/lock regression test remains green.
- `NodeWorkspace` forwards one atomic status snapshot per 30 Hz message-thread
  tick. `NodeCanvas` owns only the UI ballistics and passes the resolved live
  levels through presentation; the Output renderer bypasses its compile-time
  cached sprite while those levels are available.
- `OutputMeterPresentation` maps raw amplitude over -60 to 0 dB. The focused
  amplitude landmarks and attack/release sequence pass 46 assertions across
  five Output-meter test cases.
- The real audio-device fixture reports left/right live amplitudes around
  0.048/0.043 and a left display level around 0.56 while C3 is held. All live
  assertions and the complete note-down, drag, release, and voice-tail sequence
  pass in `/private/tmp/cycle-v2-live-meter-report.json`.
- Native held-note evidence is
  `/private/tmp/cycle-v2-live-meter-before-keyboard.png`; both Output channels
  visibly rise to the middle of the scale instead of remaining on the lowest
  bar.

### Keyboard octave controls

- The keyboard panel is 276 by 150 pixels. Its shared layout resolves to
  28-by-138 previous/next octave controls, two four-pixel gaps, and the existing
  200-by-138 keybed. White keys remain 25 pixels wide with a 5.52 ratio.
- The old `TextButton` minus/plus glyphs, header layout, divider, and redundant
  `Keyboard` title were deleted. The replacement JUCE buttons reuse the
  `WorkspaceDock` chevron language and expose matching hover, focus, pressed,
  tooltip, and pointing-cursor affordances.
- The focused keyboard suite passes 60 assertions in six test cases, including
  preferred and compact utility layouts, button/key containment, proportional
  black keys, MIDI note state, and octave release behavior.
- The production fixture clicks the actual right button and observes C4-C5,
  then clicks the actual left button and observes C3-C4 before completing the
  note-down, live-audio, drag, release, and voice-tail sequence. All commands
  pass in `/private/tmp/cycle-v2-live-meter-keyboard-final-report.json`.
- Final native evidence is
  `/private/tmp/cycle-v2-live-meter-keyboard-final.png`; it shows the held C3,
  both live Output channels around mid-scale, and the full-height side
  chevrons without the former header.

### Final verification

- The standalone Cycle V2 target builds successfully with `--parallel 10`.
- `CycleV2_tests` runs 529 test cases: 528 pass and the sole failure remains the
  pre-existing `TestNodeCanvasHitRouter.cpp:66` help-text assertion recorded in
  `docs/TDD/ui-bugs.md`.
- `git diff --check`, fixture JSON validation, hot-loop review, and production
  diff review pass. The keyboard slice adds no graph mutation, node-kind
  switch, serialization field, adapter, DSP algorithm, or copied key behavior.
