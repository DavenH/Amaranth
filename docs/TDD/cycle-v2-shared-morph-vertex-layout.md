# Cycle V2 Shared Morph and Vertex Controls Layout

Status: Implemented 2026-09-02

## Objective

Give the expanded Trimesh and Envelope editors one deliberate control hierarchy:
the shape/cube preview is compact, morph controls retain useful travel, vertex
properties occupy a clear right-hand column, and Axis, Link, and Guide actions
are labelled and sized as real control groups. Remove the contradictory circular
morph-slider handle while preserving the larger invisible interaction target.

## Authoritative Implementations

- `TrimeshSidePanelRenderer` is the shared production renderer and geometry
  source for Trimesh morph, Axis, Link, cube-preview, vertex-parameter, and Guide
  controls. `TrimeshWidget::expandedControlHitRegions` is authoritative for their
  pointer and keyboard targets.
- `EnvelopeMorphControls` owns Envelope's control-region geometry and delegates
  its vertex rows to `TrimeshSidePanelRenderer`. `EnvelopeEditorComponent` owns
  the existing JUCE slider and button interaction lifecycle.
- `GraphCommandDispatcher` and the editor command services remain authoritative
  for all durable edits. This work changes presentation geometry only.
- The existing `guideCurve.svg` is the semantic Guide Curve symbol. A Guide
  affordance must reuse that asset through the repository icon loader instead of
  maintaining a second hand-painted curve symbol.

## Production Geometry Contract

At the current expanded-editor reference widths:

- Every visible morph rail provides at least 96 px of value travel. Its painted
  value marker is a 1.5--2 px vertical line in the axis colour, at least 14 px
  tall, with no circular body. The existing hit region remains at least 23 px
  high and extends beyond the visible rail.
- Every selected-vertex rail provides at least 72 px of value travel. Guide
  controls may not consume the rail down to a token sliver.
- Vertex properties remain a right-hand column; the cube/plane preview occupies
  the left column and morph rows stack below or beside it according to the
  editor's available width. Sections never overlap at the compact supported
  expanded-editor size.
- Axis and Link labels are centered over their complete button columns in both
  editors. Group labels use the shared spanning-line treatment from the UI
  guidelines rather than floating unanchored text.
- Guide assignment has a labelled, minimum 24 px target and uses the semantic
  Guide Curve SVG. Envelope does not regain a Guide target while that command is
  unsupported.

## Test-First Contract

1. Geometry tests cover reference and compact widths for cube/plane, morph,
   vertex, Axis, Link, and Guide regions, including minimum rail travel and
   containment/non-overlap.
2. A renderer test proves the morph value marker geometry is a thin vertical
   line and contains no circular handle contract.
3. Trimesh hit-region tests repeat a complete morph drag and Axis/Link actions
   after the rearrangement; Envelope tests repeat a complete morph and vertex
   edit gesture.
4. Automation exposes section and group-label bounds for both editors so the
   production fixtures can assert containment and alignment after resizing.
5. Production-size screenshots cover selected-vertex states in both editors.

## Negative Boundaries

- Do not change morph values, link semantics, primary-axis semantics, selected
  vertices, Guide assignments, undo ownership, or publication timing.
- Do not add a second layout calculation in painting, hit-testing, or
  automation. All consumers use the same geometry functions.
- Do not reduce the interaction target to the thin painted marker or rail.
- Do not copy or redraw the Guide Curve SVG as ad-hoc path geometry.
- Do not expose unsupported Envelope Guide assignment.

## Implementation Slices

1. Replace the Trimesh circular morph handle with the precise line marker and
   add the missing Envelope Axis/Link group labels through shared group-label
   geometry.
2. Rebalance the Trimesh upper columns and Guide allocation so all vertex rails
   meet the production travel budget at reference and compact sizes.
3. Expose shared bounds to automation, preserve complete edit gestures, capture
   both production fixtures, and finish resize/pixel review.

## Completion Criteria

- All geometry, full-gesture, automation, and screenshot criteria pass for both
  expanded editors.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style review,
  and production-diff review are complete.
- `docs/TDD/ui-bugs.md` marks both the shared layout and morph-handle findings
  complete only after all slices and the Guide SVG integration are finished.

## Implementation Evidence

- Slice 1 replaces the Trimesh morph circles/dark overdraw with 1.5 x 17 px
  semantic-colour value markers. The existing 8 x 12 px hit expansion remains,
  yielding a 31 px-tall interaction target independent of the painted marker.
- Trimesh and Envelope now use the same Axis/Link group-label renderer and
  header geometry. Envelope publishes both label bounds for production
  automation; focused geometry and hosted-editor tests pass.
- The production Trimesh pointer replay still completes Axis and Link changes
  and captures `/private/tmp/cycle-v2-trimesh-morph-controls.png`. The Envelope
  marker fixture asserts both label bounds and captures
  `/private/tmp/cycle-v2-envelope-marker-controls-enabled.png`.
- Slice 2 gives the right-hand Trimesh vertex column a 224--300 px budget and
  caps painted morph travel at 160 px. At the 360 px compact side-panel fixture,
  Guide-bearing vertex rails retain at least 72 px, morph rails retain at least
  96 px, the cube retains at least 80 px, and the upper columns do not overlap.
  The unchanged production pointer replay passes after the geometry change.
- Slice 3 extracts the SVG parse/cache path into `NodeIconRenderer`, keyed by
  semantic resource ID. `NodePaletteEntryIconRenderer` remains a narrow
  `NodeKind` facade and no longer owns a second cache. The Trimesh Guide target
  now paints the existing `guideCurve.svg`; its former local curve/dot drawing
  is deleted. A 30 x 17 px raster test proves the SVG remains non-blank at the
  actual control size, while the existing registry-wide icon test still covers
  every graph node.
- Existing hosted interaction tests retain complete Trimesh morph and selected-
  vertex gestures and Envelope morph/selected-vertex gestures through commit.
  The production fixtures retain Axis/Link actions and selected-vertex states.
  Reference and compact geometry share the same public bounds used by painting,
  hit testing, and automation.
- Pixel review used the production fixtures. Full editor chrome and controls are
  visible in the pre-icon Trimesh and Envelope captures; after SVG integration,
  macOS denied process activation and the component capture omitted the OpenGL
  surface, but retained all six native-size Guide boxes and their legible shared
  curve symbols for the icon-specific review.
