## Curves

## High-Level Pipeline

```mermaid
flowchart LR
    mesh[mesh + trilinear morph position]
    int1[points]
    int2[points]
    int3[points]
    curves[curves]
    path[path]
    wave[waveform]

    mesh --[interpolate]--> int1
    int1 --[pad]--> int2
    int1 --[padCyclic]--> int2
    int1 --[padChaining]--> int2
    int2 --[deform]--> int3
    int3 --> curves
    curves --> path
    path --[sample]-->  wave
```

Mesh Raserization Refactor.

## Class Diagram (Desired, post-refactor)

CurvePiece becomes a 'sampleable' union of a CurvePath or a Curve

### Rasterizer2D becomes 

```mermaid
flowchart LR
    mesh["vector[Vertex2]"]
    int1["vector[Intercept]"]
    int3["vector[Intercept]"]
    curves["vector[CurvePiece]"]
    wave[waveform]

    mesh --[SimpleInterpolator]--> int1
    int1 --[PaddingPointPositioner]--> int3
    int3 --[SimpleCurveGenerator]--> curves
    curves --[SimpleCurveSampler]--> wave
```


### EnvRasterizer becomes

```mermaid
flowchart LR
    mesh["EnvelopeMesh[TrilinearVertex]"]
    int1["vector[Intercept]"]
    int3["vector[Intercept]"]
    curves["vector[CurvePiece]"]
    wave[waveform]

    mesh --[TrilinearInterpolator]--> int1
    int1 --[EndPointPositioner]--> int3
    int3 --[PathCurveGenerator]--> curves
    curves --[SimpleCurveSampler]--> wave
```


### VoiceMeshRasterizer becomes

```mermaid
flowchart LR
    mesh["Mesh[TrilinearVertex]"]
    int1["vector[Intercept]"]
    int3["vector[Intercept]"]
    curves["vector[CurvePiece]"]
    wave[waveform]

    mesh --[TrilinearInterpolator]--> int1
    int1 --[ChainingPointPositioner]--> int3
    int3 --[PathCurveGenerator]--> curves
    curves --[SimpleCurveSampler]--> wave
```


### GraphicRasterizer becomes

```mermaid
flowchart LR
    mesh["Mesh[TrilinearVertex]"]
    int1["vector[Intercept]"]
    int3["vector[Intercept]"]
    curves["vector[CurvePiece]"]
    wave[waveform]

    mesh --[TrilinearInterpolator]--> int1
    int1 --[CyclicPointPositioner]--> int3
    int3 --[PathCurveGenerator]--> curves
    curves --[SimpleCurveSampler]--> wave
```