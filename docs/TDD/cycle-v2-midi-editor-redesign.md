# MIDI editor visual redesign

Status: Complete

Implement the approved `.context/midi-editor-design/mockup-v3.png` composition.
The phrase is the primary content. Keep only editable tempo, Snap 1/16 and Clear
in the toolbar. Remove the repeated phrase heading, Length widgets, selected
event metadata and bottom shortcut bar. Keep the native title “MIDI Pattern”.

Geometry: shared 80 px left / 16 px right timeline gutters, 32 px notes-only
overview, 24 px tick ruler, 36 chromatic rows (increased from the initial 29),
48 px velocity and 76 px controller plots with 4 px gaps. Freed height belongs to the piano roll.
Use recognizable grey natural / near-black accidental keys, restrained blue
notes and thin velocity stems, small amber automation handles and faint fill.
Keep existing 8 px controller and velocity acquisition and Shift fine gestures.
Overflowing controller lanes scroll independently rather than clipping or
squeezing the note grid. Verify normal and minimum window sizes and many lanes.

Ownership: PresetMidiEditor remains the authoritative authoring/gesture owner;
PresetMidiEditorView owns paint and shared geometry. Reuse JUCE editable Label,
ScrollBar, MIDI pitch classification and existing MidiPatternMiniMap note paint.
PerformanceKeyboard retains transport, audition, save and semantic publication.
No graph, DSP, scheduler or serialization policy moves into presentation.
Keep saved trailing silence, automatic content extension, two-measure authoring
padding, Backspace/arrow seeking, Space playback and existing commit frequency.

Deletion targets: duration UI and duration-drag state; tempo stepper and separate
BPM label; title/selection/footer prose; overview automation; oversized lane
gaps, velocity caps and controller handles. Do not add replacement metadata.

Verification: focused editor/keyboard/scheduler tests with actual editing
gestures, direct tempo entry, unchanged note times and saved duration, dense
pitch mapping, notes-only overview and controller overflow. Build exact Debug
and Release apps, run readonly app seek fixtures, and inspect production-size
normal/compact screenshots against the approved mockup. Run architecture,
code-map, production-style/diff and size checks before completion.

## Verification

The production-size normal (1122 × 848 content) and compact (900 × 640)
native captures match the approved hierarchy. Normal plot bounds are
80,100,1026,612 for notes; 80,716,1026,48 for velocity; and 80,768,1026,76
for MOD. Compact note height is 404, with velocity at 508 and MOD at 560.
Both retain the 4 px gaps and bottom inset. All plots share 80/16 px gutters.
The toolbar spans the panel: tempo and Clear have opposing 16 px edge insets,
all controls share y=8..36 and centre y=22, and the overview begins at y=44.
The ordinary screenshot includes selected-note/resize and velocity-cap states.

Debug and Release apps build. Focused authoring/keyboard/scheduler verification
passes 1,632 assertions in 61 cases, including direct tempo text entry and
clamping, saved silence/timing retention, all 29 initial pitches at both sizes, identical
notes-only overview pixels with/without CC data, and editing the eighth CC lane
after scrolling at minimum size. Velocity retains its existing 8 px acquisition
and Shift mapping (2 px per velocity step), despite its smaller visible cap.

The 9-command layout, 22-command ruler/overview and 43-command navigation app
fixtures pass; navigation
exports remain byte-identical. Native pointer checks cover tempo entry/cancel,
Snap off/on, note selection/audition and native resizing. Evidence lives under
`.context/midi-redesign-*.log`, `.context/midi-editor-native-results.json`,
`.context/midi-editor-native.png` and `.context/midi-editor-native-compact.png`.

Architecture and code-map checks pass, with existing unrelated size triggers.
Changed owners remain cohesive and below thresholds: editor 621 lines, view 377,
header 135, keyboard panel 643. The panel changes only its technical dialog ID;
transport, audition and save ownership are unchanged. Shared lane constants and
geometry drive paint, hit testing and overflow. Production diff/style and
whitespace checks pass. All deletion targets above are complete.


## Three-octave pitch range

The requested follow-up raises the shared pitch count from 29 to 36 while
preserving the window and lane dimensions. Rendering, hit testing, initial
centering and MIDI-range scrolling clamps derive from that one count.
Existing gesture tests now use pitch-row geometry instead of fixed density.
All 36 rows remain editable at normal and minimum window sizes; all 1,674
assertions in 61 focused cases pass. Debug and Release apps build. The layout
fixture verifies the actual app; capture: `.context/midi-editor-36-rows.png`.


## Inset lane labels

Velocity, MOD and CC names now sit inside the top-right of their own plot,
with an 8 px horizontal / 4 px vertical inset. One shared painter draws names
after the lane data so handles and stems cannot paint over the text. Plot sizes,
value scales and gesture geometry stay unchanged. Debug and Release builds and
all 698 assertions in 24 focused editor cases pass. Actual-app single/multiple
controller captures are `.context/midi-lane-labels.png` and
`.context/midi-lane-labels-multiple.png`; code-map and architecture checks pass.
