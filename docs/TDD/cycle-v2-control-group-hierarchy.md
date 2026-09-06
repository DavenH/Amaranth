# Cycle V2 Control Group Hierarchy

Status: Implemented

## Objective

Make control scope and button meaning legible in every expanded effect,
Trimesh, and Envelope editor without restoring wasteful outer boxes. Use a
centered label with quiet flanking rules for genuine control groups. Replace
context-free binary toggles with controls that identify the property being
changed, show the available modes, and make the current mode explicit.

## Authoritative Implementations

- `PropertyControls` owns shared property-control typography, colour, spacing,
  and presentation primitives. It will own the spanning group-label primitive.
- `TrimeshSidePanelRenderer` owns Trimesh control geometry and painting. Its
  existing Axis/Link spanning-label treatment is the mature visual behavior to
  extract unchanged into `PropertyControls`.
- `EnvelopeMorphControls` owns Envelope control-region geometry, while
  `EnvelopeEditorComponent` owns the existing component interaction lifecycle.
  `EnvelopePurposeSelector` is the mature segmented-choice interaction and
  accessibility reference.
- Delay, Reverb, Equalizer, Unison, Impulse Response, and Waveshaper retain
  their domain editors, parameter mappings, command callbacks, gesture
  publication, previews, and undo behavior unchanged.

No compatibility adapter is required. The shared extraction translates only
group-label presentation. The Envelope axis-scale selector translates an
explicit Linear/Logarithmic choice to the existing `logarithmic` Boolean at the
component boundary; it does not own envelope view behavior.

## Audit And Intended Grouping

| Editor | Current issue | Intended groups |
| --- | --- | --- |
| Delay | Five rows read as one undifferentiated list | Echo, Stereo / output |
| Reverb | Space, filtering, and mix are visually flat | Space, Tone / output |
| Equalizer | Gain and Frequency headings do not show column scope | Gain and Frequency spanning headings |
| Unison | Group/Individual is hidden in a combo; `+`/`−` lack local meaning | Voice mode, Voice selection, active mode parameters |
| Waveshaper | A large gap implies groups but names neither | Gain, Quality |
| Impulse Response | `IR sample` is left aligned over three equal actions | Response, IR sample |
| Trimesh | Cube, morph, and vertex headings float without scope rules | Cube display, Morph position, Vertex parameters; retain Axis and Link |
| Envelope | Purpose, morph, markers, scale, range, and parameters share one band; `Vertex` and `Log` are ambiguous | Envelope purpose, Morph position, Envelope markers, Axis scale, Vertical range, Vertex parameters |

Group labels describe a shared operation, property, or outcome. They do not
repeat the editor title, narrate a single obvious control, or name only the
currently selected object. Small related actions share a row only when their
labels/icons and hit targets remain clear.

## Design Contract

- A group heading owns the full width of the controls it describes. Its text is
  centered between low-contrast horizontal rules with a deliberate text gap.
- Group headings add hierarchy, not containers: no surrounding rounded
  rectangle, nested background, or repeated outer inset is introduced.
- The heading row is 18 px unless a production-size review proves a domain
  needs a different established metric. Peer headings use identical geometry.
- Controls remain at least as large and precise as before. Added headings use
  existing stranded gaps where possible; otherwise editor content is
  rebalanced rather than compressing rails below their contracts.
- Envelope Axis scale is a two-segment selector labelled `Axis scale`, with
  both Linear and Logarithmic choices visible. Each segment contains a small
  line-spacing diagram and an accessible name. Selection uses fill, border, and
  a persistent indicator, not hue alone.
- Envelope marker actions are grouped as `Envelope markers`; their accessible
  names and tooltips continue to say that they set/toggle the selected vertex
  as Loop or Sustain.
- Envelope range actions are grouped as `Vertical range`; their accessible
  names remain Fit and Full rather than relying on icons alone.
- Unison voice add/remove controls gain explicit accessible names and local
  scope. Group/Individual becomes a visible mutually exclusive mode choice if
  it fits without reducing the parameter controls below their current geometry.

