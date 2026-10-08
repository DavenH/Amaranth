# Cycle V2 Scratch and Cable UI Repairs

Status: implemented (2026-10-07). The remaining Spy tile brightness report is
tracked separately in `docs/TDD/ui-bugs.md`.

## Contract and owners

- `EnvelopePurpose::applyEnvelopePurpose` owns the derived output port type.
  Parameter delta replay must call it after changing an envelope's purpose so
  undo, redo, and committed transient edits expose the same port as direct edits.
- `GraphConnectionValidator` and `GraphEdgeValidator` own scratch connection
  eligibility. Canvas gestures use those validators; no second connection rule
  belongs in the UI.
- `GraphEditor` owns deletion of a presented cable, including its inline pan and
  anchored Spies. `NodeCanvasAuthoring` only translates the presented modulation
  bundle to edge indices. Deletion is one undo step.
- Cable removal and undo should touch the removed pan, its adjacent edges, and
  anchored probes. They should not copy unrelated graph nodes or audio samples.

## Verification

- A new envelope changes purpose twice in one transient edit, commits, connects
  to Voice Context Scratch, and undoes both connection and purpose.
- Deleting a cable with a Spy removes and restores both. Deleting an inline pan
  cable with Spies on both sides removes and restores the pan, edges, and Spies.
- A focused automation fixture chooses Scratch in the expanded envelope editor,
  closes it, drags the output to Voice Context Scratch, and undoes the edge.
- A focused automation fixture deletes a real graph cable with an anchored Spy,
  checks both counts, and exercises undo. The C++ test also covers redo.
- Review the production diff, architecture audit, changed file sizes, style,
  and targeted tests before commit.

## Deletion target

Remove the full-graph undo snapshot from the new cable removal command. Keep
existing standalone node and probe deletion policies separate from this repair.

## Architecture review

The previous canvas deletion loop called `removeEdgeAt` once per presented
edge. `NodeCanvasAuthoring` now only resolves a presented bundle and reconciles
selection. `GraphEditor` decides which inline pan, edges, and anchored probes
belong to the deletion. `GraphDelta` records those removed objects with their
indices for undo and redo; the dispatcher owns publication. No second cascade
decision remains in UI, runtime, or node code. The new command does not copy
the graph or unrelated audio samples. Planning still scans graph edges and
probes, and vector erasure/insertion shifts other entries. There is no new
live movement path, serialization, or durable preparation.

The architecture audit reports 21 existing size triggers. The only touched
trigger, `NodeCanvasAuthoring.cpp`, is 960 lines versus 959 at baseline. It
still coordinates node, cable, editor, and selection commands; their semantic
policies remain with their domain owners. This edit adds no new policy to that
file, so extraction of its broader responsibilities remains separate. Other
touched files: `GraphDelta.cpp` 278 to 364 lines, `GraphEditor.cpp` 239 to 311,
and `GraphCommandDispatcher.cpp` 594 to 609. No touched file reached a new
size trigger or grew by 200 lines. The cable delta replaces the old
multi-command path rather than wrapping it.

## Completed checks

- `cmake --build --preset tests --target CycleV2_tests --parallel 10` and the
  standalone Debug app build succeeded.
- `[authoring]~[scratch]`: 24 cases, 248 assertions passed. The focused new
  Scratch case passed with 12 assertions.
- `cycle-v2-agent-new-scratch-envelope-connection.json`: 13 commands passed.
  `cycle-v2-agent-delete-spy-cable.json`: 9 commands passed.
- `git diff --check` passed; the architecture audit and changed file sizes
  were reviewed. Applicable clang-tidy completed with existing warnings.
- The broader authoring filter still hits the zero spectral Spy preview
  assertion also present in the untouched baseline binary; it is recorded in
  `docs/TDD/ui-bugs.md` and is outside this repair.
