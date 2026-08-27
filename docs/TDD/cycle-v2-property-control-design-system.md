# Cycle V2 Property Control Design System

## Status

In Progress (all property-presentation and Voice Context hosted-interaction
slices complete; IR resource actions remain blocked on the documented
wave-resource command-service boundary, 2026-08-27).

## Context

Cycle V2 has several individually useful editor styles but no shared visual and
interaction contract for property controls. The result is not merely cosmetic:

- the Guide Curve editor places a label, gap, slider, and 42-pixel value field
  inside a 236-pixel rail, leaving too little effective slider travel for the
  five-decimal increment it advertises;
- Waveshaper places unlabeled normalized values on short controls inside a
  sparse 224-pixel rail;
- Impulse Response gives most of a 1,050-pixel editor to an unscaled waveform
  while its 212-pixel control rail contains short sliders, no visible values,
  three equal-weight commands, and a large unused lower region;
- Voice Context has strong label, track, landmark, and readout alignment, but
  implements those mechanics through its own painted geometry; and
- Delay and Reverb provide long tracks, semantic readouts, honest landmarks,
  and mature gesture publication, but use a loose effect-specific row rhythm
  and live in a mixed-domain editor implementation scheduled for extraction.

These differences prevent users from transferring expectations between
panels. Some controls look compact but are hard to acquire; others are easy to
drag but do not expose an exact value; some show raw normalized values rather
than musical units; and related panels disagree about header, label, focus,
reset, and value-entry behavior.

This TDD defines a shared property-control grammar and migrates the first
high-value panels through it. It applies macOS property-panel principles of
density, alignment, native-feeling interaction, keyboard access, and semantic
value presentation without attempting to make Cycle V2 visually
indistinguishable from AppKit. Cycle's canvas palette and domain-specific
visualizations remain product identity.

## Product Principles

### Explicitness Without Noise

Every state that materially affects sound or authoring must be readily
inspectable. This does not require every possible control to remain expanded at
once.

Apply this visibility order:

1. Keep frequent, directly manipulated properties visible.
2. Keep their current semantic values and units visible.
3. Summarize important state and routing in compact or overview presentation.
4. Put infrequent configuration behind a clear disclosure or named action.
5. Show contextual commands only when they apply.
6. Duplicate presentation only when the two views serve different tasks.

The Curve Guide shelf and expanded Guide editor are valid overview/detail
duplication: the shelf navigates resources and the editor manipulates one
resource. Two complete editable copies of the same property set would be
unjustified duplication.

### Four Control Contracts

For every control, specify and test four independent geometries:

1. **Visual footprint:** the visible track, thumb, fill, label, and value.
2. **Hit target:** the region that acquires hover, focus, click, and drag.
3. **Manipulation mapping:** ordinary drag, fine drag, wheel, keyboard, direct
   entry, reset, clamping, and snapping behavior.
4. **Value indication:** the exact point and text that communicate the current
   value.

Increasing one geometry must not silently enlarge or degrade the others. A
thin track may have a comfortable hit target. A compact thumb must still show
an exact reference point. A long track does not by itself provide fine
resolution.

### Space Has A Job

Each large region must explain its allocation through information,
manipulation, recognition, or group separation. Remaining space goes to the
content that can convert it into useful information or precision; it is not
distributed evenly by default.

Graph-plus-properties editors therefore preserve a large domain view, but the
property rail first receives enough width for readable values and useful
control travel. Large unexplained voids inside a rail fail the design even when
the complete panel looks balanced from a distance.

## Authoritative Implementations

The implementation must extract and compose current mature behavior rather
than create a third parallel control system.

- Curve control construction and transaction binding:
  `cycle-v2/src/Nodes/Curve/Editor/CurveEditorPrimitives.*` and
  `CurveExpandedEditorComponent.*`.
- Guide resource editing and compact-host behavior:
  `cycle-v2/src/Nodes/Guide/Editor/GuideCurveEditorComponent.*` and
  `cycle-v2-guide-resource-dock.md`.
- Waveshaper domain model, panel, and publication:
  `cycle-v2/src/Nodes/Waveshaper/` and the shared curve editor base.
- Impulse Response domain model, panel, and publication:
  `cycle-v2/src/Nodes/ImpulseResponse/` and the shared curve editor base.
- Existing Waveshaper gain conversion and IR parameter conversion:
  `WaveshaperSignalProcessor.cpp`, `lib/src/Audio/CycleDsp/IrModel.*`, and the
  Cycle 1 effect implementations. Waveshaper's currently private gain helper
  must be extracted to an application-neutral domain mapping before the UI
  formats that value; it must not be copied into the editor.
- Effect parameter mapping, formatting, snapping, reset, transaction, and
  preview behavior: `lib/src/Audio/CycleDsp/EffectParameterMapping.*`, the
  domain DSP implementations, and the current effect editor in
  `cycle-v2/src/UI/ConcreteNodeEditors.cpp`.
- Voice Context geometry, domain mapping, landmarks, and hit semantics:
  `cycle-v2/src/UI/VoiceContextCompactEditor.*` until its domain extraction
  under `cycle-v2-source-layout.md`.
- Expanded-editor lifetime, rebinding, close, and publication:
  `cycle-v2/src/UI/NodeEditorHost.*` and `NodeEditorCommandService.*`.
- Current focused interaction coverage:
  `cycle-v2/tests/TestNodeEditorHost.cpp`,
  `cycle-v2/tests/TestNodeCanvasArchitecture.cpp`, and the existing Guide,
  Waveshaper, Delay, Reverb, and IR automation fixtures.

Behavior reused unchanged includes semantic parameter mappings, model
publication, gesture-level undo, prepared-preview invalidation, curve-panel
interaction, Guide resource ownership, and effect DSP visualization. This TDD
changes their presentation and input adapters, not their domain algorithms.

## Shared Design Contract

### Spacing And Alignment Tokens

The first implementation will validate this compact token set at production
size. Values may move during the screenshot review, but panels may not invent
local replacements without a documented semantic need.

| Token | Initial value | Contract |
| --- | ---: | --- |
| Outer panel inset | 12 px | Applied once by the owning content container |
| Inline gap | 8 px | Label-to-control and control-to-value spacing |
| Property row height | 30 px | Includes a comfortable interactive target |
| Property row gap | 6 px | Repeated properties within one group |
| Section gap | 16 px | Marks a real semantic group boundary |
| Label column | 88 px | Shared within a rail; alignment follows the row variant |
| Value field | 56 px minimum | Expanded for the longest valid formatted value |
| Continuous visible track | 4 px | Presentation only, centred in the hit region |
| Continuous thumb | 8-10 px | Contains a centre notch or hairline reference |
| Continuous hit height | 28 px minimum | May exceed the painted geometry |
| Ordinary usable track | 140 px minimum | After thumb radius and end insets |
| Graph-editor rail | 328-360 px | Enough for label, track, gap, and value field |

The initial one-line ordinary row is:

