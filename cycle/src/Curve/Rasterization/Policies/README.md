# Cycle Rasterization Policies

Cycle-specific policies are separated from the shared Amaranth library policies:

- `Graphic/`: translation of Cycle layer groups, morph-axis settings, panel
  state, and scratch-channel state into explicit shared rasterizer inputs.

Cross-product policies, including voice slicing and chaining, belong in
`lib/src/Curve/Rasterization/Policies/`.
