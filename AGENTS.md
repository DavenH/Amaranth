# Repository Guidelines

## Start With The Smallest Relevant Context

- Work from the repository root unless a command explicitly requires another directory.
- For an unfamiliar subsystem, read only its branch in `docs/code-map.md`, then
  use `rg` to locate the concrete symbols and callers. Do not inventory the
  whole repository before a narrow change.
- For production C++ edits, read `docs/style-guide.md` before editing. It is the
  authoritative style reference and stays separate from this always-loaded file.
- For nontrivial architecture, DSP, interaction, graph, or UI work, read the
  relevant sections of `docs/engineering-policy.md` and the active TDD/ADR.
- Read `docs/juce.md` before changing JUCE integration or writing generic
  framework-like utilities. Prefer an existing JUCE or Amaranth facility.
- For Cycle UI reproduction or screenshots, use `docs/cycle-agent-runbook.md`.

## Repository Map

- `lib/`: reusable Amaranth engine, DSP, mesh, interaction, UI, persistence, and utilities.
- `cycle/`: mature Cycle 1 synth/app/plugin; authoritative for established product behavior.
- `cycle-v2/`: node-graph Cycle 2 app, graph model, runtime, nodes, and canvas UI.
- `oscillo/`: standalone tuner and pitch-analysis app.
- `installer/`: product-neutral package installer.
- `docs/`: architecture, design, workflow, and session guidance.
- `scripts/`: setup, analysis, migration, UI automation, and regression helpers.

`docs/code-map.md` is the semantic source-tree index. Update it in the same
change when adding, removing, renaming, or changing the role of a source
boundary, entry point, reusable core, or canonical workflow. Replace or prune
stale entries instead of appending indefinitely. Keep descriptions at 120
characters or fewer and run `python3 scripts/check_code_map.py`.

## Discovery And Common Commands

- Files: `rg --files <area>`; symbols/callers: `rg -n '<term>' <areas>`.
- Read focused ranges with `sed -n '<start>,<end>p'`; inspect changes with
  `git status --short`, `git diff --stat`, and `git diff --check`.
- Bootstrap dependencies with `./scripts/install_deps.sh`; create the required
  `.env` with `./scripts/make_env.sh`.
- List presets with `cmake --list-presets`.
- Configure/build: `cmake --preset <preset>` then
  `cmake --build --preset <preset> --parallel 10`.
- Common presets: `standalone-debug`, `plugin-debug`, `tests`.
- Test with `ctest --preset tests`; narrow with `-R '<name>'` when appropriate.
- Always use `--parallel 10` or higher for CMake builds.

## Core Engineering Boundaries

- Search for the mature implementation before writing DSP, rasterization,
  interpolation, mesh interaction/traversal, serialization, or UI behavior.
- Reuse or extract shared behavior. Adapters may translate types, events,
  ownership, and lifecycle; they must not copy domain algorithms or state machines.
- Keep orchestration separate from domain algorithms and realtime/blockwise,
  analysis/gridwise, model, presentation, and adapter responsibilities.
- In DSP, analysis, and visualization hot paths, prefer `Buffer`/`VecOps`
  operations. Do not put scalar `std::<math>` work in per-sample/bin/pixel loops
  when a vectorized operation exists. Never allocate on the audio thread.
- Cycle 2 UI/editor code expresses semantic edits through
  `GraphCommandDispatcher` or a domain command service; it does not mutate
  `NodeGraph`, publish document changes, or manage undo directly.
- Preserve Cycle 1 interaction complexity when porting behavior. A live gesture
  update must not copy/serialize/rebuild unrelated graph, mesh, or audio state.
- Tests protect observable behavior and architectural intent, not scaffolding,
  trivial getters, or approximations of mature behavior.

## Change Workflow

1. Identify ownership, authoritative implementations, lifecycle/thread rules,
   expected complexity, and deletion targets. Create/update a TDD for
   cross-subsystem or multi-slice work.
2. Implement the smallest complete slice, then reread the production diff and
   remove duplicated mechanics, leaked policy, and mixed abstraction levels.
3. Recheck changed production files against `docs/style-guide.md`; run
   `git diff --check`, focused tests, and applicable lint/automation.
4. For nontrivial Cycle 2 production changes, run
   `python3 scripts/cycle_v2_architecture_audit.py`, inspect `git diff --stat`,
   and apply the size/responsibility review in `docs/engineering-policy.md`.
5. Commit each coherent completed slice with an imperative subject. Continue
   through the active TDD until its criteria and deletion targets are complete.

Record incidental assertions, crashes, or suspicious runtime failures in
`docs/TDD/ui-bugs.md` or `docs/TDD/audio-bugs.md` with concise repro evidence.

## Chat Playbook

Consult `docs/chats/README.md` and recent entries when a session establishes
durable preferences, naming, ambiguity-resolution, or workflow expectations.
Update it before compaction or when the user says `update playbook`; keep entries
semantic and concise rather than storing transcript chronology.