```text
| label 88 | 8 | flexible track >= 140 | 8 | value >= 56 |
```

If the available width cannot satisfy that row, the layout switches to an
explicit compact variant with label and value on one line and a full-width
track below. It must not silently compress the track below its minimum.

Labels, control edges, value fields, section edges, and text baselines align
within a panel. Dense one-line rows use trailing-aligned labels; two-line rows
use leading-aligned labels above the control. A region does not mix these
without a semantic group boundary. Domain visualizations may override optical
centring, but not the property grid.

### Property Roles

Shared presentation primitives cover only stable roles:

- continuous amount with semantic formatter and parser;
- discrete ordered value with visible meaningful stops when practical;
- Boolean state;
- mutually exclusive mode selection;
- contextual or immediate action;
- section heading and explanatory status.

The shared layer owns component geometry, appearance states, accessibility
surface, and input mechanics. It does not own node kinds, parameter IDs,
defaults, DSP mappings, command dispatch, or which properties a panel exposes.
Concrete domains compose the roles and provide semantic formatting, parsing,
landmarks, defaults, and publication callbacks.

### Precision Slider Contract

Every continuous or ordered control records an adjustment budget:

| Field | Meaning |
| --- | --- |
| Domain range | User-facing minimum, maximum, and units |
| Meaningful increment | Smallest change users need to select intentionally |
| `D` | Usable ordinary drag distance after end insets and thumb radius |
| `R` | Number of meaningful increments across the range |
| Ordinary mapping | Absolute or relative drag behavior |
| Fine mapping | Shift-drag sensitivity and fine keyboard increment |
| Entry | Accepted numeric text and unit behavior |
| Indication | Display precision and exact visual reference |

If `D / R` does not support intentional pointer placement, the control must
provide direct numeric entry and fine adjustment rather than implying that the
track alone provides the resolution.

All precision sliders follow these interaction rules unless a domain documents
a stronger convention:

- ordinary dragging previews continuously and clamps at exact endpoints;
- Shift-drag provides a discoverable fine-adjust mode without jumping when the
  modifier changes;
- arrow keys use the meaningful ordinary increment and Shift-arrow uses the
  fine increment;
- the value field accepts valid semantic text and preserves invalid text while
  focused so the user can correct it;
- Escape cancels an in-progress text edit and restores the prior value;
- Return commits a valid text edit;
- double-clicking the track or thumb restores the documented default;
- one pointer drag, fine drag, keyboard adjustment sequence, text commit, or
  reset produces one semantic undo entry as appropriate;
- focus, hover, active drag, disabled, and invalid-entry states are visible
  without relying on hue alone; and
- tooltip or established hover-help copy exposes fine adjustment and reset
  gestures without permanent instructional text in every row.

The value field displays semantic units such as `dB`, `Hz`, `s`, `%`, beats,
or an integer multiplier. Raw normalized values are allowed only when the
normalized quantity is itself the domain concept.

### Discrete Controls And Actions

- Discrete sliders and selectors expose every meaningful stop when the count
  and width allow it; pointer, wheel, keyboard, reset, and programmatic updates
  agree on the same values.
- A selector is preferred over a pseudo-continuous slider when the ordered
  geometry adds no useful comparison.
- Primary, secondary, contextual, and destructive actions do not share equal
  visual weight by default.
- Contextual actions are disabled or absent when unavailable, with a readable
  explanation when the reason is not evident.
- Destructive resource actions are separated from processing properties and
  rely on undo when deletion is recoverable.

### Panel And Header Contract

Expanded editors share title typography, enabled-state placement, close
behavior, focus order, border treatment, and content inset. A header may omit
an Enabled control when the domain has no bypass state, but it must not create
a second local header style.

The close affordance must be a real focusable semantic control, not only a
painted hit region. Escape continues to close an expanded editor through the
host-level behavior established for Guide and Spy detail editors.

Graph-plus-properties panels use content-driven bounds:

- the rail remains within the validated 328-360-pixel range;
- the graph receives the remaining flexible width;
- the editor has a useful maximum width and does not expand to nearly the full
  canvas merely because space is available;
- a square transfer view remains square;
- a time-domain view may remain wide, but includes scale or landmarks that
  explain its width; and
- below the minimum two-column width, the panel uses a deliberate stacked or
  scrolling layout rather than compressing controls.

Exact preferred and minimum bounds are specified and screenshot-tested in each
domain migration slice after measuring its content.

## Architecture

### Shared Presentation Core

Introduce a small presentation-only editor-control module under the target
`cycle-v2/src/UI/Editors/` ownership from `cycle-v2-source-layout.md`. Expected
responsibilities are:

- shared tokens and state colours;
- a precision-slider LookAndFeel or component;
- label, value-field, Boolean, selector, section, and action presentation;
- ordinary and compact property-row geometry; and
- accessibility labels, values, help, focus, and keyboard surface.

Names are selected during implementation after the first concrete API is
proven. The design does not require a universal `PropertySchema`, a runtime
form builder, or inheritance across unrelated editors.

`CurveEditorPrimitives` remains the Curve-domain binding and layout adapter. It
composes the shared presentation core while preserving the transaction
lifecycle owned by `CurveExpandedEditorComponent`. Domain editors retain named
members and explicit layout, making their meaningful controls visible in code.

Effect-domain editors compose the same presentation primitives with
`NodeEditorCommands`. Their mapping, formatter, landmarks, previews, and
parameter lists remain within Reverb, Delay, Equalizer, and other concrete
domain folders as the active source-layout TDD extracts them.

Voice Context retains its explicit domain geometry and edit model. It may reuse
shared tokens, row geometry, and slider painting without becoming a child of a
generic parameter editor or moving Voice semantics into generic UI code.

### Permitted Adapters

Adapters may translate:

- shared component events to Curve editor bindings or `NodeEditorCommands`;
- normalized stored values to an authoritative domain formatter/parser;
- component focus and accessibility events to the existing host lifecycle; and
- shared row rectangles to domain-owned visualization bounds.

Adapters may not implement parameter mapping, DSP behavior, resource
ownership, curve interaction, undo policy, or node-kind selection.

### Expected Production Change

The first shared-core and Guide slice is expected to add roughly 250-450 lines
of cohesive production code and remove or replace the existing local styling
and compact-value helpers. The complete migration may touch 8-14 production
files with roughly 400-800 net changed lines, excluding behavior-neutral file
moves performed by `cycle-v2-source-layout.md`.

A shared component exceeding roughly 300 lines, a migration exceeding 800 net
production lines without domain file moves, or repeated per-domain modifier,
focus, formatting, or value-entry branches is evidence to stop and review the
abstraction.

No temporary compatibility layer is planned. Existing domain editors migrate
directly, one complete slice at a time.

### Lifecycle, Threads, And Complexity

Property components are created, rebound, focused, edited, painted, and
destroyed on the JUCE message thread under the existing editor host. Shared
LookAndFeel lifetime must outlive every attached control and must be detached
before destruction. Rebinding an editor updates component state without
publishing a user edit or retaining callbacks to the prior node or Guide.

