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
    - Standalone Debug: `cmake --preset standalone-debug && cmake --build --preset standalone-debug`
    - Plugin Debug: `cmake --preset plugin-debug && cmake --build --preset plugin-debug`
    - Tests: `cmake --preset tests && cmake --build --preset tests`
- Run tests: `ctest --test-dir build/tests -V` (Catch2 discovery is enabled).

## Coding Style & Naming Conventions
- C++17, 4‑space indentation, braces on the same line; always brace single statements.
- Headers start with `#pragma once`; system includes before project includes with a blank line.
- Prefer `enum class`; use `override` on overrides; pointer/reference bind to type (`Foo* p`).
- Follow `docs/style-guide.md` for alignment and initializer‑list formatting.
- Observe the instructions about JUCE SDK library in  `docs/juce.md`
- Linting: `.clang-tidy` is configured; example: `clang-tidy -p build/standalone-debug path/to/file.cpp`.

## Testing Guidelines
- Framework: Catch2. Get access to TEST_CASE, REQUIRE, etc with `#include <catch2/catch_test_macros.hpp>`
- Locations: `lib/tests/*.cpp`, `cycle/tests/*.cpp`, `oscillo/tests/*.cpp`.
- Naming: begin test files with `Test*.cpp` or place under a `tests/` subfolder.
- Coverage: prefer tests for new DSP modules and bugfixes; include edge‑rate/sample‑rate cases.

## Commit & Pull Request Guidelines
- Commits: short, imperative subject (“Fix buffer wraparound”), optional body for context.
- PRs: include description, linked issues, platform (Linux/macOS), and screenshots for UI changes.
- CI/local proof: show build preset used and `ctest` output when adding tests.

## Runtime & Configuration Tips
- `.env` is mandatory at configure time; CMake reads it to locate SDKs.
- Linux uses Intel IPP; macOS uses Accelerate/vDSP. Do not allocate on audio threads—preallocate and reuse buffers (see `docs/contributing.md`).

## Tooling Tips
- `jdoc <symbol>`: Quick lookup for JUCE APIs (types, methods). It takes about 1 second to run.
- Examples: `jdoc AudioProcessor`, `jdoc AudioBuffer`.
- Use during reviews/tests to confirm semantics (thread safety, parameter ranges, ownership).
- Prefer fully qualified names for nested namespaces; match JUCE casing exactly; no template arguments.
