# Surface Colour Lab

Status: Implemented

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