Pointer and keyboard edits continue through the existing domain command
service or Curve editor transaction. The shared presentation core never reads
or writes `NodeGraph`, owns undo, schedules compilation, or communicates with
the audio thread. DSP and preview configuration continue to cross their
existing immutable publication boundaries.

Ordinary layout and paint are `O(P + T)`, where `P` is the small number of
visible properties and `T` is the number of declared landmarks. Hit testing
and one control update are constant time. A drag update performs no parameter
registry scan, node-kind dispatch, filesystem access, DSP analysis, image
generation, or control-tree rebuild. Semantic text parsing occurs only while
editing or committing the value field, not on the audio or paint path.

## Migration Slices

### Slice 1: Shared Core And Guide Proving Ground

- Capture the post-merge Guide editor at the standard automation size and
  annotate panel, graph, rail, row, track, thumb, value, and empty bounds.
- Define shared tokens, precision-slider mechanics, property-row layout, value
  entry, focus, and accessibility behavior.
- Make Curve presentation primitives compose the new core while retaining
  Curve transaction binding.
- Give Noise, DC Offset, and Phase explicit adjustment budgets and semantic
  display precision.
- Replace the 236-pixel rail with a width that satisfies the ordinary-track
  minimum.
- Constrain the editor to content-driven maximum bounds while preserving a
  useful curve canvas.
- Make the close affordance focusable and preserve first-Escape dismissal.
- Add focused geometry and complete-gesture tests before visual tuning.

### Slice 2: Waveshaper

- Preserve the square transfer graph and allocate panel size around that
  diagnostic proportion.
- Show Pre Gain and Post Gain as semantic values with units derived from the
  authoritative mapping rather than raw normalized values.
- Give both continuous controls the shared precision, entry, reset, focus, and
  undo behavior.
- Present antialiasing as an honest ordered discrete control with `1x`, `2x`,
  `4x`, and `8x` values.
- Remove the centred sparse control island and explain every remaining gap
  through grouping or panel geometry.

### Slice 3: Impulse Response

- Show Size, Post Gain, and High Pass using authoritative semantic mappings and
  units. Do not invent a display mapping in editor code.
- Treat Size as the actual discrete set currently represented by eight stops,
  unless a separate DSP design changes that contract.
- Add time, sample, or length landmarks to the response view so wide empty
  support communicates duration; provide fit/zoom only if the mature panel can
  own it without duplicating waveform interaction.
- Replace equal-weight Load, Unload, and Model buttons with a documented action
  hierarchy. Establish whether Model is a command, mode, or editor destination
  before choosing its control.
- Keep resource actions separate from continuous processing properties and
  remove the unused lower rail region.

If authoritative semantic mappings for Size, Post Gain, High Pass, or Model
cannot be located, this slice stops at that boundary and records the missing
domain contract instead of shipping an approximation.

### Slice 4: Mature Reference Alignment

- Migrate Delay and Reverb property presentation only after their domain
  editors are cohesive under the active source-layout design.
- Preserve their existing DSP mappings, long-track interaction, landmarks,
  readouts, previews, reset behavior, and gesture publication unchanged.
- Make Equalizer consume shared tokens and precision behavior where compatible,
  without forcing its paired gain/frequency layout into an ordinary rail.
- Make Voice Context consume the shared spacing, state, focus, and precision
  language while retaining its compact domain-specific layout.
- Compare all migrated panels together and remove remaining local literals or
  duplicated generic control painting.

Implementation design for Delay and Reverb:

- The authoritative implementation is the current effect editor in
  `ConcreteNodeEditors.cpp`, together with `DelayPreviewPainter`, the Reverb
  preview resource, `EffectParameterMapping`, `CycleDelay`, and
  `NodeEditorCommandService`. Preview geometry, semantic mappings, snapping,
  enabled behavior, continuous publication, gesture-level undo, and the
  520-by-520 host silhouette are reused unchanged.
- Extract domain-owned editor components and factories under `Nodes/Delay` and
  `Nodes/Reverb`. `ConcreteNodeEditors.cpp` retains registry assembly and the
  still-pending Equalizer implementation; it no longer chooses Delay or Reverb
  controls, formatting, snapping, plotting, automation, or layout by node
  kind.
- Add only a narrow presentation-to-command binding beside the shared property
  controls. It translates slider gesture lifecycle and normalized values to
  `NodeEditorCommands`; it contains no concrete node kind, parameter ID,
  formatter, mapping, preview, or layout policy.
- Delay owns its beat mapping, tempo landmarks, Pan Cycle stops, and delay
  preview. Reverb owns its seven kernel-size stops, seconds display, and
  spectrogram preview. Percentage properties remain explicit domain members in
  each editor.
- Both domains use the shared two-line property-row variant so labels and
  editable values sit above a nearly full-width track. This preserves the
  mature panels' useful adjustment distance instead of forcing their macro
  layout into the narrower graph-editor rail grammar.
- The stable end state is deletion of all Delay/Reverb branches and members
  from `ConcreteNodeEditors.cpp`. No compatibility adapter remains; the narrow
  binding is a stable shared boundary for domain-owned parameter editors.

Implemented:

- Extracted complete Delay and Reverb editor components and factories into
  their domain directories. The generic registry now selects those factories
  without containing either domain's layout, mappings, preview, or controls.
- Preserved both 520-by-520 panels, 150-pixel previews, enabled state,
  publication path, and preview painters while replacing their local slider
  presentation with shared two-line property rows and editable semantic value
  fields.
- Delay now exposes time in beats with `0.5`, `1`, `2`, `3`, and `4` beat
  landmarks, Pan Cycle as the exact `1` through `12` stop set, and amount
  controls as percentages. Reverb exposes its authoritative seven kernel-size
  stops as seconds at the existing 44.1-kHz reference and its amount controls
  as percentages.
- Added one narrow `NodePropertySliderRow` binding for semantic parameter
  gestures. It owns only command lifecycle translation and suppresses nested
  JUCE drag notifications so double-click reset remains one transaction.
- Parameter gesture completion no longer rebuilds the open editor. Domain
  callbacks already retain the current local presentation, while the durable
  graph and downstream refresh are committed by the command service. This
  keeps the interacted control alive across successive gestures.

Adjustment budgets:

| Control family | Domain | Ordinary / fine step | `D` | Alternate precision path |
| --- | --- | --- | ---: | --- |
| Delay Time | 0.09-4.00 beats | 0.25 / 0.05 beats | 476 px | editable beat entry |
| Delay Pan Cycle | 1-12 intervals | one discrete stop | 476 px | exact integer entry |
| Delay amounts | 0-100% | 1% / 0.1% | 476 px | editable percentage entry |
| Reverb Size | seven kernel durations | one discrete stop | 476 px | exact displayed-second entry |
| Reverb amounts | 0-100% | 1% / 0.1% | 476 px | editable percentage entry |

Review evidence:

