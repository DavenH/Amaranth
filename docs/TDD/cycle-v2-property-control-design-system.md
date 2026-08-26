# Cycle V2 Property Control Design System

## Status

Proposed (2026-08-26).

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
