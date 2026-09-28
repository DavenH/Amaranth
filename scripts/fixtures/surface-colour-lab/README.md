# Stengah surface fixture

`stengah-b0-spy1.f32` is the 512 × 512 uncoloured scalar export from
`cycle-v2/tests/TestScalarSurfaceReference.cpp`, captured during the Stengah B0
Spy #1 material investigation. It is the `impulseResponse.time` preview, not a
reconstructed height map inferred from a screenshot.

The binary has little-endian int32 columns/rows followed by column-major float32
samples, bottom-up Y. Surface Colour Lab converts orientation once during import.
The fixture is kept alongside the tool so experiments need no Cycle build.