- Production diff for this slice: 844 lines added and 211 removed across 11
  production files, for 633 net lines. The largest new implementation is the
  domain-owned Delay editor at 296 lines. Generic shared code gains no node
  kind, concrete parameter ID, DSP mapping, preview, or layout branch.
- Focused host tests cover domain ownership, 140-pixel minimum geometry,
  semantic entry, and nested slider notification collapse. Existing command
  service tests retain two-update publication, commit, downstream movement,
  and undo coverage.
- Real-input automation reports
  `/private/tmp/cycle-v2-delay-property-report.json` and
  `/private/tmp/cycle-v2-reverb-property-report.json` have zero failed commands
  and cover two drag updates, commit, undo, a subsequent reset in the same
  control instance, and Shift-drag. The existing Reverb high-pass live-preview
  fixture also passes after extraction.
- Production screenshots: Delay before
  `/private/tmp/cycle-v2-delay-properties-before.png`, Delay after
  `/private/tmp/cycle-v2-delay-properties-after.png`, Reverb before
  `/private/tmp/cycle-v2-reverb-properties-before.png`, and Reverb after
  `/private/tmp/cycle-v2-reverb-properties-after.png`.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. The complete Cycle V2 run passes 10,239 of 10,240
  assertions; its sole failure remains the pre-existing hit-router hover-help
  assertion recorded in `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-delay-reverb-tests-junit.xml`. A compilation database
  remains unavailable for focused clang-tidy. No DSP or visualization hot-loop
  behavior changed.

Implementation design for Equalizer:

- The authoritative implementation is the current Equalizer editor in
  `ConcreteNodeEditors.cpp`, `EqualizerPreviewPainter`, and the gain/frequency
  functions in `EffectParameterMapping`. The five-band topology, shelf/peak
  semantics, response graph, marker hit geometry, graph-to-paired-parameter
  gesture, defaults, and 620-by-550 silhouette remain unchanged.
- Extract the complete editor and factory under `Nodes/Equalizer`; the generic
  registry retains only factory registration. This deletes the final
  effect-domain implementation from `ConcreteNodeEditors.cpp`.
- Compose two shared compact property rows per band inside the existing paired
  Gain/Frequency columns. The paired macro layout remains domain-owned; the
  shared rows provide exact hairline thumbs, editable semantic values, focus,
  reset, fine adjustment, and component geometry.
- Gain retains the authoritative -30-to-+30 dB mapping and strong pixel-based
  0 dB detent. Frequency retains the authoritative continuous 20 Hz-to-20 kHz
  logarithmic mapping; its visible landmarks are references and never hard
  snap points. Direct entry accepts `Hz` and `kHz`.
- Direct slider gestures use `NodePropertySliderRow`. Dragging a response-graph
  band marker continues to use the existing paired parameter transaction and
  updates the same two domain rows without notification. No graph interaction
  or response calculation enters shared presentation code.

Implemented:

- Extracted the complete Equalizer editor and factory under
  `Nodes/Equalizer`; `ConcreteNodeEditors.cpp` now contains only the remaining
  Curve and Trimesh host adapters plus factory registration.
- Preserved the five-band paired columns, response painter, marker hit
  geometry, paired graph gesture, continuous logarithmic frequency mapping,
  shelf/peak explanations, and 620-by-550 panel.
- Replaced the local slider, label, readout, and gesture implementation with
  shared compact property rows. Every gain and frequency value is now an
  editable semantic field, and all sliders use the shared exact hairline thumb,
  focus, fine adjustment, keyboard, and reset behavior.
- Ordinary Gain dragging retains the strong pixel-based 0 dB detent. Fine
  dragging bypasses that detent so a user can intentionally select small
  non-zero values; direct entry and reset still resolve exactly.

Adjustment budgets:

| Control | Domain | Ordinary / fine step | `D` | Alternate precision path |
| --- | --- | --- | ---: | --- |
| Band Gain | -30.0 to +30.0 dB | 0.6 / 0.06 dB | 241 px | editable signed dB entry |
| Band Frequency | 20 Hz to 20 kHz, continuous logarithmic | 0.01 / 0.001 normalized | 241 px | editable Hz or kHz entry |

Review evidence:

- Production diff for this slice: 456 lines added and 408 removed across five
  production files, for 48 net lines. The 442-line domain editor replaces the
  final 408-line mixed effect implementation; the additional surface is the
  semantic parsing, shared-row composition, and explicit graph-gesture
  separation. Generic shared code gains no Equalizer branch.
- Focused host tests cover paired-row ownership, semantic gain/frequency
  entry, formatted units, and minimum track geometry. The existing paired
  graph-command test passes nine assertions for one transaction and undo.
- `/private/tmp/cycle-v2-equalizer-property-report.json` has zero failed
  commands and covers two slider updates, commit, undo, fine drag outside the
  ordinary detent, and reset. The corrected existing graph fixture report
  `/private/tmp/cycle-v2-equalizer-editor-report.json` has zero failed commands
  and covers paired marker drag plus save/open persistence.
- Production screenshots: before
  `/private/tmp/cycle-v2-equalizer-properties-before.png`; after
  `/private/tmp/cycle-v2-equalizer-properties-after.png`.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. The complete Cycle V2 run passes 10,251 of 10,252
  assertions; its sole failure remains the pre-existing hit-router hover-help
  assertion recorded in `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-equalizer-tests-junit.xml`. A compilation database
  remains unavailable for focused clang-tidy. The retained scalar `std::pow`
  is the authoritative one-call graph-marker mapping, not a DSP, sample, bin,
  or pixel loop.

Implementation design for Voice Context presentation:

- The authoritative implementation remains `VoiceContextCompactEditor`: it
  owns the source-domain selector, parameter mappings, landmark values,
  readouts, hit geometry, canvas edit routing, and compact node summary.
- Expose only the shared precision-slider paint primitive already used by the
  shared LookAndFeel. The primitive accepts a domain accent colour but owns the
  track thickness, fill, exact hairline thumb, and enabled/hover/focus states;
  it has no Voice parameter or hit-testing knowledge.
- Align the expanded Voice rows to the shared 88-pixel label, 30-pixel row,
  and 6-pixel gap metrics while preserving its single-column macro layout,
  semantic landmarks, readouts, selector, checkbox, and discrete oversampling
  control.
- Voice Context remains a canvas-painted special editor, not a hosted tree of
  JUCE controls. This slice does not invent keyboard focus, text entry, or undo
  behavior in paint code. Moving those interactions to real semantic controls
  requires a separate host extraction that preserves its current canvas edit
  service and complete gesture semantics.

Implementation design for the hosted Voice Context extraction:

- The authoritative domain behavior is the current
  `VoiceContextCompactEditor`, the Voice parameter definitions,
  `EffectParameterMapping::voiceLengthSeconds`/`voiceLengthUnitValue`, and the
  transient parameter gesture owned by `NodeEditorCommandService`. The
  extraction moves the expanded presentation and its ranges, stops, labels,
  and defaults; compact-node selector and summary behavior remain in
  `VoiceContextCompactEditor`.
