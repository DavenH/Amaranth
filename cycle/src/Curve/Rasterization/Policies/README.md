# Cycle Rasterization Policies

Cycle-specific policies are separated from the shared Amaranth library policies:

- `Graphic/`: graphic mesh axis and morph-position behavior.
- `Voice/`: voice slicing, chaining, curve resolution, and waveform update behavior.

Cross-product policies belong in `lib/src/Curve/Rasterization/Policies/`.
