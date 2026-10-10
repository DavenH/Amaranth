# Surface programs 13 / 14

Status: Implemented

Authoritative appearance: the user's downloaded recipes 13 and 14 and the Python
lab evaluator. Preserve immutable copies as fixtures. Compile their OKLab gradients
to linear RGB lookup data using that evaluator, not a second OKLab implementation.
Shared C++ program evaluator owns cached Gaussian Y band-pass and compositing using
Buffer operations. Gaussian coefficients use sigma/truncate=4 and reflected padding
matching SciPy; existing box blurs and FFT audio convolution do not implement this
boundary/kernel contract. No copied audio/mesh logic.

Render the complete colour product only when existing scalar cache invalidates;
existing GL renderer presents packed RGBA for these programs. CPU fallback consumes
the same product. Program identity participates in upload invalidation. Existing
materials are unchanged. Signal Spy uses peak normalization without soft clipping
for these programs; authored mesh amplitude stays unchanged. UI exposes two appended
styles in V1/V2 with existing persistence and refresh ownership. No realtime work.

Complexity O(pixels * bounded Gaussian support), storage O(pixels); no work per
presentation frame except texture draw. Initial CPU cached implementation is deliberate,
not a copied shader; no parallel V1/V2 renderer. Existing large material/GL files get
delegation branches only, policy and numerical operations live in the new program
module. Target <=3 channel error versus Python at native resolution, matching CPU/GL
presentation. Verify recipe fixtures, cache style switches, existing surfaces and
Stengah. Completion requires builds, focused tests, architecture audit and visual proof.

## Completed implementation and review

Both View > Time Surface Colour menus expose Icy-hot 13 / 14. New style indexes
are appended, preserving stored old indexes. The same cached program product is
used by V1 GL and V2 compact/expanded material paths. Shader-disabled V1 presents
the cached RGB texture through fixed-function GL; V2 CPU images use the same core.
No general JSON runtime or Python dependency was added to Cycle. Source JSON and
golden PNGs are checked in; the generated palette is immutable numeric data.

Responsibility review: ScalarSurfaceMaterial.cpp (976 → 1000 lines) retains material
selection/evaluation but delegates the new program math. GLScalarSurfaceRenderer.cpp
(870 → 890) owns context/upload/presentation; the new branch presents cached colour,
not a second implementation. NodePreviewRenderer.cpp (1067 → 1073) owns profile
routing and supplies Signal Spy role/domain facts to the existing normalization
service; recipe mode bypasses only soft clipping. Main.cpp (561 → 583) and
CycleV2Automation.cpp (891 → 901) translate UI/automation selection to the existing
workspace style setter. None owns Gaussian or blending algorithms. Shared program
module is 128 lines, header 13. Generated palette header is 2058 lines of data,
not a composition boundary requiring extraction. Architecture audit re-run and
size triggers reviewed; no new duplicate lifecycle policy. Pixel transforms use
Buffer; padding/lookup/write loops contain no scalar std math. clang-tidy unavailable.

## Verification

- `cmake --build --preset tests --parallel 10` and standalone-debug build passed.
- Shared surface/program suite: 22 cases, 1638 assertions passed.
- V2 production Signal Spy route: 9 assertions passed against Python golden PNGs;
  source remains unchanged. Native-resolution tolerance is <=3/255 per channel.
- V1 automation selects both menu options; both diagnostics report zero GL errors.
  GPU validation (including both programs) passed with maximum channel error 2.
- V2 automation selects both styles, captures compact Stengah, opens expanded Organ
  mesh and recreates the canvas context (create 1→2, close 0→1). All commands pass;
  GPU validation passes with maximum channel error 2 before/after recreation.
- Shader-disabled V1 automation passes both style switches with zero GL errors.
- Inspected CPU production Stengah images `/tmp/cycle-program-13.png` and
  `/tmp/cycle-program-14.png`, plus compact app captures. These are faithful to the
  saved recipes, not a claim of matching the older generated artistic reference.
- Native OS capture couldn't foreground Cycle. App-side canvas captures omit the
  GL background, so they establish compact preview appearance only, not full-panel
  GL screenshot parity. Live GL diagnostics/parity cover the GL path numerically.

Artifacts: `/tmp/cycle-icy-hot-report.json`, `/tmp/cycle-v2-icy-hot-report2.json`,
`/tmp/cycle-icy-hot-fallback-report.json`; corresponding logs use matching stems.
The known saved-default-preset FileManager.cpp:174 assertion recurred at startup
before CalmingKeys loaded; tracked in ui-bugs.md, unrelated to these options.
No remaining implementation slice or deletion target.
