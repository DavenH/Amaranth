# FXRasterizer `setMesh` Audit

TDD: `docs/TDD/rasterization-pipeline-migration.md`, Phase 7.

Phase 7 keeps `FXRasterizer::setMesh(Mesh*)` as a compatibility adapter, but
the rasterizer now consumes a lightweight vertex list internally. These call
sites can migrate to a direct point/vertex provider after the effect owners have
a stable mesh-lifecycle API.

| Call site | Current role | Migration classification |
| --- | --- | --- |
| `cycle/src/UI/Effects/WaveshaperUI.cpp` | Pushes the selected waveshaper mesh into the local FX rasterizer, or clears it when the effect panel is inactive. | Compatibility adapter. Candidate for direct vertex-list provider from the selected effect mesh. |
| `cycle/src/UI/Effects/IrModellerUI.cpp` | Pushes the selected impulse-response mesh into both the UI rasterizer and `IrModeller`. | Compatibility adapter. Candidate for direct provider shared by UI/audio effect rasterizers. |
| `cycle/src/Audio/Effects/IrModeller.cpp` | Mirrors the UI mesh into `audioThdRasterizer`, including the current `setUI` repair path. | Compatibility adapter. Candidate for an audio-safe point snapshot so the audio rasterizer no longer observes `Mesh` lifetime. |
| `cycle/src/UI/VertexPanels/EffectPanel.h` | Owns the local `FXRasterizer` facade used by effect panels. | Facade remains useful; backing source can become direct vertices or immutable point snapshots. |
| `cycle/src/UI/VertexPanels/GuideCurvePanel.cpp` | Uses `EnvWavePitchRast`, currently a typedef of `FXRasterizer`, to rasterize guide-curve meshes into audio tables and panel previews. | Compatibility adapter. Candidate for direct vertex-list table rasterization once guide curves have a point-source owner. |
| `cycle/src/App/tests/TestPresetRoundTrip.cpp` | Restores current preset meshes into effect rasterizers for serialization regression coverage. | Compatibility adapter until the UI/effect owners expose direct point sources in tests. |
| `lib/src/Audio/PitchedSample.cpp` | Calls the generic `MeshRasterizer::setMesh` on voice/sample rasterizers, not the FX specialization. | Out of scope for Phase 7. |

Removal criteria for the compatibility adapter:

- effect UI code can pass a direct vertex list or point snapshot without first
  materializing a `Mesh*`,
- `IrModeller` audio-thread rasterization no longer depends on UI rasterizer
  mesh ownership,
- preset round-trip fixtures are updated to bind effect point sources directly,
- focused AfricanHorn -> Icycle automation remains assertion-free.
