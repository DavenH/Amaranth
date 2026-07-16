# Repository Guidelines

## Project Structure & Module Organization
- root: CMake entry (`CMakeLists.txt`, `CMakePresets.json`), `.clang-tidy`, `.env` (required).
- `lib/`: Shared Amaranth library (JUCE modules vendored under `JuceLibraryCode/`), unit tests in `lib/tests/`.
- `cycle/`: Main synth app/plugin; sources in `cycle/src/`, tests under `cycle/tests/`. `cycle/README.md` has a lot of context about the design.
- `oscillo/`: Standalone utility; sources in `oscillo/src/`, tests in `oscillo/tests/`.
- `docs/`: Style and contributor notes. `scripts/`: setup helpers.

## Build, Test, and Development Commands
- Bootstrap SDKs: `./scripts/install_deps.sh` (JUCE, VST3, Catch2; IPP on Linux).
- Write env file: `./scripts/make_env.sh` (creates `.env` with `JUCE_MODULES_DIR`, `VST3_SDK_DIR`, `CATCH2_CMAKE_DIR`, and optional `IPP_DIR`).
- List presets: `cmake --list-presets`.
- Configure + build (examples):
    - Standalone Debug: `cmake --preset standalone-debug && cmake --build --preset standalone-debug --parallel 10`
    - Plugin Debug: `cmake --preset plugin-debug && cmake --build --preset plugin-debug --parallel 10`
    - Tests: `cmake --preset tests && cmake --build --preset tests --parallel 10`
- Always use `--parallel 10` or higher when running `cmake --build`.
- Run tests: `ctest --test-dir build/tests -V` (Catch2 discovery is enabled).
- Capture Cycle UI screenshots and filtered launch logs for regression checks and UI bug fixes: `scripts/capture_cycle_ui.sh /tmp/cycle-ui.png /tmp/cycle-logs.txt`. The script also writes the complete unfiltered log to `/tmp/cycle-logs.txt.raw`; inspect the filtered log first to avoid unnecessary token use.
- Cycle agent automation runbook: `docs/cycle-agent-runbook.md`. Use this for targeted UI bug reproduction, focused fixture runs, agent screenshots, state export/assertions, OpenGL panel capture, crash `.ips` collection, and long-running session IPC.

## Coding Style & Naming Conventions
- C++17, 4‑space indentation, braces on the same line; always brace single statements.
- Headers start with `#pragma once`; system includes before project includes with a blank line.
- Prefer `enum class`; use `override` on overrides; pointer/reference bind to type (`Foo* p`).
- Follow `docs/style-guide.md` for alignment and initializer‑list formatting.
- Observe the instructions about JUCE SDK library in  `docs/juce.md`
- Linting: `.clang-tidy` is configured; example: `clang-tidy -p build/standalone-debug path/to/file.cpp`.

## Style Enforcement
- Before making code edits, read `docs/style-guide.md` and treat it as authoritative.
- In DSP, analysis, and visualization hot paths, use `Buffer`/`VecOps` array operations for transforms, normalization, nonlinear shaping, and reductions.
- Do not use scalar `std::<math>` calls (for example `std::pow`, `std::sqrt`, `std::abs`, `std::isfinite`) inside per-sample/per-bin/per-pixel loops when `Buffer`/`VecOps` equivalents are available.
- Keep inner loops minimal: indexing, lookup, and write only. Move math outside loops.
- Only if `Buffer`/`VecOps` cannot express an operation should we introduce scalar fallback logic.
- Before finalizing a change touching DSP or visualization paths, run a self-check on modified files for scalar `std::<math>` usage in hot loops and fix violations.

## Architectural Boundaries & Existing Implementations
- Before implementing nontrivial DSP, rasterization, mesh interaction, graph traversal, serialization, or UI behavior, search for an existing implementation in the repository.
- Treat existing mature implementations as authoritative when porting or extending equivalent behavior. Prefer adapter/facade code around existing behavior over reimplementation.
- Before adding a compatibility layer or adapter, write down the authoritative implementation, the exact behavior reused unchanged, the translation performed at the boundary, and the intended deletion or stable end state. An adapter may translate types, events, ownership, and lifecycle; it must not copy interaction, rendering, topology, DSP, or domain state-machine behavior.
- If reuse would require copying mature behavior, stop. Extract a shared core used by both callers or document the blocked extraction in a TDD and request architectural direction. Do not ship the copied behavior as a transitional implementation.
- Treat a compatibility/bridge class that grows beyond narrow boundary translation, accumulates domain-specific branches, or mixes unrelated node families as an architectural failure. Do not continue adding to it. A large new adapter, repeated `NodeKind` switching, or shared code containing one client's domain types requires explicit architectural justification before implementation.
- Before accepting a nontrivial implementation, inspect the production diff size, largest changed files, new type/kind branches, and overlap with mature sources. Unexpected code volume is evidence to re-check the design, not merely a review inconvenience.
- Do not duplicate complex domain logic locally just to satisfy a narrow test, preview, or temporary workflow. If reuse is blocked, document the blocker and stop at the appropriate abstraction boundary.
- Keep abstraction levels separated. High-level orchestration code should coordinate routing, lifecycle, and ownership; it should not contain domain algorithms.
- Domain-specific behavior belongs behind domain-specific interfaces or modules, with blockwise/realtime, gridwise/analysis, UI/model, and adapter layers separated where appropriate.
- If an implementation would require approximating existing behavior, record the missing adapter/refactor in the relevant TDD or refactor document instead of shipping the approximation.
- Tests must preserve product intent and architectural intent. Do not write or update tests to bless simplified behavior when the requirement is parity with an existing mature implementation.
- Prefer fewer, higher-signal tests that guard the intended contract over narrow tests that lock in scaffolding, fake data, or implementation accidents.
- When a test creates pressure to add low-quality production code, change the design or test boundary rather than weakening the architecture.
- Do not mark a TDD implemented while its principal architecture, deletion targets, negative boundaries, or completion criteria remain unfinished. Use a partial/in-progress status and state the remaining work explicitly.

