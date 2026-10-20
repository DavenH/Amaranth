# Direct Icy Hot surface recipe

Status: Implemented.

The supplied `surface-recipe(21).json` and screenshot are authoritative. Rendering
that recipe against the saved Stengah B0 Spy grid reproduces the supplied image:
raw peak-normalized signal, gain 1.3, OKLab cyan/purple/gold palette, no detail.
Existing Icy Hot is recipe 13, a greyscale base plus band-pass screen overlay;
it is a different treatment with weaker broad colour. No lighting is missing.

Preserve recipe 21 verbatim and compile its palette through the lab evaluator.
Extend the existing shared program descriptor with disabled-detail support;
skip filtering/compositing for this raw program. Expose Icy Hot (Raw) in the
shared style catalog, append stable IDs, and preserve prior saved selections.
The shared evaluator owns mapping; existing CPU/GL presentation and preset
metadata own caching, display and persistence. No new renderer or DSP changes.

Normalization is a separate contract: compact time Spies peak-normalize, authored
Trimesh does not, and comparison cards apply fixed source gains. Do not silently
change these policies to match a single peak-normalized reference.

On the Stengah fixture, peak is 0.528291, so the lab/compact Spy uses 1.892895x
display gain. Under recipe 21, 34.1% of samples reach beyond palette coordinates
0.35–0.65; with unity gain only 11.6% do. Comparison output Spy also removes its
known source gain of 3. Thus selecting the same palette does not establish the
same input mapping. An explicit display-contrast choice would be a separate
product decision; this change preserves the established true-level comparison.

Verify all recipes 13–21 against lab PNGs, including production Spy mapping;
verify cache switching and native GL presentation, then inspect an app capture.
Raw evaluation is O(pixels) only on scalar/style invalidation; it must not run
Gaussian filters or allocate/work on the audio thread. No legacy path deletion
is required: the existing choices remain independently authored recipes.

Responsibility review: ScalarSurfaceProgram.cpp 155 -> 163 lines remains the
cached palette/filter/composition owner; the disabled-detail branch reuses its
existing palette and packing functions. ScalarSurfaceMaterial.cpp 1052 -> 1054
only extends selection, and GLScalarSurfaceRenderer.cpp 901 -> 902 only extends
its parity validation list. No algorithms or lifecycle responsibilities are
added to either large file. The 2,049-row generated LUT is numeric data from
the Python lab, not a new colour evaluator. No Cycle V2 production file changes.

Verification (2026-10-19):

- Built Cycle, CycleV2, AmaranthLib_tests and CycleV2_tests with parallelism 10.
- Shared surface suite: 25 cases, 2,709 assertions pass, including every pixel
  of recipes 13–21 against lab references (maximum allowed error 3/255).
- Production Spy mapping and preset presentation: three cases, 98 assertions
  pass, including the new recipe and both old/new Icy Hot saved selections.
- Python lab: 27 tests pass. Architecture audit and code-map check complete;
  production diff/style and whitespace checks pass.
- Cycle 1: eight live commands pass, including menu selection and save/reload.
- Cycle 2: 26 live commands pass, including B0 Spy detail, save/reload, context
  recreation and expanded time mesh. Inspected `.context/icy-hot-raw-canvas.png`:
  the reference's broad gold/cyan bands and highlights are reproduced in-app.
- Exact built CycleV2 with GPU validation enabled: all 18 material comparisons
  pass at maximum channel error 2/255, before and after context recreation.
  Log: `.context/raw-surface-gpu.log`; fixture/report in the same directory.
- OS capture is blocked by macOS Screen Recording permission; no OS screenshot
  claimed. App-side capture proves the Spy image. Its incidental sidebar overlap
  is recorded in `ui-bugs.md`.

The attached recipe was absent from the shipped catalog, rather than incorrectly
implemented by the existing Icy Hot style. It is now available as Icy Hot (Raw)
in both apps, using the established per-preset selection. Comparison fixed-gain
mapping remains a documented independent difference, not a hidden style change.

Follow-up: the user selected recipe 21 as the time-domain default in both apps.
`TimeSurfaceStyles::defaultStyle` owns that choice for initial material selection,
ID/index fallback and Cycle 1 document settings. Existing explicit preset styles
retain their meaning; missing fields reset to recipe 21 instead of inheriting
the prior preset. No renderer, normalization, audio or spectral-default changes.
Both standalone apps and focused test targets rebuilt successfully. Shared
surface/settings checks pass 2,715 assertions in 27 cases; Cycle V2 program/
persistence checks pass 98 assertions in three cases. Existing style fixtures
pass 16 Cycle 1 commands and 31 Cycle V2 commands with the new missing-field
default, saved-choice preservation and context recreation. Audit, code-map and
whitespace checks pass. Material evaluation remains in its existing owner;
this follow-up adds only a shared default constant and selection routing.