- Add a domain-owned hosted editor under `Nodes/VoiceContext/Editor`. It
  composes shared property rows for Octave, Voice Length, and Pitch; visible
  option groups for the two start domains and four oversampling factors; and a
  semantic Portamento toggle. It contains no graph, dispatcher, document, or
  undo ownership.
- Octave and Pitch publish through the existing numeric transient parameter
  service, preserving two-update gesture, commit, downstream refresh, and one
  undo. Domain and Oversampling use the existing semantic text command; each
  selection is one complete discrete edit. Voice Length remains an application
  preview setting and crosses a narrow `NodeEditorResources` setter beside the
  existing Unison preview context rather than becoming a fake graph parameter.
- Make Voice Context a normal hosted-editor capability and register its
  domain factory. Delete the expanded paint/hit methods, `VoiceContextEdit`,
  canvas drag state, authoring gesture bridge, coordinator route branch, and
  Voice-specific automation hit geometry. Hosted child components become the
  real event and automation targets.
- The stable end state leaves `VoiceContextCompactEditor` responsible only for
  the compact node selector and summary. `NodeCanvas` coordinates preview
  duration storage through `NodeEditorResources` and no longer interprets
  Voice pointer gestures or paints the expanded panel.
- Focused tests must cover every visible option, semantic text entry, invalid
  correction, ordinary and fine keyboard operation, two drag updates followed
  by commit and undo, preview-length persistence, hosted target discovery, and
  first-Escape close. The existing real attachment fixture is migrated from
  painted bounds to component IDs and remains the production event-path proof.

Implemented:

- Continuous Octave, Voice Length, and Pitch rails now use the same exact
  indicator, four-pixel track, fill treatment, and state-capable painter as the
  other migrated property controls, retaining each Voice domain accent.
- Replaced local row, gap, and label metrics with the shared property metrics.
  The content block grew by six pixels so all six rows retain their established
  rhythm without overlap.
- Retained Voice's explicit ticks, seconds/semitone readouts, source selector,
  Portamento checkbox, discrete oversampling stops, hit bounds, mappings, and
  graph-command routing unchanged.

Adjustment budgets:

| Control | Domain | Discrete / displayed increment | `D` | Existing alternate path |
| --- | --- | --- | ---: | --- |
| Octave | -2 to +2 octaves | one octave | 256 px | visible five-stop landmarks |
| Voice Length | 0.05 to 148 seconds, nonlinear | rounded seconds | 256 px | semantic seconds readout and reference ticks |
| Pitch | -12 to +12 semitones | one semitone | 256 px | semantic semitone readout |

Review evidence:

- Production diff for this slice: 46 lines added and 12 removed across three
  production files. The shared painter gains geometry and state parameters
  only; it has no Voice control, mapping, hit, or command branch.
- The existing painted-row contract passes 36 assertions covering every
  authored hit region, endpoint, mapping, label, and compact summary.
- The Voice attachment automation still completes Octave, Pitch,
  Oversampling, and Voice Length drag gestures and downstream preview updates.
  Production screenshot:
  `/private/tmp/cycle-v2-voice-context-properties-after.png`.
- At this interim point the migration was presentation-complete but
  interaction-partial; the hosted extraction below closes that boundary.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. The complete Cycle V2 run passes 10,251 of 10,252
  assertions; its sole failure remains the pre-existing hit-router hover-help
  assertion recorded in `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-voice-context-property-tests-junit.xml`. Existing
  scalar `std::abs` calls only format two message-thread summary strings.

Hosted extraction completion (2026-08-27):

- Added a domain-owned Voice Context editor under `Nodes/VoiceContext/Editor`
  and made Voice Context a normal hosted-editor capability. Octave, Voice
  Length, and Pitch now compose shared semantic property rows; Domain and
  Oversampling are visible exclusive option groups; Portamento is a real
  focusable toggle.
- Octave and Pitch use the existing transient `NodeEditorCommandService`
  gesture path. Domain and Oversampling use semantic text commands. Voice
  Length uses the authoritative nonlinear seconds mapping and crosses only a
  narrow preview-duration setter beside the existing Unison preview context.
- Direct entry accepts integral Octave/Pitch values and semantic seconds,
  rejects invalid fractional pitch, and displays concise real values. Ordinary
  and Shift keyboard adjustment use domain increments, and all hosted controls
  expose stable automation IDs.
- Deleted the expanded Voice paint and hit-test implementation,
  `VoiceContextEdit`, canvas Voice drag state, the authoring gesture bridge,
  coordinator routing, and Voice-specific automation hit geometry. The compact
  editor now owns only its source-domain selector and summary.
- Expanded endpoint landmark labels are clamped within the slider width, so
  `0.05` and `148` remain readable at production size. Preview status and the
  field now use the same shared concise-real policy.

Hosted adjustment budgets:

| Control | Domain | Ordinary / fine increment | `D` | Alternate precision path |
| --- | --- | --- | ---: | --- |
| Octave | -2 to +2 octaves | one octave | >= 140 px | exact integer entry and five landmarks |
| Voice Length | 0.05 to 148 seconds, nonlinear | 0.1 / 0.01 seconds by keyboard | >= 140 px | semantic seconds entry and four reference landmarks |
| Pitch | -12 to +12 semitones | one semitone | >= 140 px | exact integer entry and three landmarks |

Hosted extraction evidence:

- Production change: 442 lines added and 680 removed, for 238 net lines
  deleted. The largest new production file is the 408-line domain editor;
  `VoiceContextCompactEditor.cpp` loses 442 lines. Generic shared code gains no
  node-kind, parameter-ID, mapping, resource, or gesture branch.
- Focused hosted tests pass 54 assertions, including every visible option,
  semantic and invalid entry, keyboard adjustment, preview duration, and a
  real two-update pitch gesture followed by commit, downstream refresh, and
  undo. Compact-editor tests pass 18 assertions and the remaining Voice
  authoring test passes seven.
- The migrated attachment fixture has zero failed commands and exercises real
  Octave, Pitch, Oversampling, and Voice Length controls. Report:
  `/private/tmp/cycle-v2-voice-context-hosted-final-report.json`; filtered log:
  `/private/tmp/cycle-v2-voice-context-hosted-final-logs.txt`. A separate
  first-Escape fixture also has zero failures:
  `/private/tmp/cycle-v2-voice-context-escape-report.json`.
- Production screenshot:
  `/private/tmp/cycle-v2-voice-context-hosted-final.png`. It verifies readable
  endpoint ticks, aligned labels and values, visible selected options, concise
  readouts, and the shared exact slider indicator.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. The complete Cycle V2 run passes 10,290 of 10,291
  assertions; its sole failure remains the pre-existing hit-router hover-help
  assertion recorded in `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-voice-context-tests-junit.xml`. No DSP or
  visualization hot-loop behavior changed.

