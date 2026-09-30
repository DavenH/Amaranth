# Preset time-surface styles

Status: implemented.

## Contract and ownership

The Surface Colour Lab JSON recipes and its OKLab evaluator are authoritative.
The existing shared ScalarSurfaceProgram owns cached, vectorized CPU filtering
and linear-RGB screen composition; GL presents that RGBA product unchanged.
Extend its recipe descriptors and generated palettes, not the shader algorithm.
Recipes 13–20 remain reproducible fixtures. Canonical names are Greyscale,
Icy Hot (13), Blues (Cycle 1), and Bullion (20). Recipes 15–19 are experimental;
retain 14 and older styles as legacy choices. Stable style IDs must not depend
on menu ordering. Greyscale is exactly value -> equal sRGB channels, no effects.

Preset selection belongs to document presentation, not application preferences
or DSP state. V1 uses document settings; V2 uses PresetPresentation via its
command boundary. Missing or unrecognized selections resolve to Blues without
inheriting the last opened preset. Loading must not dirty the document.
Selection changes are O(1), with only local cached render products invalidated.
Filtering remains O(pixels * bounded Gaussian support), outside the audio thread,
only when scalar data or program changes. Authored mesh amplitude stays intact;
Spy normalization remains the existing separate display-only boundary.

## Slices and verification

- [x] Shared style catalog, recipes 15–20, exact cached program evaluation.
- [x] Canonical and experimental menus in both applications.
- [x] Preset save/load and missing-field defaults in both applications.
- [x] Lab/native pixel parity (3/255 channel tolerance), flat greyscale,
      cache switching, persistence and live preset switching checks.
- [x] Refactor/style review, builds, architecture audit.

Selection follows existing preset presentation metadata semantics (not graph
undo). Presentation has a separate dirty revision, so undoing a graph edit
cannot accidentally mark a changed style saved. No graph/audio publication or
full presentation/preview-image copy occurs on selection. V1 document loads
synchronize style immediately on the message thread, or schedule through the
existing async UI updater for host/plugin-state loads on another thread.
No filtering or gradient rebuilding occurs in the off-thread callback.
The optional missing-section Savable hook resets only explicitly opted-in
settings; unrelated legacy document settings retain their previous behavior.

## Responsibility review

ScalarSurfaceMaterial.cpp (~1,000 lines) remains material/evaluation ownership;
recipe mechanics stay in the existing 128-line ScalarSurfaceProgram.cpp.
Main.cpp (~580 lines) and SynthMenuBarModel only route a shared catalog; remove
their duplicated per-style switches. NodeCanvas (~2,730 lines) receives only
document-style synchronization and command delegation, no rendering/serialization
algorithms. PresetPresentationCodec owns the optional field; no graph/DSP schema
or large GraphSerializer extension is needed. Generated LUT size is data, not
additional policy. Existing global-style preference writes are deletion targets.

After review: Main.cpp 583 -> 460 lines, SynthMenuBarModel 481 -> 432;
shared TimeSurfaceStyles owns labels, IDs, menu grouping and fallback. The two
per-client style switches and global preference writes are deleted.
ScalarSurfaceProgram 128 -> 155 lines; generated descriptors replace hardcoded
13/14 branches. Palette lookup, alpha shaping, screen composition, and packing
are separate small functions. Six deduplicated generated LUTs explain the large
data-header diff; no additional rendering policy is hidden there.

NodeCanvas 2731 -> 2742 lines remains a composition root; added lines only
delegate selection and synchronize the shared renderer. NodePreviewRenderer
1073 -> 1075 only chooses the existing linear display normalization for plain
Greyscale. CycleV2Automation 901 -> 909 replaces bespoke 13/14 entries with the
same catalog. These pre-existing large classes remain explicit extraction
candidates; this change adds no node-kind policy or domain algorithms.
GraphDocument (175 lines) owns saved presentation revision; codec (177 lines)
owns serialized metadata; dispatcher (523 lines) owns mutation eligibility.

## Verification (2026-09-29)

- Presets: tests and standalone-debug, builds use --parallel 10.
- Shared material/settings: 26 cases, 2701 assertions pass.
- V2 production Spy/metadata/presentation: 18 cases, 251 assertions pass.
- Focused CTest: seven tests pass, including production Spy parity and graph
  commit/cancel regressions.
- Lab comparison includes every pixel of Stengah B0 Spy 1 for recipes 13–20,
  through both the shared evaluator and production Signal Spy path (<=3/255).
- Greyscale exact 256-step ramp including boundaries; switching each recipe
  invalidates cached products. Spectral legacy-palette test remains unchanged.
- V1 live fixture: 16 commands pass, save/change/reload, old-preset Blues,
  and every experimental menu entry.
- V2 live fixture: 31 commands pass, save/change/reload, default reset, variants,
  CPU preview capture, and context recreation. Same fixture passes with shaders
  disabled. Expanded-mesh GPU fixture: 14 commands pass; all 16 validation
  materials max channel error 2/255, including recreated context.
- Architecture audit completed, production hot loops use Buffer operations;
  no new scalar std::math in those loops. Scoped git diff --check clean.
  clang-tidy unavailable. Unrelated user edits in ui-bugs.md have pre-existing
  whitespace and were not modified.
- In-app preview inspected: /tmp/cycle-v2-canonical-icy-hot.png. Desktop capture
  helper failed its foreground check; no full desktop screenshot claimed.
  Known missing ooh-aah startup preset assertion recurred in the V1 log,
  already tracked in ui-bugs.md. Automation subsequently opened the fixture.

No recipe tuning or source/DSP changes were made. Future low-frequency tuning
of Icy Hot and Bullion remains user-authored lab work, not unfinished porting.
