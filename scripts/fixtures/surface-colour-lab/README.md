# Stengah surface fixture

`stengah-b0-spy1.f32` is the 512 × 512 uncoloured scalar export from
`cycle-v2/tests/TestScalarSurfaceReference.cpp`, captured during the Stengah B0
Spy #1 material investigation. It is the `impulseResponse.time` preview, not a
reconstructed height map inferred from a screenshot.

The binary has little-endian int32 columns/rows followed by column-major float32
samples, bottom-up Y. Surface Colour Lab converts orientation once during import.
The fixture is kept alongside the tool so experiments need no Cycle build.

`program-13.json` and `program-14.json` preserve the user's final recipes. Their
PNG files are lab-rendered native-resolution reference outputs, regenerated with:

```sh
scripts/run_surface_colour_lab.sh --recipe scripts/fixtures/surface-colour-lab/program-13.json --output scripts/fixtures/surface-colour-lab/program-13.png
scripts/run_surface_colour_lab.sh --recipe scripts/fixtures/surface-colour-lab/program-14.json --output scripts/fixtures/surface-colour-lab/program-14.png
```

Cycle exposes these as Icy-hot 13 / 14. `scripts/compile_surface_programs.py`
emits an apply_patch for the compiled linear-RGB palette data using the lab's
OKLab evaluator. Regenerate that header if the authored palette changes.
