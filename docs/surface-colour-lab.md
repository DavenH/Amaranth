# Surface Colour Lab

Saved programs 13 and 14 are also available in Cycle 1 and Cycle 2 under
**View → Time Surface Colour → Icy-hot 13 / Icy-hot 14**. These are fixed versions
of the saved recipes, not live links to Downloads. Cycle needs no Python at runtime;
filters/compositing are cached when the scalar product changes.

A local Python/browser sketchbook for scalar-field colour experiments. No Cycle
build is needed, and nothing changes Cycle's materials, presets or source data.

## Start

From the repository root:

```sh
./scripts/run_surface_colour_lab.sh
```

The launcher creates an isolated environment under `build/surface-colour-lab-venv`
and installs NumPy, SciPy and Pillow on first use (internet required then).
It opens `http://127.0.0.1:8765`. Stop with Ctrl-C in the terminal. Use
`--port 8766` if the port is occupied, or `--no-browser` to open it yourself.
Rendering and files stay local; the UI has no CDN dependencies.

The default input is the captured **Stengah B0 Spy #1** 512 × 512 scalar grid.
Use `--demo` for sine plus ripple, `--input path/to/grid.f32` for another export,
or **Open scalar grid** in the editor.

## Experiment

1. Start with the single **Raw values** layer. Edit its colour stops or apply a
   starting palette. These are editable sketch palettes, not promises of exact
   parity with existing Cycle materials.
2. Add a layer. Choose low-pass, high-pass or band-pass and its own gradient.
   Gaussian sigma is in source pixels; axes can be X, Y or both.
3. Adjust gain, offset, opacity and blend. **Detail amplitude** alpha fades the
   mapped midpoint to transparency. With **Uniform** alpha, the midpoint paints
   its colour like any other value. New detail layers start at gain 1, not 8.
4. Compare with raw mapping or inspect **Selected filter field**. Disable a layer
   to evaluate its contribution. Disable comparison for a larger composite.
5. Save a JSON recipe and export a PNG. Reload recipes to revisit experiments.

The reference uses the first layer's gradient with unit gain and no filters.
Input scaling and interpolation space are shared by both views. The selected-field
view shows the filter after gain/offset, before its gradient and opacity mask.
Clipping statistics report values outside the palette range.
**Selected layer alpha** displays effective coverage (including opacity), black
for transparent and white for opaque; this previews the selected layer's alpha
even when that layer is disabled in the composite.

**Nearest-neighbour preview** avoids browser interpolation when evaluating fine
detail. **Aspect** only stretches presentation; PNGs retain native grid dimensions.
The supplied capture has 512 samples per axis, so zoom cannot reveal additional
source detail.

## Mapping contract

Let H be the explicitly scaled source and G(s) its Gaussian blur:

| Filter | Field |
| --- | --- |
| Raw | H |
| Low-pass | G(sigma) |
| High-pass | H − G(sigma) |
| Band-pass | G(inner sigma) − G(outer sigma) |

These are **spatial image filters**, not audio-frequency filters or amplitude
thresholds. Gaussian boundaries use reflected padding. Constant fields have zero
detail through the boundaries; nonconstant boundary shapes can still affect a blur.

Raw/low-pass coordinates are `0.5 + (field − 0.5) × gain + offset`.
High/band-pass coordinates are `0.5 + field × gain + offset`. Outside the stop
range, endpoint colours extend. Gradient interpolation supports sRGB, linear RGB
and OKLab; all layer compositing is linear RGB with final gamut clipping. Layers
run bottom to top, initially over black. No normals, shadows or lighting are hidden
in the engine. Blur results use a bounded cache across palette edits.