## Test-First Contract

1. Shared group-label geometry centers the text, leaves two nonzero rule runs,
   preserves a deliberate text gap, and remains contained at compact and
   reference widths.
2. Every affected editor exposes group-label bounds through its existing
   automation state; peer groups are contained, non-overlapping, and span the
   controls they name.
3. Envelope exposes two Axis-scale options with accessible Linear and
   Logarithmic names, distinct production-size diagrams, and a non-colour
   selected-state difference.
4. Selecting Linear then Logarithmic publishes through the existing discrete
   command path, changes the rendered grid, supports undo, and survives rebind.
5. Envelope Loop/Sustain, Fit/Full, morph, Axis/Link, and selected-vertex
   gestures retain their complete existing interaction tests after relayout.
6. All six effect editors retain enablement, parameter editing, undo, and
   preview behavior. IR resource actions and Unison voice actions retain their
   complete semantic sequences.
7. Production-size before/after captures cover all six effects, Trimesh, and
   Envelope in representative states.

## Negative Boundaries

- Do not restore outer grouping rectangles or title bands around visualization
  regions.
- Do not add headings to isolated controls when their property label already
  provides complete context.
- Do not use colour alone to distinguish a selected mode or toggle state.
- Do not copy the Trimesh spanning-label painter into each editor. Extract it
  once into the shared property presentation layer.
- Do not move parameter IDs, mappings, graph commands, undo, DSP, or preview
  policy into the shared presentation primitive.
- Do not implement a generic node-kind switchboard for domain grouping.
- Do not replace mature icons or interaction algorithms with approximations.

## Implementation Slices

1. Extract the shared spanning group-label primitive and migrate Trimesh's
   Axis/Link plus Cube, Morph, and Vertex headings. Add geometry/raster tests.
2. Reorganize Envelope controls and add the labelled Linear/Logarithmic Axis
   scale selector through the existing discrete command path. Preserve all
   marker, range, morph, vertex, and undo behavior.
3. Apply the audited groups to Delay, Reverb, Equalizer, Waveshaper, and IR,
   preserving current property and resource interaction contracts.
4. Make Unison's voice-editing modes and actions explicit, then complete the
   cross-editor production screenshot and automation review.
5. Rebalance the Envelope band around one aligned top row and one aligned
   action row. Move Envelope purpose beside Markers, Scaling, and Zoom; shorten
   those peer labels; replace the four Cycle v1 PNG-atlas action icons with a
   dedicated SVG family; and increase the Envelope editor height by the same
   amount as the control band so the graph retains its vertical budget.

Each slice receives a refactor pass, style check, focused semantic tests,
production capture, and an imperative commit before the next slice.

## Implementation Evidence

- Slice 1 extracts Trimesh's local spanning-label painter into
  `PropertyControls` as shared geometry, immediate painting, and a noninteractive
  component. Geometry tests prove centered text, two nonzero rule runs,
  deliberate text gaps, compact-width containment, visible raster output, and
  the absence of a surrounding painted box.
- Trimesh Axis and Link retain their existing geometry while Cube display,
  Morph position, and Vertex parameters adopt the same shared treatment. The
  existing complete pointer/keyboard interaction test passes unchanged.
- Production review used
  `/private/tmp/trimesh-group-labels.png` with filtered log
  `/private/tmp/trimesh-group-labels-logs.txt`; the editor remains unboxed and
  all five control-group headings are visible without clipping or reduced rail
  travel.
- Slice 2 replaces Envelope's context-free `Log` toggle with a domain-owned
  two-segment Axis scale selector. Linear and Logarithmic are separate
  keyboard-focusable buttons with equal-spacing and expanding-spacing diagrams,
  persistent selected underlines, tooltips, and accessible names. Both use the
  same extracted segmented-control shell as `EnvelopePurposeSelector`.
- Envelope's unchanged 246 px control band now identifies Envelope purpose,
  Morph plane, Morph position, Envelope markers, Axis scale, Vertical range,
  Axis, Link, and Vertex parameters. The former `Vertex` label and `Log` button
  are deleted; no graph height or morph/parameter travel is lost.
