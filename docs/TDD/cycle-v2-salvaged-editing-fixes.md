# Recovered editing fixes

Date: 2026-10-05. Status: Implemented and checked on macOS.

## Scope

Recover four independent fixes on `cycle2/salvage-editing-fixes` from `master`.
Keep the larger rewrite and unfinished changes on `cycle2/refactors-001`.
Preserve Cycle 1 behavior and reuse the existing undo system.

- Wait for previous preview work before reusing its mutable processing data.
- Prevent cursor polling from replacing hosted drag coordinates.
- Delete unused selection-position copies; their remaining readers were comments.
- Publish repeated Unison edits through the existing gesture and refresh services,
  retaining the starting model revision and committing one undo action.

## Ownership and size review

`GraphCommandDispatcher` owns revision validation and undo transactions.
`NodeEditorModelCommands` coordinates model publication with existing presentation
services (68 implementation lines, 28 header lines); it contains no mesh algorithms.
`PresentationRefreshPolicy` and the existing edit session still decide refresh timing.
`NodeCanvas.cpp` grows from 2,793 to 2,796 lines only to supply the existing gesture
mode to that policy. Further editor behavior should use the command services rather
than grow this canvas. `NodeEditorCommandService.cpp` decreases from 825 to 809 lines;
`NodeEditorHost.h` grows from 352 to 354. The architecture audit was run. The recovered
Unison fix does not replace graph or mesh storage or claim constant-cost mesh editing.

## Checks

- Both standalone apps and both test targets built with `--parallel 10`.
- Library mesh/envelope/rasterization tests: 83 cases, 11,870 assertions passed.
- Focused model, gesture, lifecycle and drag tests: six cases, 169 assertions passed.
- Wider causal/lifecycle/drag checks: 35 of 36 passed. The existing empty-Trimesh
  selection test failure is documented in `ui-bugs.md`; it remains unresolved.
- App automation: Unison Live and On Release each passed 33 commands; Guide gain
  and undo passed 20 commands. Startup assertions are recorded in `ui-bugs.md`.
- Style and diff checks passed; no hot-loop math was introduced. Clang-tidy unavailable.
