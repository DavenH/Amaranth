# JUCE folder layout & generated code (read this before refactors)

**Context:** This project uses **JUCE**. The `lib/JuceLibraryCode/` and `cycle/JuceLibraryCode/` folders contains **generated amalgamation files** that compile JUCE modules and plugin clients as part of this project. These files are intentionally committed for reproducible builds and fast setup.

## What’s inside `*/JuceLibraryCode/` and why it exists

* **`include_juce_*.cpp` / `.mm` / `.c`:** Single-translation-unit “amalgamations” that pull in the selected JUCE modules (audio, GUI, DSP, etc.).

    * `.mm` files are **Objective-C++** and exist for macOS frameworks; they must compile with `-x objective-c++` (already handled in CMake).
* **Plugin client files** (e.g., `include_juce_audio_plugin_client_*`): Stubs/wrappers JUCE uses to build different plugin formats (VST2/3, AU, AAX, LV2, Standalone, Unity). They may build conditionally by platform/config, but are kept here so cross-target builds don’t require regenerating them.
* **`JuceHeader.h`:** The canonical include that wires up module headers and project macros. Code in this repo should include JUCE via:

  ```cpp
  #include "JuceHeader.h"
  ```
* **`ReadMe.txt`:** Projucer/JUCE notes about the generated folder.

## How CMake wires it up here

* `CMakeLists.txt` sets:

    * `JUCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode`
    * Recursively adds all `*.cpp`, `*.c`, and (on Apple) `*.mm` from `JuceLibraryCode`.
    * On Apple, `.mm` files are given **Objective-C++** compile flags (don’t remove).
* JUCE module availability and app settings are expressed via a set of `JUCE_*` **compile definitions** already present in `CMakeLists.txt`. Don’t rename or “simplify” these unless you know the JUCE build graph.

## Rules for the agent (very important)

**Do:**

* Treat `**/JuceLibraryCode/` as **generated/vendor code**.
* Leave filenames, include paths, and compile flags as-is.
* Include JUCE via `JuceHeader.h` (not by reaching into `modules/` paths directly).
* Keep plugin client includes even if a single local build doesn’t use them; they’re needed for other targets.

**Don’t:**

* **Don’t refactor, reformat, or move** files under `**/JuceLibraryCode/`.
  (No symbol renames, no header shuffles, no “dead code” deletions here.)
* **Don’t apply clang-tidy/format** to that folder.
  If you’re bulk-editing, exclude it explicitly.
* **Don’t convert `.mm` files to `.cpp`** or remove Objective-C++ flags.
* **Don’t rename `JuceLibraryCode/`** or `JuceHeader.h`; many includes depend on this path.

## If changes to JUCE are actually needed

* Prefer **adjusting compile definitions** or **module selection** in CMake.
* If you must modify JUCE itself, do it in a separate patch and clearly annotate why (e.g., bug workaround). Consider upstreaming and keep a minimal diff.

## Quick checks the agent should run

* **Build all targets** (`Debug` and `Release`) after edits.
* On macOS, verify that `.mm` sources are compiled as Objective-C++ (already set in this CMake; don’t remove).
* Ensure JUCE macros (`JUCE_MODULE_AVAILABLE_*`, `JUCE_APP_VERSION`, etc.) remain defined as in CMake.