### Shared Indicator And Display Precision Refinement (2026-08-27)

Implemented:

- The exact thumb hairline now derives its one-pixel rectangle from the
  floating-point centre of the thumb. It no longer rounds the centre to an
  integer X coordinate, which had made the line move optically left or right
  as a static slider entered a drag.
- Added one shared real-value formatter for property readouts. Non-integral
  values retain at most the precision needed for two significant figures;
  values displayed at whole-number resolution omit the decimal point and
  trailing zeroes.
- Applied the policy to Guide percentages, Waveshaper and IR gain, IR high
  pass, Delay beats and percentages, Reverb seconds and percentages, Equalizer
  gain and kHz readouts, and the Voice Context duration summary. Parsing,
  stored parameter precision, automation serialization, drag sensitivity, and
  domain mappings remain unchanged.

Review evidence:

- Geometry tests cover integer, quarter-pixel, half-pixel, and three-quarter-
  pixel thumb positions and require the indicator and thumb centres to remain
  identical. Formatter tests cover whole, signed, percentage, sub-unit, and
  rounding cases.
- Shared property-control tests pass 68 assertions; focused node-editor host
  tests pass 160 assertions.
- Guide, Waveshaper, IR, Delay, Reverb, and Equalizer fixtures exercise the
  real editors with zero failed commands. Reports are under
  `/private/tmp/cycle-v2-*-precision-report.json`.
- Production screenshot:
  `/private/tmp/cycle-v2-property-precision-after.png`. The indicator remains
  visually centred and representative values read as `2 beats`, `80%`, `50%`,
  and `10` plus the multiplication sign without redundant decimals.
- The Reverb launch logged an unrelated intermittent CoreMIDI assertion, now
  recorded in `ui-bugs.md`; all fixture commands still passed.
- The complete Cycle V2 run passes 10,274 of 10,275 assertions; its sole
  failure remains the pre-existing hit-router hover-help assertion recorded in
  `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-property-precision-tests-junit.xml`. A compilation
  database remains unavailable for focused clang-tidy.

### Deferred App-Wide Work

The performance keyboard, output meters, palettes, dock sizing, and complete
canvas chrome remain outside these slices. Their audit findings are subsequent
design work. The shared proportion, spacing, focus, and precision principles
apply when those components are scheduled, but this TDD does not authorize
their implementation.

## Verification

### Geometry And Presentation

- Unit tests calculate visible track, usable track, hit bounds, label bounds,
  value bounds, and focus-ring bounds at the minimum, preferred, and a wider
  panel size.
- No ordinary continuous row reports less than 140 pixels of usable track; a
  narrow layout must select the compact two-line variant instead.
- Hit targets do not overlap adjacent rows or detach from their visible
  affordances.
- Value fields accommodate minimum, maximum, signed, and unit-bearing strings
  without clipping.
- Guide, Waveshaper, and IR automation state exports include enough bounds and
  formatted values to make fixture assertions stable without blessing private
  implementation details.

### Interaction And Publication

Focused tests and fixtures exercise the real routed sequence for:

- ordinary drag with at least two transient updates, commit, visible/model
  effect, and one undo;
- Shift-drag entering and leaving fine mode without a value jump;
- keyboard focus traversal, arrows, Shift-arrows, Return, and Escape;
- valid direct entry, invalid entry correction, cancellation, and commit;
- double-click reset to the authoritative default;
- exact endpoints and representative interior values;
- every discrete stop for Guide-adjacent and effect controls;
- disabled, hover, pressed, focused, and invalid states; and
- close through both the focusable control and first Escape press.

Tests assert semantic values and publication outcomes, not merely constructor
success, local getters, or screenshot existence.

### Visual Evidence

For each slice:

1. Capture the current editor at production size before implementation.
2. Record a bounds and space audit with a stated purpose for every large region.
3. Capture the result using the same graph, parameter state, window size,
   backing scale, and appearance.
4. Compare silhouette, density, label/value alignment, effective track length,
   exact indicator position, empty space, focus, disabled state, and narrow
   behavior.
5. Keep the before/after screenshot and filtered automation log paths in the
   implementation evidence.

Screenshots prove appearance. Semantic tests prove gestures, undo, publication,
and downstream effects. Neither substitutes for the other.

### Build And Style

- Run `git diff --check` for every slice.
- Review every changed production file against `docs/style-guide.md`.
- Run focused clang-tidy checks where the affected targets provide a valid
  compilation database.
- Run the focused Cycle V2 editor tests and automation fixture for the migrated
  domain.
- Build Standalone Debug with `--parallel 10` or higher.
- Read filtered launch logs first and record incidental assertions or crashes
  in `docs/TDD/ui-bugs.md`.

## Negative Boundaries

- Do not add property or domain behavior to `NodeCanvas`, `NodeEditorHost`, or
  another generic switchboard.
- Do not introduce a universal schema that chooses controls, mappings,
  formatters, or layout by `NodeKind` or parameter ID.
- Do not copy Delay/Reverb slider behavior into a new shared implementation;
  extract the mature mechanics and keep domain specializations explicit.
- Do not copy Curve transaction or publication behavior into generic
  presentation controls.
- Do not move Voice Context domain semantics into a generic editor component.
- Do not reproduce DSP transfer functions or invent semantic units in UI code.
- Do not expose raw normalized values where an authoritative musical or signal
  value exists.
- Do not accept a shorter visible track as a substitute for fine adjustment or
  direct entry.
- Do not enlarge the thumb to solve hit acquisition; expand hit geometry
  independently.
- Do not allow large panel bounds, nested padding, or unused rail height without
  a documented information or interaction purpose.
- Do not make unrelated keyboard, meter, dock, or canvas redesign part of these
  migration slices.

## Deletion Targets

- Delete the private `RailLookAndFeel` and duplicated generic label/button
  styling from `CurveEditorPrimitives.cpp` once the shared presentation core
  owns them.
- Delete Guide's local `showCompactValue` helper.
- Delete local rail widths and row constants replaced by the validated shared
  layout contract, retaining only domain-specific panel proportions.
- Delete duplicated generic effect slider chrome, row layout, and value-field
  presentation from the mixed effect editor as domain editors migrate.
- Delete duplicated Voice Context track/thumb/focus presentation only where the
  shared core can express it without absorbing Voice domain behavior.
- Remove repeated control colour, font, spacing, focus, and state literals from
  all completed migration targets.

Remaining compatibility or duplicated generic presentation prevents this TDD
from reaching `Implemented` unless explicitly accepted as a stable domain
specialization.

## Completion Criteria

- Guide, Waveshaper, IR, Voice Context, Delay, and Reverb share one observable
  property-control grammar for labels, values, precision, state, focus, reset,
  and row rhythm while retaining domain-appropriate macro layouts.
- Every sound-affecting property in those panels is visible or has an explicit
  disclosure, and visible continuous properties show a semantic value.
- Every precision-sensitive control has a documented adjustment budget,
  sufficient usable travel or an honest alternate precision path, and an exact
  indicator.