Detail amplitude alpha is `opacity × a`, where `a = clamp(2 × abs(mappedValue − 0.5), 0, 1)`.
It uses the same post-gain/offset value as the gradient, so positive and negative
detail have equal coverage at equal magnitude. **Alpha boost** b replaces a with
`b × a / (1 + (b − 1) × a)`: b=1 is exactly proportional; higher values lift
coverage smoothly without an early hard saturation plateau. There is no threshold.
Gain can still drive the mapped value outside 0…1; inspect the clipping statistic.
**1 − detail amplitude** inverts that boosted amplitude before multiplying by
opacity: the midpoint receives full layer opacity and the endpoints receive zero.
It is the exact coverage complement of Detail amplitude at the same boost.

Every blend is gated by this alpha. **Normal** blends the detail RGB directly.
**Colour · OKLab** uses detail hue/chroma with base OKLab lightness; **Hue · OKLab**
also keeps base chroma (achromatic detail leaves hue unchanged). Out-of-gamut
perceptual targets reduce chroma at fixed lightness/hue. Final opacity interpolation
is linear RGB, so partial blends need not retain exact OKLab lightness.
Multiply remains a darkening operation, not a hue-transfer operation. Add and
Screen lighten; detail alpha prevents them from affecting neutral-detail regions.

Version 1 recipes load with an explicit upgrade notice: the old energy mask becomes
mapped-amplitude alpha at boost 1; old mask gain is discarded. Other authored
settings remain unchanged, including any high gain. Save again to store version 2.

### Greyscale amplitude with polarity-coloured detail

Load `scripts/fixtures/surface-colour-lab/greyscale-icy-hot.json` as a starting point.
It uses a monotonic white-to-black raw base, following recipe 9's direction, with
gain 1 to avoid clipping the base. Reverse its stops for black-to-white instead.
An Icy-hot overlay uses **Colour from → Original signal** while high-pass controls
only alpha. The **Colour gain/offset** controls affect palette lookup independently
of the filtered layer's gain/offset, which still control its alpha.

This retains the original signal's polarity colours instead of colouring the sign
of its high-pass residual. Colour blend retains base OKLab lightness in the target;
Normal can reintroduce the palette's own bright highlights if preferred. Hue alone
cannot colour an achromatic base because it preserves base chroma. Older recipes
continue using **Filtered layer** unless explicitly changed. This is an experimental
starting recipe, not an exact reproduction of recipe 8 on a different background.

Input scaling choices are explicit: already-unit values (clipped to 0…1), bipolar
peak normalization, or peak normalization followed by Spy-style soft clipping
(`x = source / peak × 3; H = 0.5 + 0.5 × x / (1 + abs(x))`). They never alter
the stored source. Recipes store scaling, layers and display aspect, **not the grid**.
Use the same grid when reproducing a saved result. The server shares one loaded
grid between tabs; use separate ports for independent sources.

## Scripted rendering and data

```sh
./scripts/run_surface_colour_lab.sh --recipe surface-recipe.json --output result.png
./scripts/run_surface_colour_lab.sh --demo --output raw-demo.png
```

Cycle `.f32` exports contain two little-endian int32 dimensions (columns, rows),
followed by column-major float32 values with bottom-up Y. CSV is a numeric matrix
in ordinary top-down image row order, without headers. Inputs must be finite,
at least 2 × 2 and at most 1,048,576 samples. The engine is independently importable
from `scripts/surface_colour_engine.py` for batch experiments.

## Verification

```sh
build/surface-colour-lab-venv/bin/python -m unittest discover -s scripts/tests -p test_surface_colour_lab.py -v
```

Optional browser check, with the lab running (Playwright is not a runtime dependency):

```sh
build/surface-colour-lab-venv/bin/pip install playwright
build/surface-colour-lab-venv/bin/python scripts/tests/surface_colour_lab_browser_smoke.py
```

The browser check uses installed macOS Chrome; pass `--chrome` for another Chromium
executable. It verifies editing, disabled-layer identity, recipe reload, PNG export
and narrow layout. Its screenshot defaults to `/tmp/surface-colour-lab-ui.png`.
