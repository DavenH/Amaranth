# Surface Colour Lab

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
3. Adjust gain, offset, opacity and blend. **Detail energy** fades zero detail to
   transparency continuously; without it, zero detail paints the midpoint colour.
4. Compare with raw mapping or inspect **Selected filter field**. Disable a layer
   to evaluate its contribution. Disable comparison for a larger composite.
5. Save a JSON recipe and export a PNG. Reload recipes to revisit experiments.

The reference uses the first layer's gradient with unit gain and no filters.
Input scaling and interpolation space are shared by both views. The selected-field
view shows the filter after gain/offset, before its gradient and opacity mask.
Clipping statistics report values outside the palette range.

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
