# Typed Rasterization Commands

## Status

Proposed.

## Problem

Rasterizers expose mutable requests, parallel field setters, mutable morph
references, stored raw mesh/provider pointers, and overlapping verbs whose
publication side effects are not evident (`render`, `updateWaveform`,
`renderWaveformOnly`). `RasterizationRequest` also contains execution modes and
legacy fields rather than being solely a render description.

## Goals

- Make complete render inputs immutable for one operation.
- Distinguish geometry rendering, waveform rendering, and publication by type.
- Make borrowed mesh and provider lifetimes explicit.
- Remove dead, legacy-only, and domain-specific request fields.
- Preserve a narrow compatibility facade for Cycle v1 `Updateable` wiring.
- Expose values that vary per render as command inputs while retaining stable
  collaborators and saved references as object state.

## Target Design

```cpp
RenderResult& renderGeometry(const GeometryRenderCommand& command);
RenderResult& renderWaveform(const WaveformRenderCommand& command);
void publish(const RenderResult& result, const PublicationMetadata& metadata);
```

Commands contain all required input and do not mutate stored ambient request
state. Domain-specific commands may compose a common slice request without
turning the common type into a switchboard.

This is not a rule to flatten every object into a long parameter list. A mesh,
morph position, phase, axis, or bounds that vary between calls belong in the
command. A stable provider, cache, allocator, or service reference that defines
the renderer instance may remain a member.

## Semantic Tests

- Reusing a rasterizer with two commands cannot leak omitted state from the
  first command into the second.
- Rendering never changes publication unless `publish` is called.
- Geometry-only commands cannot accidentally request waveform work.
- Invalid borrowed inputs fail at the command boundary.
- Compile-time checks prevent mutation through a submitted command.

## Completion Criteria

- Production callers do not obtain mutable `RasterizationRequest&`.
- Operation names state whether publication occurs.
- `calcInterceptsOnly` and legacy `paddingSize` are absent from the common
  request.
