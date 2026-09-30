# Surface Colour Lab

Status: Implemented

## Independent colour source (implemented)

Recipes 8/9 expose coupling between detail alpha and palette indexing. Reuse the
existing filtered coordinate for alpha, but optionally index the palette with
original scaled signal plus independent colour gain/offset. Default remains
filtered for old recipes. The engine owns this one choice; no duplicate filter
pipeline, complexity unchanged. Existing OKLab Colour blend retains greyscale-base
lightness while detail alpha admits polarity colours. Supply a recipe using the
user's updated Icy-hot stops and recipe 8's high-pass selection. Validate equal
original values receive equal palette coordinates despite different residuals,
and that changing colour source leaves alpha unchanged. Keep the rail geometry;
show the two additional controls only for Original signal.

Completed with 27 passing unit/API tests and browser verification: switching colour
source changes rendering but leaves the alpha image identical. Starter recipe uses
monotonic white-to-black base with gain 1; no claim of matching the earlier artistic
result's brightness. Screenshot `/tmp/greyscale-icy-hot-lab.png` reviews the existing
rail geometry with independent colour controls. Existing recipe files unchanged;
the palette default is updated to the user's revised stops. No pending deletion.

## Detail compositing revision (implemented)

Replace raw-field energy thresholding with alpha from mapped distance to 0.5:
`a = clamp(2 * abs(value - 0.5), 0, 1)`, optionally boosted continuously with
`b*a / (1 + (b-1)*a)`. Default b=1 is exactly proportional. The same alpha gates
every blend mode. Add OKLab Colour (preserve base lightness) and Hue (also preserve
base chroma); reuse existing OKLab transforms, reduce out-of-gamut chroma rather
than clipping channels for these modes. Cost stays O(pixels × layers), vectorized.
The engine owns mapping/alpha/blending; UI only selects parameters. Keep the existing
360px rail, direct numerical alpha-boost entry and comparison views; add an alpha
inspection view. Version 1 recipes migrate explicitly to the new alpha contract,
discarding old mask gain, while preserving authored gradients and layer settings.
Delete the old energy-mask implementation. Verify proportional/symmetric alpha,
neutral identity for every blend, hue transfer/lightness retention, migration and
browser interaction. No Cycle production changes.

Completed with shared coordinate/alpha/blend functions, versioned recipe migration,
alpha inspection and new UI blend choices. New regression tests cover proportional
alpha, all-mode neutral identity, hue/chroma/lightness, gamut reduction and masked
Add/Multiply composition. Browser smoke exercises Hue/Colour, alpha view and numeric
boost entry, then saves/reloads/exports. Inspected `/tmp/surface-colour-lab-alpha.png`
at the same 1600 × 1100 geometry. Defaults are deliberately subtle, gain 1 rather
than 8. Refactor removed duplicated coordinate mapping and the old energy mask;
pixel work remains vectorized. No remaining slice or deletion target.

Inverse amplitude option: invert the shared boosted amplitude before opacity;
do not duplicate the curve. Tests prove complementary coverage, symmetry and
midpoint/endpoint behavior. Reported asymmetric recipe used uniform alpha and
offset -0.85 with gain 17.9, not the amplitude mask; the user file stays unchanged.

Build a small offline Python/browser experiment tool, not another Cycle shader
revision. Load authoritative exported scalar grids without modifying them.
The existing Stengah reference export owns fixture generation and orientation;
the lab imports that format, not Cycle's DSP or rendering lifecycle. NumPy and
SciPy own array operations/Gaussian filtering; Pillow owns PNG encoding. This
is deliberately an experimental material engine, not a CPU/GPU parity adapter.

Separate the vectorized colour/filter engine, localhost HTTP transport, and
browser controls. Keep a bounded blur cache across colour-only edits. The UI
debounces edits and submits at most one render at a time, discarding stale
results. A 360px control rail leaves the rest for raw-reference/composite views;
stack previews on smaller windows. No implicit relief or shadow. Raw mapping
is the initial and always accessible baseline.

Support editable gradient stops; raw/low/high/band-pass layers; spatial Gaussian
sigma in pixels with axis choice; gain, offset, opacity, and optional continuous
detail masks. Blend in linear RGB and offer sRGB/linear/OKLab gradient
interpolation. Save JSON recipes and native-resolution PNG; reload recipes and
scalar grids. Recipe input scaling is explicit and never alters the source.
Include a captured Stengah fixture and analytical demo for immediate use.

Verification: constant/ramp/filter decomposition, orientation and invalid files,
colour-space round trips, raw baseline identity, layer disabling, deterministic
recipe reload, cache reuse, HTTP errors and end-to-end browser editing/export.
Do not claim visual success from core tests alone. No Cycle production change.

Completed: separate engine, localhost server, browser editor, isolated launcher,
captured fixture, JSON recipes and headless/native PNG rendering. Eighteen unit/API
tests pass. Chrome smoke checks cover adding/filtering layers, palette changes,
disabled-layer identity, recipe reload, PNG download and responsive width. Inspected
the actual 1600 × 1100 UI screenshot at `/tmp/surface-colour-lab-ui.png`; the skill's
raw/composite comparison and dedicated control rail are implemented. Example
two-layer render measured 116 ms, not a general performance guarantee.

Refactor review: the engine owns colour/filter policy; transport owns input validation
and serialized access; the browser owns editor state and stale-result suppression.
No production shader, graph lifecycle, DSP or GPU-parity policy was copied. NumPy
and SciPy perform pixel math vectorwise. No new C++ files or V2 branches; architecture
audit is not applicable. There are no production deletion targets or pending slices.
