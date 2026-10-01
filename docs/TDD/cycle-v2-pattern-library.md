# Cycle V2 Pattern Library

Status: Complete

## Current revision

The library currently contains 29 composed phrases. Replace them with six short,
basic templates: bass, lead, pad, keys, sustained, and rhythm. These are starting
points for sound audition, not finished compositions. Retarget clean factory
presets by sound family; preserve modified preset files. The sequence codec and
library remain authoritative for MIDI content and persistence. Add a type tag to
pattern metadata and filter the browser at the presentation boundary. Saving a
new pattern uses the chosen tag; editing a user pattern preserves its tag.

The browser should allocate one compact row to naming and actions, one row to
the type filter, then give the rest to 88 px pattern rows. Selecting a row must
retain the viewport position, including when selection refreshes the sidebar.
The minimap's CC line uses a muted warm colour distinct from the note blue.
No graph or audio policy moves into the browser; list filtering is O(patterns)
only when the filter or library changes, and row selection is O(1).

The old factory IDs are translated to the corresponding basic type only when
`PatternLibrary::find` resolves a reference. The authoritative data remains
the six `.cyclepattern` files and the authoritative sequence validator remains
`PresetPresentationCodec`. This narrow ID translation keeps older external
presets loadable; it does not duplicate MIDI behavior or keep old compositions.
The alias lookup is a stable compatibility boundary for saved preset IDs.

Completion requires a focused scroll regression, tag persistence and filter
test, retargeted fixture, actual-size screenshot, build, architecture audit,
style review, and commit. The prior verification below records the first slice.

## Revision verification

- Six basic factory patterns cover Bass, Lead, Pad, Keys, Sustained, and Rhythm.
  The bass template is an eight-note bassline; pad notes last several seconds.
  All templates contain CC1 motion and velocity variation. The 29 prior
  compositions were removed.
- 226 clean factory presets changed only their `patternId` to the basic type.
  The 23 pre-existing modified preset files were not edited. All factory
  references in the working tree resolve; old shipped IDs resolve through the
  compatibility translation.
- The browser stores a type tag with each pattern and filters by it. A new
  pattern takes the selected type; subsequent user edits retain it. The name
  field, create action, filter, and edit action occupy two compact rows. The
  search icon appears only in the actual preset search field. Row selection
  preserves the viewport's scroll position.
- At production size, the sidebar screenshot at
  `/private/tmp/cycle-v2-pattern-sidebar.png` shows the compact header, tagged
  rows, and warm automation traces. The piano-roll screenshot at
  `/private/tmp/cycle-v2-pattern-editor.png` shows the same automation colour.
- Pattern tests pass with 751 assertions across four cases. Piano-roll editor
  tests pass with 37 assertions across six cases. The app pattern, editor, and
  recording fixtures pass 13/13, 11/11, and 24/24 commands respectively.
  The broader MIDI tag has one unrelated failure because the pre-existing
  modified `astral-3.cyclegraph` no longer loads in its realtime test.
- Standalone Debug and test targets build with `--parallel 10`. The architecture
  audit reports only existing size triggers. Touched production files remain
  below size triggers; `NodeWorkspace` grew by two lines and still only routes
  creation and edit events. `git diff --check` passes. The touched painting
  paths contain no scalar `std::<math>` hot-loop calls. `clang-tidy` is not
  installed locally.

## Design

Patterns are named, separately stored MIDI phrases with stable IDs. A preset
stores `patternId` in `presetPresentation` and resolves that ID from the pattern
library on load. `PresetMidiSequence` remains the authoritative note, CC, and
duration model; `PresetPresentationCodec` remains the authority for validating
its JSON. Legacy embedded `sequence` data is read for old presets and retained
until the user explicitly assigns or saves a pattern.

The pattern library owns file lookup, validation, and atomic writes. Factory
patterns ship under `cycle-v2/content/patterns`; user patterns use an app data
directory. IDs are independent of display names and file paths. Pattern edits
write the library file, then refresh the keyboard if the current preset selects
that ID. Selecting a pattern changes only presentation metadata through
`GraphCommandDispatcher`; it does not mutate the graph or prepare audio.

The sidebar has Curves, Presets, and Patterns tabs. The Patterns page shows
named rows with note minimaps and an indication of automation, plus actions
to assign, create from the current phrase, and edit in the existing piano roll.
The keyboard remains the sole MIDI transport and recording owner. The piano
roll keeps its existing O(1) gesture updates and publishes a complete phrase
only at commit; library save is an explicit boundary action.

