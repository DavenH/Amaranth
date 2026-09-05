# Native macOS Property Panel Reference Profile

Use this profile when the explicit goal is a macOS inspector or property panel
that expert judges should not be able to distinguish from a system application.
The strongest route is to use AppKit or SwiftUI controls and semantic system
materials directly. A custom renderer must reproduce both appearance and
behavior across states; a convincing static screenshot is not sufficient.

Start with Apple's current
[Human Interface Guidelines](https://developer.apple.com/design/human-interface-guidelines/)
and consult the guidance for layout, controls, typography, color, materials, and
accessibility before setting exact metrics. Current native controls from the
targeted macOS release are the final reference when documentation is qualitative.

## Reference Setup

Define the target before judging:

- macOS release and hardware scale factor;
- light and dark appearance;
- system accent and highlight colors;
- panel role: attached inspector, utility window, settings pane, or popover;
- width range, titlebar or toolbar context, and whether the panel scrolls;
- representative content in English and one longer localization;
- enabled, disabled, hover, pressed, focused, mixed, error, and empty states.

Capture at least two first-party comparison panels with the same role. Compare
at 1:1 points and pixels; do not resize screenshots to make them agree.

## Pass/Fail Criteria

### 1. Window and panel structure

Pass only if:

- the panel occupies the conventional location for its role and uses native
  titlebar, toolbar, sidebar, divider, scrolling, and safe-area behavior;
- resizing follows macOS conventions, with a credible minimum width and no
  arbitrary dead bands, clipped focus rings, or stranded empty columns;
- the background uses the appropriate semantic window, control, or sidebar
  material and changes correctly with appearance, key-window state,
  transparency, and increased-contrast settings;
- separators are optically one device pixel where native separators would be,
  aligned to the backing scale rather than rendered as blurry half-pixels;
- corners, popovers, sheets, shadows, and attachment points match the native
  container type rather than sharing one product-wide decoration.

Fail for a web-style card stack, gratuitous rounded rectangles, heavy outlines,
permanent drop shadows inside the panel, or a custom header that resembles a
mobile navigation bar.

### 2. Density, grid, and grouping

Pass only if:

- the panel uses one small spacing scale with consistent outer inset, row gap,
  control gap, section gap, and separator treatment;
- labels, control leading edges, value fields, baselines, and disclosure arrows
  form stable alignment lines across sections;
- related controls are closer to one another than to adjacent groups;
- a section title, separator, disclosure group, or background change appears
  only when it improves scanning;
- dense property rows remain calm and readable without consuming a separate
  full-width line for every short label or icon;
- empty space is mainly at the panel edge or after content, not inserted between
  a label and its value or between paired meters or controls.

Fail if spacing varies by eye from row to row, nested containers compound their
padding, or a simple property panel has the loose vertical rhythm of a website.

### 3. Typography and copy

Pass only if:

- text uses the system UI font through semantic platform styles, with native
  rendering, weight, leading, truncation, and baseline placement;
- the type scale is restrained: ordinary labels, secondary values, section
  labels, and titles differ only as much as hierarchy requires;
- labels use macOS capitalization and terminology consistently and omit
  decorative punctuation unless the native control convention requires it;
- units and numeric values use consistent formatting and enough width for the
  complete valid range;
- truncation, wrapping, tooltips, and localization behave intentionally;
- disabled and secondary text remain readable in every appearance.

Fail for a near-match font, manually tuned letter spacing, all-uppercase section
labels used as decoration, centered form labels, or placeholder text serving as
the only persistent label.

### 4. Native control selection and sizing

Pass only if:

- each action uses the same control class a first-party macOS panel would use:
  checkbox, radio group, pop-up button, segmented control, text field, stepper,
  slider, color well, disclosure control, or table as appropriate;
- one coherent AppKit or SwiftUI control-size family is used within a region,
  with exceptions justified by hierarchy rather than available space;
- control heights, corner radii, bezel treatment, arrows, checkmarks, ticks,
  thumb geometry, and internal text insets match current native controls;
- labels and controls are baseline-aligned and retain complete focus rings;
- controls do not stretch merely to fill the panel: pop-up buttons and fields
  are as wide as their content and localization require, while appropriate
  flexible controls absorb remaining width;
- compact visible controls still have reliable hit targets without causing
  neighboring targets to overlap ambiguously.

Fail for mobile switches in an ordinary desktop form, oversized pill buttons,
custom chevrons where a native disclosure control belongs, mixed control eras,
or sliders whose thumb hides the value position.

### 5. Color, materials, and iconography

Pass only if:

- semantic system colors are used for text, fills, separators, selection,
  keyboard focus, disabled state, and destructive or warning meaning;
- the user's accent color propagates anywhere the native equivalent would use
  it, without recoloring unrelated decoration;
- translucency and vibrancy are used only by containers that conventionally own
  them and remain legible when transparency is reduced;
- symbols use SF Symbols or a visually compatible semantic icon set at the
  native optical size, weight, baseline, and rendering mode;
- icons communicate an action or state and are not used as ambient decoration;
- every state remains understandable without color alone.

Fail for hard-coded near-system grays, a brand accent replacing keyboard focus,
colored icons with no semantic reason, or a glass effect painted over ordinary
form rows.

### 6. Pointer, keyboard, and focus behavior

Pass only if:

- hit testing matches visible controls, with the standard arrow, I-beam,
  pointing-hand, resize, and drag cursors used in their native contexts;
- Tab and Shift-Tab follow a predictable control order, Full Keyboard Access is
  respected, and the focus ring is native in color, shape, and animation;
- Space, Return, Escape, arrows, Page Up or Down, and standard editing shortcuts
  perform the expected action for the focused control;
- sliders respond to click, drag, wheel or trackpad, and keyboard input with
  native clamping, step, and fine-control semantics appropriate to the value;
- text editing supports selection, undo, cut, copy, paste, contextual menus, and
  standard field-editor behavior;
- menus open, highlight, type-select, dismiss, and return focus like native
  menus; sheets and popovers dismiss through the expected routes;
- hover, pressed, mixed, default, cancel, validation, and disabled states change
  at the same moments as their native counterparts.

Fail if only the idle screenshot matches, keyboard traversal is absent, focus
is painted as a generic outline, or custom pointer capture leaves controls stuck
after cancellation.

### 7. Property editing semantics

Pass only if:

- selection changes update the inspector without visible stale-state flashes or
  unexplained layout jumps;
- labels name the property, controls edit it, and units or reset affordances are
  attached to the same row without ambiguity;
- continuous changes preview immediately when safe, while expensive or risky
  changes use an explicit, conventional commit boundary;
- invalid input is preserved long enough to correct, identified locally without
  moving unrelated rows, and never silently coerced to a surprising value;
- mixed multi-selection values use the native mixed or indeterminate convention;
- undo groups one semantic edit or gesture, not every drag frame and not an
  unrelated batch of changes;
- destructive actions are visually and spatially separated in proportion to
  their risk and request confirmation only when recovery is genuinely hard.

Fail if every row has an Apply button, values commit at inconsistent moments,
selection is lost during editing, or transient help text causes continuous
panel reflow.

### 8. Adaptation and accessibility

Pass only if:

- the panel works in light, dark, increased-contrast, reduced-transparency, and
  reduced-motion configurations relevant to its effects;
- all controls expose correct accessibility roles, labels, values, help,
  enabled state, relationships, and actions to VoiceOver;
- the accessibility order matches the visual and task order and grouped
  properties are announced as coherent groups;
- information conveyed by hue, animation, hover, or position has another cue;
- localized labels and larger accessibility text settings do not overlap,
  truncate critical meaning, or detach labels from controls;
- animations are brief, interruptible, and limited to explaining continuity or
  state change.

Fail if appearance is a fixed palette, VoiceOver reads internal component names,
or the panel is only usable with precise pointer movement.

### 9. Rendering fidelity

Pass only if:

- text, icons, separators, and control outlines are crisp on both 1x and 2x
  backing scales;
- focus rings, clipping, shadows, antialiasing, and compositing match native
  output at edges and during animation;
- there is no custom gamma, font rasterization, fractional translation, or
  resampling that makes the panel subtly softer than neighboring native UI;
- scrolling, resizing, live value changes, and appearance switching do not
  flicker, tear, lag, or expose stale backing content.

Fail for a panel that matches only after image scaling, uses screenshot-derived
assets for controls, or reveals inconsistent pixel alignment between states.

## Blind Evaluation Protocol

Use a corpus rather than one hero screenshot:

1. Pair the candidate with first-party panels of the same role and macOS release.
2. Capture light and dark appearance at 1x and 2x, key and inactive window,
   default and non-default accent, and at least one accessibility configuration.
3. Include narrow and wide widths, long localization, scrolling, focus, hover,
   pressed, disabled, mixed, validation, and open-menu or popover states.
4. Record short interactions: keyboard traversal, text editing, menu selection,
   slider adjustment, resize, selection change, undo, and dismissal.
5. Randomize provenance and ask macOS-experienced judges both to classify the
   source and to mark the first discrepancy they notice.

The panel passes only if classification remains at chance across the corpus and
no repeated discrepancy identifies custom provenance. A single convincing
static image is evidence of screenshot matching, not native fidelity.

## Specification Template

Before implementation, write a compact table for each property group:

| Item | Semantic role | Native control | Visual bounds | Hit bounds | Alignment | Value and commit behavior | States |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Example | Continuous amount | Slider and value field | Reference-derived | May exceed track | Label baseline and shared track edge | Live preview; one undo gesture | Default, focus, disabled, mixed |

Also record the panel width range, spacing tokens, label-column policy,
scrolling policy, appearance and accessibility matrix, and the exact first-party
reference captures used for comparison.
