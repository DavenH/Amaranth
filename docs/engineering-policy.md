# Engineering Policy

Read only the sections relevant to the work. `AGENTS.md` contains the compact
repository-wide rules; this file holds the detailed policies needed for
nontrivial production changes.

## Architecture And Reuse

- Treat existing mature implementations as authoritative when porting or
  extending equivalent behavior. Prefer a narrow adapter around existing
  behavior over reimplementation.
- Before adding an adapter, record the authoritative implementation, unchanged
  behavior, boundary translation, and intended deletion or stable end state.
- If reuse would require copying mature behavior, extract a shared core or
  document the blocked extraction in a TDD and request architectural direction.
- A bridge that accumulates domain branches, unrelated node families, or
  client-domain types is an architectural failure, not a reusable abstraction.
- Do not duplicate domain logic for a narrow test, preview, or temporary
  workflow. Tests must not bless simplified substitutes for required parity.
- High-level code coordinates routing, lifecycle, and ownership. Domain
  algorithms remain behind domain-specific interfaces at the proper layer.
- Check `docs/TDD/refactors.md` before recording a newly discovered refactor;
  add concise new findings there when they are not already covered.

Before accepting a nontrivial implementation, inspect production diff size,
largest changed files, new type/kind branches, and overlap with mature sources.
Unexpected volume is a design signal: reassess ownership and reuse.

## Cycle V2 Architecture Review

For every nontrivial Cycle V2 production change:

- run `python3 scripts/cycle_v2_architecture_audit.py`;
- measure touched C++ files with `wc -l` and inspect `git diff --stat`;
- review responsibility/composition at 800 lines for `.cpp` or 300 for headers;
- record an extraction plan or cohesion reason at 1,200/450 lines, or when one
  change grows a file by 200 lines.

Name each large file's responsibilities, stable collaborators, and policy
owners. Search UI, graph, runtime, and node layers for duplicate lifecycle,
invalidation, transaction, preparation, and eligibility decisions. Put a shared
decision behind one authoritative boundary and make callers supply domain facts.

For cross-subsystem refactors, record baseline/after sizes, policy decision
sites, dependency direction, deletion targets, and semantic or operation-count
proof in the active TDD. Moving code without removing responsibilities or
duplicate decisions does not count as completion.

## Cycle V2 Graph Mutation

- UI/editor code expresses semantic edits through `GraphCommandDispatcher` or
  a domain command service. It must not mutate `NodeGraph`, call `GraphEditor`,
  publish document changes, or manage undo directly.
- `GraphDocument::graph()` is the durable committed graph.
  `GraphCommandDispatcher::editingGraph()` is a possibly transient read view.
- A gesture captures its durable base revision once. Each publication retains
  that base; the dispatcher translates it to transient snapshots and commits or
  cancels the transaction.
- Revisions identify immutable model content, not UI events. Equal-revision
  content differences conflict in the durable graph. Parameter-only updates may
  reuse a revision only inside a dispatcher-owned transient edit.
- A successful semantic command merges all changed fields into one
  `GraphEditResult`/`GraphChangeSet`; scheduling and repaint use that result.
- New live-edit paths need a sequence test with at least two gesture updates,
  commit, observable downstream effect, and undo.

## Interaction Complexity Parity

Before changing a mature Cycle 1 interaction, document pointer-down, movement,
and commit costs. Cycle 2 must not increase any phase's asymptotic cost.

- Work for a semantic delta scales with changed objects and an explicitly
  required local render product, not unrelated graph, resource, or mesh size.
- Live movement must not clone `NodeGraph`, deep-copy a complete mesh/model,
  serialize state, stringify JSON for equality, rebuild unrelated presentation,
  or prepare durable resources.
- A temporary undo snapshot may occur once at a gesture boundary, but it must
  copy only the smallest affected domain state. The target is an invertible
  semantic delta used forward for edit/redo and backward for undo.
- Gesture tests enlarge unrelated state while holding the edited delta fixed
  and assert stable copy, serialization, lookup, preparation, and rebuild counts.
- Treat a Cycle 1 O(1) action that becomes O(n) in Cycle 2 as a correctness bug.

## DSP And Visualization Hot Paths

- Use `Buffer`/`VecOps` operations for transforms, normalization, nonlinear
  shaping, and reductions when they can express the operation.
- Keep inner loops to indexing, lookup, and write. Move math outside loops.
- Use scalar fallback only when the vector APIs cannot express the operation.
- Before finalizing touched hot paths, search modified files for scalar
  `std::<math>` calls inside per-sample/bin/pixel loops and correct violations.
- Preallocate and reuse audio-thread storage. macOS uses Accelerate/vDSP; Linux
  uses Intel IPP.

## UI Interaction And Verification

For JUCE canvas context menus, set an explicit screen-space click rectangle with
`PopupMenu::Options::withTargetScreenArea`, after `withTargetComponent` when its
display scale is needed. Convert with `localPointToGlobal` or use the event's
screen position. `withTargetComponent(this)` alone anchors at the component
origin; `withMousePosition()` may miss synthesized automation events. Verify a
new menu at a non-origin click in a focused fixture.

UI regressions require a focused fixture or assertion for the reported event
sequence and observable state. Prefer `scripts/run_cycle_agent.sh` or
`scripts/run_cycle_v2_agent.sh` while debugging. Use
`scripts/capture_cycle_ui.sh /tmp/cycle-ui.png /tmp/cycle-logs.txt` for visual
checks; inspect the filtered log before its `.raw` companion. See
`docs/cycle-agent-runbook.md` for state export, OpenGL capture, IPC, and crashes.

## Tests And Completion

- Catch2 tests live in `lib/tests`, `cycle/src/**/tests`, `cycle-v2/tests`, and
  `oscillo/tests`; prefer `Test*.cpp` names.
- Test observable semantics, full interaction sequences, downstream effects,
  architectural boundaries, and edge sample rates where relevant.
- Prefer fewer high-signal tests over checks of scaffolding, fake data, trivial
  getters, or implementation accidents.
- When a test pressures production code toward a weak abstraction, change the
  design or test boundary instead of weakening the architecture.
- A TDD remains `In Progress` while principal architecture, negative
  boundaries, deletion targets, or completion criteria remain unfinished.
- Run proportionate semantic tests, refactor/style checks, and commit each
  coherent slice. Continue through the active TDD unless blocked by a genuine
  design ambiguity, external dependency, or explicit user direction.

Use a short imperative commit subject. Pull requests include a description,
linked issues when applicable, the tested platform, UI screenshots for visual
changes, and the build preset plus CTest evidence for new tests.