Factory migration ships a compact, curated set of musically distinct patterns
covering bass, lead, pad, keys, sustained instruments, and percussion, and
replaces each clean preset's embedded phrase with a selected pattern ID. Mod
wheel is the common automation lane; a preset may ignore CC values it does
not route. Pattern-specific controllers can be added when useful. Pre-existing
modified preset files are user-owned and remain unchanged; legacy loading
handles them. No graph, DSP, curve, or timeline implementation is copied.

## Completion criteria

- Pattern files are validated, named, browsable, selectable, editable, and
  previewed as note minimaps in the sidebar.
- Preset save/load round trips the stable ID and resolves notes/CCs through
  the library. Legacy inline phrases load and can be assigned a pattern.
- Recording and piano-roll edits can create or update a user pattern; factory
  pattern edits create a user copy rather than changing shipped content.
- Factory presets are migrated without touching pre-existing user changes.
- Focused semantic and UI tests, actual-size inspection, build, style review,
  architecture audit, and a coherent commit are complete.

## Architecture review

Baseline: `PerformanceKeyboard.cpp` 865 lines, `InlinePresetBrowser.cpp` 688
lines, `NodeCanvas.cpp` 2,788 lines, `NodeWorkspace.cpp` 591 lines. After this
slice they are 877, 737, 2,815, and 712 lines respectively. The 195-line
`PatternBrowser` owns row layout and hit targets. The 99-line
`MidiPatternMiniMap` supplies the shared pitch/time projection for browser
rows and the piano roll, including a compact CC trace. The 122-line
`Graph/PatternLibrary` owns validated file lookup and atomic user saves, using
`PresetPresentationCodec` for the existing sequence format. The keyboard
remains the only MIDI transport and held-note owner. `NodeWorkspace` resolves
the reference and coordinates library, keyboard, and semantic command, while
`GraphCommandDispatcher` remains the only owner of preset presentation edits.
There is no duplicate pattern eligibility or graph mutation decision in the
browser or canvas. Editing a factory pattern forks at the library boundary;
the editor never mutates the factory file.

`NodeCanvas.cpp` is already above the 1,200-line plan trigger. The 27 lines
added here only relay sidebar configuration, row targets, and the presentation
command. The extraction target for a future root split is a sidebar host that
owns the child component, tab visibility, bounds, and automation target
translation. That extraction should delete `presetSidebar`,
`configurePresetSidebar`, `configurePatternSidebar`,
`setPatternSidebarRecords`, and `presetSidebarPointerTargetsForAutomation`
from `NodeCanvas`; it should reuse `WorkspaceDock`'s overlay eligibility
decision. This feature adds no sidebar rendering or file policy to the canvas.

## Verification

- Twenty-nine distinct factory patterns cover six families, with dedicated
  jazz keys and sax lines. Piano, electric piano, and keys presets select from
  the jazz group; sax, brass, flute, organ, vibraphone, acid bass, and selected
  percussion names receive suitable families. No pair shares more than 45% of
  relative pitch and normalized onset points. All use CC1 motion, with
  different curves.
- All 227 migrated factory preset IDs resolve to shipped patterns. Twenty-two
  preset files that were already modified in the workspace remain legacy
  inline phrases and were not overwritten. Comparing each migrated file with
  `HEAD` after removing the old sequence and new ID confirms every other
  graph and presentation field is identical.
- Focused pattern, keyboard, sequence, and inline-browser Catch2 tests pass:
  994 assertions across 31 cases. Tests cover reference-only serialization,
  library validation and stable user-ID updates, factory resolution, row
  selection, and prior playback/recording/editor semantics.
- The app pattern fixture passes 13/13 commands: load a migrated preset,
  assign a different pattern from its row, save, and reopen. The recording
  fixture passes 24/24 and saves a user pattern ID without inline MIDI. The
  editor audition and Space fixture passes 11/11 after targeting a note in
  the new shared phrase. Its test-created user pattern was removed afterward.
- The actual-size sidebar was reviewed at
  `/private/tmp/cycle-v2-pattern-sidebar.png`. Selected rows, note minimaps,
  and distinct CC traces read clearly at production size. The shared minimap
  projection was also reviewed in the piano roll at
  `/private/tmp/cycle-v2-pattern-editor.png`.
- Standalone Debug and `CycleV2_tests` build with `--parallel 10`.
  `scripts/cycle_v2_architecture_audit.py` reports only the existing size
  review/plan files, including `NodeCanvas` and `PerformanceKeyboard`.
  `git diff --check` passes. No DSP or raster hot loops were changed.
  `clang-tidy` is unavailable locally.
