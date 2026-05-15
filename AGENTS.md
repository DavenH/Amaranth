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

## Refactors

If confusing code patterns, reuse possibilities, or better abstractions are detected, check `docs/TDD/refactors.md` if a suggestion to fix it is already present. If not record refactor wishes there.

## Testing Guidelines
- Framework: Catch2.
- Locations: `lib/tests/*.cpp`, `cycle/tests/*.cpp`, `oscillo/tests/*.cpp`.
- Naming: begin test files with `Test*.cpp` or place under a `tests/` subfolder.
- Coverage: prefer tests for new DSP modules and bugfixes; include edge‑rate/sample‑rate cases.
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