- The Guide rail no longer compresses five-decimal controls into a tiny track,
  and its editor width is content-driven rather than canvas-filling.
- Waveshaper preserves a credible square transfer view and exposes semantic Pre
  Gain, Post Gain, and antialiasing values without stranded rail space.
- IR explains its time-domain width, exposes semantic values, and gives Load,
  Unload, and Model a truthful action hierarchy.
- Focus, keyboard operation, direct entry, fine adjustment, reset, gesture
  publication, downstream refresh, and undo pass through real event paths.
- Generic shared code contains no concrete node kinds, parameter IDs, DSP
  mappings, resource commands, or domain-specific layout branches.
- All deletion targets are complete, focused tests and fixtures pass, final
  production-size screenshots satisfy the space and precision audits, and the
  Standalone Debug build succeeds.

## Implementation Review Evidence

Before changing status to `Implemented`, record:

- production lines added and removed, excluding file moves;
- largest changed production files;
- new node-kind or parameter-ID branches, which should be zero in shared code;
- mature implementations extracted or reused unchanged;
- remaining duplicated presentation or compatibility code;
- per-control adjustment budgets;
- focused semantic, interaction, accessibility, and automation results;
- before/after production-size screenshot paths; and
- final build and filtered-log results.

### Slice 1: Shared Core And Guide (2026-08-26)

Implemented:

- Added a presentation-only property-control core under `UI/Editors` with
  shared geometry, precision slider mechanics, ordinary/compact row layout,
  semantic value entry, invalid-entry state, focus/hover/disabled painting,
  and shared label/button styling.
- Made `CurveEditorPrimitives` compose that core while leaving the existing
  Curve transaction and model-publication lifecycle unchanged.
- Migrated Guide Noise, DC Offset, and Phase to explicit percentages with
  one-decimal readouts, direct entry, 1% arrow steps, 0.1% Shift-arrow steps,
  Shift-drag velocity adjustment, and double-click reset to zero.
- Expanded the Guide rail from 236 to 336 pixels, producing 140 pixels of
  usable track, and constrained the editor to 1,100 by 560 pixels.
- Replaced the painted close hit region with a focusable semantic button and
  exposed Guide subcontrols through Guide-owned automation targets.

Adjustment budget for all three Guide amounts:

| Field | Value |
| --- | --- |
| Domain | 0.0-100.0% |
| Stored resolution | 0.001% without rebinding quantization |
| Meaningful displayed/fine increment | 0.1% |
| Ordinary keyboard increment | 1.0% |
| `D` | 140 px at preferred production size |
| `R` | 1,000 displayed/fine increments |
| Ordinary mapping | Absolute horizontal drag |
| Fine mapping | Shift velocity drag; 0.1% Shift-arrow |
| Alternate precision path | Editable percentage field |
| Indication | 8 by 14 px thumb with an exact centre hairline and numeric readout |

Review evidence:

- Production diff: 694 lines added and 144 removed, including 499 lines in
  four new shared-core files. Largest new implementation files are
  `PropertyControls.cpp` (269 lines) and
  `PropertyControlLookAndFeel.cpp` (117 lines); the Guide migration adds 115
  and removes 28 lines in its implementation.
- New node-kind branches: zero. Shared code contains no parameter IDs, DSP
  mappings, Guide ownership, graph mutation, or undo behavior. Guide semantic
  automation IDs remain in the Guide editor rather than `NodeCanvas`.
- Reused unchanged: `CurveExpandedEditorComponent` transaction/publication,
  Guide resource ownership, model preparation, downstream scheduling, and
  DSP depth semantics.
- At completion of this first slice, Waveshaper, IR, Voice Context, Delay,
  Reverb, and Equalizer remained scheduled for the later slices recorded
  above, so the TDD was intentionally not marked `Implemented`.
- Focused tests: property controls 38 assertions; Guide host/interaction 26;
  Guide graph gesture 16; Guide causal runtime gesture 16. All pass.
- Real-input automation:
  `/private/tmp/cycle-v2-guide-precision-controls-final-report.json` passes 19
  commands covering ordinary drag, Shift-drag, model publication, reset, and
  semantic close. Filtered log:
  `/private/tmp/cycle-v2-guide-precision-controls-final-logs.txt`.
- Production screenshots: before
  `/private/tmp/cycle-v2-guide-precision-before.png`; after
  `/private/tmp/cycle-v2-guide-precision-final.png`. Final screenshot report
  has zero failed commands; filtered log:
  `/private/tmp/cycle-v2-guide-precision-final-logs.txt`.
- Complete Cycle V2 suite: 505 of 506 cases and 10,155 of 10,156 assertions
  pass. The consistently reproducible, unrelated hit-router hover-help failure
  is recorded in `docs/TDD/ui-bugs.md` with JUnit evidence at
  `/private/tmp/cycle-v2-tests-junit.xml`.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. `clang-tidy` and a compilation database were unavailable in
  the configured environment. No DSP or visualization hot loop changed.

### Slice 2: Waveshaper (2026-08-26)

Implemented:

- Extracted the Cycle 1 `-45` to `+45 dB` gain mapping and inverse into
  `EffectParameterMapping`; both the Waveshaper DSP configuration and editor
  now consume that authoritative mapping.
- Migrated Pre Gain and Post Gain to shared 140-pixel precision rows with
  signed one-decimal dB readouts, semantic dB entry, 1 dB arrow steps, 0.1 dB
  Shift-arrow steps, Shift-drag, and 0 dB reset.
- Exposed antialiasing as a keyboard-operable selector whose complete value
  set is `1x`, `2x`, `4x`, and `8x`.
- Changed the production editor bounds from 540 by 360 to 760 by 400, allocated
  a 336-pixel property rail, preserved a square 318-pixel transfer view, and
  aligned the property group to the top grid instead of centring a sparse
  control island.
- Added generic component-ID automation discovery to `NodeEditorHost`; concrete
  editors own their semantic IDs while the host and inspector only translate
  component bounds.

Adjustment budget for Pre Gain and Post Gain:

| Field | Value |
| --- | --- |
| Domain | -45.0 to +45.0 dB |
| Meaningful displayed/fine increment | 0.1 dB |
| Ordinary keyboard increment | 1.0 dB |
| `D` | 140 px at production size |
| `R` | 900 fine increments |
| Ordinary mapping | Absolute horizontal drag |
| Fine mapping | Shift velocity drag; 0.1 dB Shift-arrow |
| Alternate precision path | Editable signed dB field |
| Indication | Exact centre hairline plus signed dB readout |

Review evidence:

- Production diff for this slice: 173 lines added and 48 removed. The largest
  changed implementation is `WaveshaperEditorComponent.cpp` at 167 total
  lines. Generic shared code gains no node-kind, parameter-ID, DSP, or undo
  branches.
- Reused unchanged: Curve gesture publication, one-gesture undo, transfer-curve
  interaction/rasterization, preview invalidation, and oversampling DSP
  behavior. The former private DSP gain helper is deleted.
