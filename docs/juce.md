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

## Capabilities already available

Before adding a generic utility, search both JUCE and the semantic routes in
`docs/code-map.md`. The configured modules already provide:

| Area | JUCE facilities already included | Project preference |
| --- | --- | --- |
| Core/data | strings, files, streams, JSON/`var`, properties, time, UUIDs | Reuse JUCE types at OS/UI boundaries; preserve established model types. |
| Events/threads | message dispatch, timers, async updates, locks, threads, thread pools | Keep realtime work lock/allocation-free and use existing project schedulers. |
| Graphics | colours, images, paths, fonts, transforms, gradients, image codecs | Reuse JUCE drawing primitives and existing Amaranth renderers. |
| GUI/input | components, controls, menus, dialogs, keyboard/mouse, viewports | Check shared widgets and product interaction owners before wrapping controls. |
| Audio I/O | device management, callbacks, MIDI, buffers, format readers/writers | Reuse app audio-engine boundaries; do not open devices from domain nodes. |
| Plugins | processors, parameters/state, host integration, plugin client shells | Preserve Cycle's existing processor and generated-client wiring. |
| DSP | FFT, filters, convolution, oversampling, interpolation, decibel helpers | Prefer Amaranth `Buffer`/`VecOps` and mature DSP wrappers in hot paths. |
| OpenGL | contexts, textures, framebuffers, and shader integration | Reuse the established panel/canvas GL hosts and lifecycle. |
| Optional modules | OSC in Cycle, cryptography in AmaranthLib, animation in Cycle | Use only where the target already links the module. |

JUCE supplies framework mechanics, not Amaranth product semantics. Existing
Amaranth mesh, rasterization, envelope, interaction, graph, preview, and audio
implementations remain authoritative. A JUCE primitive may replace generic
plumbing; it should not be used to create a second product behavior path.

### Reuse check before writing framework-like code

1. Search the current target for the needed concept and existing wrapper.
2. Check `JuceHeader.h` for module availability in that target.
3. Search the corresponding JUCE module API before creating a container,
   thread helper, timer, file/JSON helper, menu, dialog, image codec, audio
   format adapter, MIDI helper, FFT, filter, convolution, or OpenGL wrapper.
4. If project code adds product policy around JUCE, extend that owner instead
   of bypassing it with a direct parallel use.

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

* **Build targets** (`Debug`) after edits.
* On macOS, verify that `.mm` sources are compiled as Objective-C++ (already set in this CMake; don’t remove).
* Ensure JUCE macros (`JUCE_MODULE_AVAILABLE_*`, `JUCE_APP_VERSION`, etc.) remain defined as in CMake.