## Engineering Loop

For every nontrivial implementation, execute these stages in order. Treat each
stage as an active task with evidence; do not assume passive awareness of an
instruction is sufficient.

1. **Technical design**: identify the authoritative implementation, ownership,
   thread/lifecycle boundaries, semantic contracts, expected complexity, and
   deletion targets. Create or update a TDD when the change crosses subsystem
   boundaries or contains multiple coherent slices.
2. **Technical implementation**: implement the smallest complete slice against
   the design. Reuse mature code and keep domain logic below orchestration
   layers.
3. **Refactor pass**: after behavior works, reread the production diff. Extract
   leaked low-level mechanics, remove duplication and scaffolding, split mixed
   abstraction levels, and verify high-level classes read as orchestration.
   Passing tests do not make this stage optional.
4. **Style check**: reread every changed production file against
   `docs/style-guide.md`, run `git diff --check`, inspect hot loops, and run
   applicable clang-tidy checks where available. Correct compressed formatting,
   oversized functions, mixed declarations, and misleading names before
   final verification.
5. **Commit**: run proportionate semantic tests and commit the coherent slice
   with an imperative subject. For an explicitly requested implementation train,
   continue through this stage without waiting for a separate commit prompt
   unless the user asked to leave changes uncommitted.
6. **Continue to completion**: reread the active TDD and return to step 2 for
   the next incomplete slice. Repeat implementation, refactor, style, testing,
   and commit until every completion criterion and deletion target is satisfied.
   Stop only for a genuine unresolved design ambiguity, an external blocker, or
   explicit user direction. A passing slice, a clean commit, limited remaining
   time, or the availability of further useful work is not a stopping point.

Before committing, report internally against this checklist in sequence. If a
stage uncovers an architectural defect that cannot be corrected within scope,
record it in the active TDD/refactor document rather than silently proceeding.
After committing, actively inspect the TDD for remaining work; do not wait for
the user to prompt the next slice or to act as the implementation scheduler.

## Reuse Before Reimplementation
- If you are about to write interpolation, rasterization, curve evaluation, oversampling, convolution, envelope/loop semantics, mesh traversal, or DSP transfer logic, stop and locate the existing implementation first. Reuse it or create a narrow adapter. Do not reimplement it in place.

## Refactors

If confusing code patterns, reuse possibilities, or better abstractions are detected, check `docs/TDD/refactors.md` if a suggestion to fix it is already present. If not record refactor wishes there.

## Testing Guidelines
- Framework: Catch2.
- Locations: `lib/tests/*.cpp`, `cycle/tests/*.cpp`, `oscillo/tests/*.cpp`.
- Naming: begin test files with `Test*.cpp` or place under a `tests/` subfolder.
- Coverage: prefer tests for new DSP modules and bugfixes; include edge‑rate/sample‑rate cases.
- Tests must assert observable semantic behavior or an architectural boundary. Avoid tests that merely execute scaffolding, restate implementation details, check trivial getters, or bless fake/approximate behavior. For interaction work, cover the complete event sequence and resulting visible/model state, not only isolated helper methods.
- Every UI interaction regression should receive a focused automation fixture or assertion that fails for the reported behavior: hover/current highlighting, drag movement, selection stability, editor publication, and downstream effects as applicable.
- UI regressions: use `scripts/capture_cycle_ui.sh /tmp/cycle-ui.png /tmp/cycle-logs.txt` to capture the Cycle UI and filtered launch logs before/after visual changes or when investigating UI bugs. Read `/tmp/cycle-logs.txt` first; only read `/tmp/cycle-logs.txt.raw` when detailed rasterizer/mesh logs are needed.
- UI automation: prefer a focused fixture with `scripts/run_cycle_agent.sh` over the full smoke suite while debugging. Add small, stable fixtures under `scripts/fixtures/` for regressions that agents should be able to reproduce. See `docs/cycle-agent-runbook.md`.
- When assertions, crashes, or suspicious runtime failures appear incidentally and are not the direct cause of the current feature/fix work, record a concise note in `docs/TDD/ui-bugs.md` or `docs/TDD/audio-bugs.md` as appropriate. Use `audio-bugs.md` for realtime audio pipeline issues; otherwise prefer `ui-bugs.md`. Include the assertion text, context, repro artifact/log path when available, and whether it is still open or was addressed.

## Commit & Pull Request Guidelines
- Commits: short, imperative subject (“Fix buffer wraparound”), optional body for context.
- PRs: include description, linked issues, platform (Linux/macOS), and screenshots for UI changes.
- CI/local proof: show build preset used and `ctest` output when adding tests.

## Runtime & Configuration Tips
- `.env` is mandatory at configure time; CMake reads it to locate SDKs.
- Linux uses Intel IPP; macOS uses Accelerate/vDSP. Do not allocate on audio threads—preallocate and reuse buffers (see `docs/contributing.md`).

## Chat Playbook
- Consult `docs/chats/README.md` and recent files in `docs/chats/` when a session establishes durable user preferences, naming direction, ambiguity-resolution rules, or workflow expectations.
- Update `docs/chats/` before any conversation compaction and whenever the user explicitly asks to preserve new guidance from the session.
- Treat the exact phrase `update playbook` as a direct instruction to add or refresh the relevant `docs/chats/` session summary immediately.
- Keep chat-playbook updates concise, generalized, and semantic; do not store raw transcript dumps.
