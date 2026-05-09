# Rasterization Policies

Policy headers are grouped by the domain they govern:

- `Core/`: generic rasterizer state, cleanup, padding, scaling, sorting, and restriction.
- `Curves/`: curve resolution and waveform preparation/baking.
- `Mesh/`: mesh slicing, guide curves, wrapping, and hidden-dimension projection.
- `Envelope/`: envelope marker, sustain, loop, release, timing, and validation rules.

Cycle-only policies live under `cycle/src/Curve/Rasterization/Policies/`.
