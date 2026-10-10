# Cycle V2 pattern library management

Status: Implemented.

## Contract and ownership

`PatternLibrary` owns stable pattern IDs and `.cyclepattern` files. Renaming a
user pattern changes only its display name and retains its ID, sequence, and
tags. Deleting a user pattern moves its file to Trash and reloads the library.
Factory patterns remain immutable. Existing presets may still hold the deleted
ID; their current resolver falls back to an embedded sequence, then to the
audition key when no sequence exists. Deletion must explain that consequence
without mutating unrelated preset files.

`PatternBrowser` owns selection, visible controls, and confirmation UI.
`NodeWorkspace` routes the semantic requests to `PatternLibrary` and refreshes
the browser and keyboard. `NodeCanvas` and `InlinePresetBrowser` only forward
callbacks. File writes never occur in a paint or audio callback.

Editing a factory pattern currently saves a new user variation after the first
gesture. Make that copy explicit before opening the editor so accidental edits
do not silently create files; subsequent edits of the copy update the same ID.

## Completion

- Rename and Delete are visible for selected user patterns and disabled for
  factory patterns.
- Rename retains ID, sequence, tags, and references across reload.
- Delete moves to Trash after confirmation, refreshes the library, and leaves
  saved preset documents untouched.
- Factory editing asks to create a named copy and does not create one on cancel.
- Focused persistence and UI sequence tests, production-size screenshot,
  architecture/style audit, build, and commit pass.

## Verification and architecture review

The persistence and browser action sequence tests pass, including factory copy
cancel/confirm, user edit, rename, delete, and factory protection. The
neighboring pattern and inline-browser tests pass: 147 assertions across seven
cases. The standalone Debug app builds on macOS. A production-size 1728 by
962 screenshot confirms the three action buttons fit the sidebar and the tag
filters and note rows remain visible.

`PatternLibrary` remains the sole file-operation and ID owner. `PatternBrowser`
owns prompts and selection eligibility. `NodeWorkspace` routes semantic
requests and refreshes the keyboard; `NodeCanvas` and `InlinePresetBrowser`
only forward callbacks. The architecture audit passes. Touched size triggers
are `NodeCanvas.cpp` (2851 lines, +3 narrow forwarding lines) and
`NodeWorkspace.cpp` (773 lines, +39 orchestration lines). No policy was copied
into the graph or runtime layers. No extraction is required for these narrow
forwarding additions.
