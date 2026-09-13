# Cycle V2 Guide Sprite and Legacy Mod Cleanup

Status: complete

## Scope

- Keep Curve Guide shelf sprites visibly rendered across repeated factory-preset changes.
- Remove the `legacyEnvelopeMorph` constant-zero modulation source and its Envelope
  red/blue routes from factory graphs and conversion tooling.
- Keep Curve Guide hover tethers anchored to the hovered tile even when serialized
  shelf-order metadata is stale.
- Increase the keyboard transport row by approximately 30% and use its full height
  for a subtle progress fill.
- Make compact and expanded spectral-phase Trimesh surfaces use one colour map.

## Technical design

### Guide previews

The authoritative preview renderer remains `CurvePanelHost`; `GuideCurveShelf`
only owns document presentation state and schedules captures. A capture is complete
only after `CurvePanelHost` publishes an image containing visible panel content.
Framebuffer reads with no visible content are transient failures, commonly caused
by preset-switch timing, and must remain pending for a later OpenGL frame. The
failed attempt still dirties the capture area, so the canvas background must be
restored after every attempt.

This keeps each retry O(1) per pending guide and does not copy graph or mesh state.
The completed implementation adds no adapter or duplicated rasterization logic.

Guide shelf painting and hit testing use the graph's current guide-vector order.
Relationship tethers must resolve the hovered guide ID back to that same vector
index; serialized `shelfOrder` is metadata, not a second presentation ordering.

### Keyboard transport and spectral phase

The transport remains owned by `PerformanceKeyboardPanel`. Its progress bounds
match the taller transport row, with a low-alpha fill behind the centered play
button.

`TrimeshRenderProfile` remains authoritative for surface scaling and colour. The
CPU compact renderer samples `TrimeshSurfaceStyle::colourForValue`; the expanded
OpenGL renderer consumes `gradientImage`. Spectral phase therefore needs a gradient
generated from that same phase-colour function rather than the magnitude gradient.

### Legacy modulation cleanup

`port_cycle_v1_preset.py` is the authority for newly converted factory graphs, and
`migrate_cycle_v1_v2_envelope_ownership.py` owns the corresponding in-place
migration. The previous parity decision materialized Cycle 1's stuck zero Envelope
morph as a `legacyEnvelopeMorph` node. Product direction now supersedes that
compatibility behavior: Envelope modulation uses the normal implicit Voice Context
mapping unless an authored source is explicitly connected.

The migration removes `legacyEnvelopeMorph` and every incident edge. The converter
does not create them. The ownership audit treats their presence as stale content.
All factory graphs are migrated through the authoritative serializer. This also
materializes the current Trimesh defaults, including the previously implicit
`gain` parameter, so the checked-in graphs remain canonical.

## Verification

- Snapshot-cache regression coverage rejects and retries empty captures.
- A focused preset-switch fixture verifies guide sprites finish rendering.
- Converter, migration, audit, serializer, and audio-executor tests reject legacy
  nodes and routes.
- Factory-content audit reports zero `legacyEnvelopeMorph` nodes or incident edges.
- A production-size screenshot verifies Curve Guide tiles after repeated preset
  changes.
- Rendering tests verify stale shelf metadata cannot move a hover tether and the
  phase OpenGL gradient agrees with CPU colours.

## Completion criteria

- [x] Empty guide captures are never accepted as completed snapshots.
- [x] Repeated preset changes leave every visible guide sprite rendered.
- [x] Conversion and migration tooling cannot recreate legacy zero-mod nodes.
- [x] Factory preset content contains no legacy zero-mod nodes or incident edges.
- [x] Focused tests, build, style checks, and screenshot verification pass.
- [x] Guide hover tethers originate at the hovered visible tile.
- [x] Keyboard transport and spectral-phase compact/expanded visuals agree with
  the requested layout and shared mapping.