- Hosted tests cover 109 assertions across purpose, marker/range geometry,
  Linear/Logarithmic selection, real curve publication, grid changes, and shared
  morph controls. The semantic marker fixture passes. Production control review
  used `/private/tmp/envelope-groups-after.png`; macOS rejected the OS compositor
  handoff, so the app-side capture intentionally leaves the OpenGL curve region
  black while preserving the native control band under review.
- Slice 3 applies the shared heading component to Delay (`Echo`,
  `Stereo / output`), Reverb (`Space`, `Tone / output`), Equalizer (`Gain`,
  `Frequency`), Waveshaper (`Gain`, `Quality`), and Impulse Response
  (`Response`, `IR sample`). The effect mappings, row components, IR resource
  actions, and preview ownership remain unchanged. Group headings and bounds
  are exposed through the existing editor automation states.
- Slice 4 replaces Unison's mode combo with a visible `Group | Individual`
  segmented selector under `Voice mode`. `Voice selection` scopes the chooser
  and add/remove actions in Individual mode, while the parameter heading
  follows the active mode. Add/remove now have explicit accessible names and
  tooltips. The parameter widgets keep their former sizes; only the unused
  inter-row slack was reduced from 8 px to the shared 6 px gap.
- Focused tests pass with 16 group-label, 42 Delay/Reverb, 14 Equalizer, 34
  Waveshaper, 124 Impulse Response, 44 Unison, 78 Envelope purpose/interaction,
  and 18 logarithmic-grid assertions. The standalone `CycleV2` target builds
  successfully.
- Updated semantic fixtures pass for all six effects. Production-size OS
  review used `/private/tmp/group-delay-os.png`,
  `/private/tmp/group-reverb-os.png`, `/private/tmp/group-equalizer-os.png`,
  `/private/tmp/group-waveshaper-os.png`, `/private/tmp/group-ir-os.png`, and
  `/private/tmp/group-unison-os.png`. Their filtered logs contain no failures,
  assertions, warnings, or crashes. Together with the Trimesh and Envelope
  captures above, these cover every editor in the audit.
- Slice 5 increases the Envelope editor and its control band by 40 px together,
  preserving the graph's vertical budget. Morph plane, Morph position, Axis,
  Link, and Vertex parameters now share one heading baseline; the morph plane
  begins 10 px below its heading instead of floating in residual space.
- Envelope purpose joins Markers, Scaling, and Zoom in a single separated
  action toolbar. The four groups are contained and ordered by tested geometry,
  with smaller purposeful button widths and no overlap with the vertex column.
- Loop, Sustain, Fit, and Full no longer read from the Cycle v1 24 px PNG
  atlas. Four embedded SVGs use a shared 48 px viewBox, 2.4 px square-ended
  strokes, precise marker/arrow terminations, and inward-versus-outward zoom
  topology. XML validation, the embedded-component icon check, 90 hosted
  Envelope assertions, the purpose and marker fixtures, and the standalone
  build all pass. Final production review used
  `/private/tmp/envelope-harmony-final.png`; its filtered log contains no
  warnings, assertions, failures, or crashes.

## Deletion Targets

- Deleted `TrimeshSidePanelRenderer`'s private `drawSpanningGroupLabel`; all
  callers use the shared property-control primitive.
- Replaced IR's plain `resourceTitle` label with the shared group heading.
- Replaced Equalizer's plain column-header labels with spanning equivalents.
- Deleted Envelope's `vertexModeLabel` and context-free `Log` button.
- Deleted Unison's mode combo after production-size review of the segmented
  mode selector.

## Completion Criteria

- Every applicable control group in all six effect editors, Trimesh, and
  Envelope has an explicit, correctly scoped heading without an outer box.
- Every remaining button communicates its action/property and current state
  through visible local context plus accessible naming.
- Envelope Axis scale and Unison voice mode show their alternatives and current
  selection directly.
- All deletion targets, semantic tests, automation fixtures, production-size
  screenshots, standalone build, style review, and diff review are complete.
