# Cycle V2 Expanded Effect Controls

Status: Implemented

## Objective

Unify expanded IR, Waveshaper, Delay, Reverb, Equalizer, and Unison controls.
Move enablement into one icon toggle beside Close, then apply one property-grid
and content-sized choice-control language without erasing domain differences.

## Authoritative Implementations

- `EditorChromeLayout` owns full and embedded editor header geometry.
- The existing editor controls and `NodeEditorCommands` callbacks own semantic
  mutation, complete gestures, rebind, and undo. No enablement state moves into
  chrome.
- `PropertyControlLookAndFeel` and `PropertyControlMetrics` own the established
  slider/readout language.
- Cycle 1's electricity/bolt toggle supplies the interaction metaphor only. A
  new native SVG must use Cycle V2's icon grammar and resource pipeline; the
  Cycle 1 bitmap atlas is not copied.

## Design

Add one shared `EffectEnableButton`, implemented as an accessible JUCE toggle
button with a cached semantic SVG. Full and embedded header layouts reserve the
same 28 x 28 px action slot immediately left of Close with an 8 px gap. Enabled
state uses icon, fill, and border-weight changes so colour is not the only cue;
disabled state retains a visible bolt. Hover, pressed, keyboard focus, tooltip,
and accessibility state come from the same component in every editor.

Curve-based effects register the button with their existing expanded-editor
base so it occupies header coordinates rather than pretending the first
property row is chrome. Ordinary component editors use the same header layout
directly. Each editor retains its current `enabled` parameter callback and
binding; this is a presentation extraction, not a command adapter.

After removal of local Enabled rows, IR and Waveshaper return the space to their
property grids. Antialiasing uses a content-sized choice width based on `8x`
plus standard insets rather than consuming the rest of the rail. IR, Delay,
Waveshaper, and Reverb use the same row height, label allocation, gaps, and
readout treatment; preview/panel allocations remain domain-specific.

## Test-First Contract

1. Full and embedded headers place a 28 x 28 enable action immediately left of
   Close, with equal centers, 8 px separation, title clearance, and containment
   at every supported editor size.
2. The SVG validates, resolves through the shared UI-icon registry, and renders
   non-blank at 16 px and the production button size.
3. The shared button exposes tooltip, toggle accessibility, keyboard focus, and
   visibly distinct enabled/disabled raster checks that do not depend on colour
   alone.
4. Every affected editor publishes identical enable-button bounds and toggles
   its durable `enabled` parameter through a production pointer target; rebind
   and undo restore both value and visible state.
5. IR and Waveshaper no longer allocate an Enabled property row. Their first
   property begins at the standard content inset, and Waveshaper Antialiasing is
   no wider than its longest option plus standard horizontal insets.
6. Production screenshots compare all six editors at their actual sizes.

## Negative Boundaries

- Do not implement bypass locally, change DSP enable semantics, or add a second
  enabled state.
- Do not copy the Cycle 1 raster icon or draw six local bolt paths.
- Do not make the 16 px glyph the hit target; the button remains 28 px.
- Do not communicate state by tint alone.
- Do not force effect plots or domain-specific actions into identical geometry.

## Implementation Slices

1. Add the shared SVG/button and full/embedded header geometry with native-size
   raster and layout tests.
2. Migrate Delay, Reverb, Equalizer, and Unison without changing callbacks.
3. Add the curve-editor header action boundary and migrate IR and Waveshaper,
   reclaiming their local row space. Completed.
4. Normalize the property grids and content-size Waveshaper Antialiasing; add
   cross-editor automation and screenshot evidence. Completed.

## Completion Criteria

- All six effects use the same enablement component and header placement.
- Rebind, pointer/keyboard activation, undo, compact layout, and screenshot
  criteria pass.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, SVG validation,
  style review, and production-diff review are complete.

## Implementation Evidence

- Slices 1 and 2 add a native `effectEnable.svg`, the shared accessible
  `EffectEnableButton`, and common full/embedded header geometry. Enabled and
  bypassed states differ by fill and border weight as well as colour; the 16 px
  bolt remains inside a 28 px keyboard-focusable target.
- Delay, Reverb, Equalizer, and Unison retain their existing parameter callbacks
  and IDs while replacing the textual toggle with the shared header action.
  Their four production editor fixtures pass, including save/reopen behavior.
- The curve-editor base now accepts one presentation-only header action. IR and
  Waveshaper register the shared toggle there, retain their existing discrete
  publication path, and begin their real property rows at the standard rail
  inset. Focused tests cover the geometry and complete transaction sequence;
  both production property fixtures cover pointer activation and undo.
- IR and Waveshaper now use the same 56 px compact property-row rhythm as Delay
  and Reverb. Waveshaper's Antialiasing choice is 72 px wide, while the reclaimed
  Enabled-row space returns to the real properties.
- A focused cross-editor test verifies standardized enable-button geometry for
  Delay, Reverb, Equalizer, and Unison; the curve-editor tests verify the same
  embedded-header contract for IR and Waveshaper. The shared button also passes
  enabled/bypassed raster, tooltip, keyboard-focus, and Return-key activation
  checks.
- The six-effect production fixture resolves each semantic button target,
  toggles the durable parameter, and verifies undo. OS-level production-size
  captures for all six editors confirm the same bolt placement and property-row
  rhythm, including the OpenGL-composited IR and Waveshaper editors.
- Standalone Debug builds and packages the SVG. The Cycle V2 suite passes 10,797
  of 10,798 assertions; the sole failure is the pre-existing edge-hover help
  assertion in `TestNodeCanvasHitRouter.cpp:66`.
