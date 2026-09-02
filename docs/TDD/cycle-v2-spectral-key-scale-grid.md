# Cycle V2 Spectral Key-Scale Grid

Status: In progress

## Objective

Make the expanded spectral Trimesh surface use the modulation axis assigned to
key scale. The preview MIDI note must place the displayed slice on that axis,
and selecting that axis as the primary view must make the logarithmic spectral
grid span the keyboard pitch range from its first through last column.

## Authoritative Implementations

- `PreviewPitchResolver` owns the existing Cycle V2 preview-note convention and
  keyboard endpoint handling. The note remains sourced exactly as it is today.
- `buildModulationTripleConfiguration` owns modulation-source defaults and
  parsing, while `ModulationSource::normalizeKey` owns MIDI-note to
  unit-position mapping for key scale.
- Cycle v1 `VisualDsp::resizeArrays` and `Spectrum3D::willAdjustSurfaceColumns`
  establish the visual contract: spectral columns carry the current preview
  pitch unless key scale is the primary view axis, in which case their pitches
  span `Constants::LowestMidiNote` through `Constants::HighestMidiNote`.
- Mature `Panel3D::drawLogSurface` already consumes each `Column::midiKey` and
  applies its pitch-dependent logarithmic mapping. Cycle V2 must supply the
  missing semantic column metadata rather than reproduce that rendering.

## Design

Extend preview-pitch resolution with the key-scale axis declared by the
attached Modulation Triple. `NodePreviewResources` passes that presentation
context to `TrimeshWidget`.

At the widget boundary, create a presentation-only node copy whose assigned
key-scale morph parameter is replaced with the normalized preview pitch. The
durable graph and its parameters remain unchanged. The bridge passes the axis
identity to `TrimeshPanelDataSource`; for spectral rendering with key scale as
the primary view axis, the data source labels its existing panel columns with
the MIDI note represented by each inclusive unit position. All other columns
retain the preview note.

This is a narrow presentation adapter. It translates graph modulation metadata
and preview pitch into mature Trimesh morph and `Panel3D` column inputs. It does
not copy pitch normalization, logarithmic mapping, rasterization, or DSP.

## Test-First Contract

1. Preview context resolves key scale on at least two modulation axes while
   preserving the existing preview note.
2. Notes at the keyboard endpoints normalize through
   `ModulationSource::normalizeKey`, and two ordinary notes move the effective
   morph position on the assigned axis only.
3. A spectral surface whose primary axis is key scale labels its first and last
   columns with the inclusive keyboard endpoints.
4. Reassigning key scale changes which primary axis produces pitch-spanning
   columns; a non-key-scale primary axis keeps every column at the preview note.
5. Time-domain surfaces retain uniform preview-note metadata.

## Negative Boundaries

- Do not mutate `NodeGraph`, publish commands, or persist preview-derived morph
  values.
- Do not duplicate MIDI normalization or logarithmic grid mapping.
- Do not change Trimesh DSP, FFT resolution, mesh traversal, or panel painting.
- Do not retain the Cycle v1 assumption that key scale is always red.

## Completion Criteria

- Focused resolver, widget, data-source, and rebind tests pass.
- A production fixture opens a magnitude-spectrum Trimesh editor and confirms
  its OpenGL panels remain live after preview-context changes.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review are complete.

## Implementation Evidence

- Preview resolution now carries the key-scale axis parsed by the authoritative
  Modulation Triple configuration builder. The existing preview-note source is
  unchanged, and the bridge uses `ModulationSource::normalizeKey` for the
  presentation-only morph position.
- Spectral panel columns now carry inclusive MIDI endpoints `20` and `127` when
  key scale is primary, with pitch-dependent sizes supplied to the mature
  `Panel3D::drawLogSurface` path. Other primary axes and time-domain panels keep
  uniform preview-note metadata.
- Focused tests pass 21 assertions across resolver, note/axis rebinding, and
  endpoint behavior. The production Stengah fixture passes all commands,
  asserts the two MIDI endpoints after switching the magnitude mesh to red,
  and reports both hosted OpenGL panels visible, showing, and non-empty.
- Standalone Debug builds. The complete Cycle V2 suite passes 10,696 of 10,697
  assertions; the sole failure is the pre-existing edge-hover help assertion in
  `TestNodeCanvasHitRouter.cpp:66`.
- OS-pixel inspection remains unavailable because this agent session lacks
  macOS Screen Recording permission. Keep this TDD in progress until the
  pitch-warped grid receives that final production-size visual review.