- Focused tests pass: authoritative effect mappings 63 assertions; Waveshaper
  semantic editor geometry, entry, validation, and discrete values 17;
  generic editor-host automation targeting 23; view bounds 33; downstream
  traversal/audio 15; configuration publication 7.
- Real-input automation:
  `/private/tmp/cycle-v2-waveshaper-properties-report.json` has zero failed
  commands and covers two drag updates, commit, visible dB update, model
  publication, undo, Shift-drag, and reset. Filtered log:
  `/private/tmp/cycle-v2-waveshaper-properties-logs.txt`.
- Production screenshots: before
  `/private/tmp/cycle-v2-waveshaper-properties-before.png`; after
  `/private/tmp/cycle-v2-waveshaper-properties-after.png`. The after report has
  zero failed commands; filtered log:
  `/private/tmp/cycle-v2-waveshaper-properties-after-logs.txt`.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. No DSP or visualization hot-loop implementation changed;
  DSP now calls the extracted scalar mapping only during configuration.

### Slice 3: Impulse Response Properties (2026-08-26)

Implemented:

- Migrated Size, Post Gain, and High Pass to shared precision rows with 140
  pixels of usable travel and 72-pixel semantic value fields.
- Reused the authoritative eight-stop impulse-length mapping and exposed exact
  values from `128 smp` through `16384 smp`; direct entry accepts only those
  powers of two and reset returns to `1024 smp`.
- Added application-neutral inverse helpers beside the existing IR DSP mapping
  for the actual exponential post gain and cubic prefilter amount. The editor
  displays the real post gain (approximately -43.4 to +43.4 dB) and high-pass
  cutoff as percent of Nyquist rather than copying Cycle 1's inaccurate
  -30-to-+30 dB presentation or exposing raw normalized values.
- Added domain-aware keyboard stepping: Post Gain uses 1 dB and 0.1 dB steps;
  High Pass uses 1% and 0.1% of Nyquist steps after its cubic mapping.
- Reduced the preferred editor from 1,050 by 470 to 900 by 430 pixels, expanded
  the property rail from 212 to 348 pixels, and added five sample landmarks to
  explain the wide time-domain view.
- Removed the visible Load, Unload, and Model buttons because all three were
  dead controls in Cycle V2. Automation explicitly reports that resource
  actions are unavailable rather than presenting false affordances.

Adjustment budgets:

| Control | Domain | Ordinary / fine step | `D` | Alternate precision path |
| --- | --- | --- | ---: | --- |
| Size | 128-16,384 samples, 8 ordered powers of two | one discrete stop | 140 px | exact sample entry |
| Post Gain | approximately -43.4 to +43.4 dB | 1 dB / 0.1 dB | 140 px | signed dB entry |
| High Pass | 0-100% of Nyquist | 1% / 0.1% Nyquist | 140 px | percentage entry |

Resource-action boundary:

- The authoritative Cycle 1 implementation is `IrModellerUI`: Load imports an
  external impulse, Model imports audio and converts it into the editable
  curve, and Unload removes the external wave resource.
- Cycle V2 currently has no wave-resource document model or semantic command
  service through which an editor can perform those operations. The existing
  Curve publication contract can update curve and parameter state, but cannot
  own a file chooser, imported audio lifetime, serialization, undo, or DSP
  resource replacement.
- A future stable implementation should add domain commands for import,
  import-and-model, and unload. The IR editor should then present Load as the
  primary import action, Model as the secondary conversion action, and Unload
  as a contextual destructive action visible only when an external resource
  exists. The commands must reuse Cycle 1's mature import/modelling behavior
  through extraction; the editor must not copy it.
- Until that boundary exists, adding callbacks, local resource ownership, or
  approximate modelling here would violate the graph mutation and reuse rules.
  Slice 3 therefore remains partial at this explicit architectural boundary.

Reuse audit update (2026-08-27):

- Cycle 1 does not currently expose these operations as an extractable domain
  service. `IrModellerUI::buttonClicked` asks the application `Dialogs` object
  to load directly into the live `IrModeller::PitchedSample`; the DSP object
  then trims, resizes, rasterizes, and switches its private `usingWavFile`
  state through pending actions.
- `IrModellerUI::modelLoadedWave` owns the `AutoModeller` call, mutates the
  editor mesh while holding both audio and vertex locks, changes the Size
  parameter, and explicitly records the edit as having no undo. Load follows
  the same no-undo path. Cycle 1 persistence stores an external file path and
  attempts to reload it later.
- A narrow adapter therefore cannot reuse the behavior unchanged: it would
  either retain Cycle 1 UI/DSP ownership inside Cycle V2 or copy trimming,
  modelling, resource lifetime, and state switching into a new command. The
  stable solution requires extracting a UI-independent import/modelling core
  and defining durable resource state before adding editor actions.
- The next design must decide whether imported audio is embedded, copied into
  a project asset store, or retained as an external reference; what immutable
  payload or asset identity undo restores; whether direct-audio and modelled
  curve modes belong in the IR node model or a separate resource object; and
  where asynchronous file selection ends and the semantic graph command
  begins. These choices affect document portability, missing-file behavior,
  undo memory, serialization, compilation, and audio-thread publication, so
  this UI TDD does not select them implicitly.

Review evidence:

- Shared presentation gained only a generic semantic keyboard-step callback
  and configurable value width; no node kind, parameter ID, DSP behavior, or
  resource command entered generic code.
- Production diff for this slice: 263 lines added and 37 removed across eight
  production files. The largest changed implementation is the domain-owned
  `ImpulseResponseEditorComponent.cpp` at 279 total lines.
- Focused tests pass: authoritative mappings 83 assertions; shared property
  geometry and semantic stepping 45; IR semantic geometry, entry, validation,
  landmarks, unavailable actions, and keyboard publication 27.
- Real-input automation:
  `/private/tmp/cycle-v2-ir-properties-report.json` has zero failed commands
  and covers production geometry, semantic displays, two drag updates, commit,
  model publication, undo, Shift-drag, reset, and the Size default. Filtered
  log: `/private/tmp/cycle-v2-ir-properties-logs.txt`.
- Production screenshots: before
  `/private/tmp/cycle-v2-ir-properties-before.png`; after
  `/private/tmp/cycle-v2-ir-properties-after.png`. The after report has zero
  failed commands; filtered log:
  `/private/tmp/cycle-v2-ir-properties-after-logs.txt`.
- Standalone Debug builds successfully with `--parallel 10`; `git diff
  --check` passes. No DSP or visualization hot loop changed; the added mapping
  helpers run only for UI/configuration conversion. The complete Cycle V2 run
  passes 10,207 of 10,208 assertions; its sole failure is the pre-existing
  hit-router hover-help assertion recorded in `ui-bugs.md`. JUnit evidence:
  `/private/tmp/cycle-v2-ir-tests-junit.xml`. A compilation database remains
  unavailable for focused clang-tidy.
