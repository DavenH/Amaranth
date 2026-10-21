# Cycle UI and UX design language

## Purpose and authority

Use this document when adding or reviewing Cycle UI. It records product-specific
decisions from the user's design feedback. The [UI design skill](../.agents/skills/ui-design/SKILL.md)
provides the general method for measuring geometry, gestures, states, and
production-size renders; this document describes how Cycle applies it. Existing
mature Cycle components and their complete interactions are the implementation
reference. When a new surface conflicts with either, inspect the live app and
resolve the difference before adding another style.

Cycle is a visual instrument and editor. Its primary views are the signal,
curve, mesh, and relationship being worked on. Controls should make those views
legible and manipulable without turning a workspace into a form or dashboard.

## Visual vocabulary

- **Base colour:** Continue the canvas's dark navy and charcoal family. Use
  neutral grey for small foreground symbols and scrollbars. Cyan is a semantic
  signal colour in specific existing views, not the default fill, outline,
  scrollbar, or decorative accent for a new surface. Do not introduce a
  muted green panel background beside the navy canvas.
- **Semantic colour:** Keep time, spectral magnitude, and spectral phase colour
  meanings consistent across ports, curves, and views. Use restrained colour
  for selection and active state; a neutral family at rest avoids unrelated
  accents competing for attention. Do not make hue the sole state cue.
- **Chrome:** Reuse the Cycle button, menu, icon, tab, popup, and scrollbar
  components and their state treatment. A row of generic outlined text buttons
  has the wrong weight for routine editor commands. Use a compact icon or
  established button treatment for a frequent direct action; use a menu for a
  collection of choices; retain words where an icon alone would be ambiguous.
- **Typography:** Name visible controls in the user's musical and visual terms.
  Keep labels concise and readable at normal canvas zoom. Use complete labels
  and units where a value matters. Do not add an explanatory sentence or a
  persistent bottom sublabel to every card. Put transient help and progress in
  the established console or hover help.
- **Geometry:** Align related content to shared edges and baselines. Apply one
  clear outer inset rather than stacking nested padding. Give the visual data
  useful area; avoid large unexplained blank regions and oversized control
  bands. Size controls by their semantic importance and required precision.

## Put an action where its object lives

The entry point must be discoverable at the moment the user needs it. Global
file operations belong in the application or File menu, with a clear way back
to imported resources. An object command belongs on that object's card,
context menu, or editor. A choice among existing Spies belongs in a menu that
names the available probes and implicit output. If a wave reference becomes a
node, its node semantics must be explicit: whether it supplies audio to the
graph, acts as an authoring reference, or both. Do not use a processing node
solely to hide an application-level import.

Respect established command scope. Undo and redo use Cycle's existing menu,
shortcuts, and editor history affordances; they should not consume a permanent
row of dock buttons. Play and Stop belong with the selected audible resource or
the established audition transport, and must make their target clear. A
comparison note should be chosen through a familiar musical control with a
visible note name/value and keyboard interaction, rather than a detached
unlabeled scalar slider.

Separate summary from editing. A compact sample or Spy card identifies the
source and shows the comparison surface. Detailed pitch analysis and pitch
envelope editing appear when the sample is expanded. The expanded editor uses
the existing two-dimensional envelope presentation and interaction language,
including its curve geometry, selection, controls, and undo. A new miniature
curve editor inside every comparison card would be a second interaction system.

In mesh editors, put Component curve below the vertex coordinate/property rows.
Depict a component as two intercepts with a wave-shaped segment between them;
keep the Guide resource glyph in the attachment selector. The two concepts
should remain recognizable together.
Place output gain directly beneath the property stack, retaining that anchor
across morph modes. Label Guide and Gain columns, keep disabled labels readable,
and show exact property values during hover and adjustment.

## Make visualizations truthful and comparable

- A Spy shows the observed processed signal; a Trimesh node shows authored mesh
  data. Keep these meanings distinct even when the views share a renderer.
- Comparable signal views use the same domain, scale, camera, axes, and display
  transfer unless a difference is explicitly shown. Preserve underlying
  samples; fix display gain, exposure, and colour mapping at the presentation
  boundary. Never silently amplify the Spy data to make its card brighter.
- Show a selected reference and pinned Spies side by side with comparable
  viewing geometry. Display source duration where it helps interpret the
  comparison, without spending a dedicated metadata strip on every card.
- A surface must remain legible across representative quiet, loud, signed, and
  spectral signals. A nearly black sample beside a bright saturated Spy is a
  failed comparison even if both textures technically rendered.
- Shading must reveal scalar structure without inventing ridges or masking
  attenuation. Test both the source data and the production-size image. A
  visually pleasing texture is insufficient when it hides or implies a false
  signal feature.

## Interaction and layout contract

1. Before implementation, capture the relevant existing editor at its actual
   window size. Identify the mature controls and their hover, active, disabled,
   focus, drag, and keyboard states. Write down the intended hierarchy and
   where each command lives.
2. Treat visible size, hit area, value indication, and manipulation mapping as
   separate properties. A thin slider can have a larger hit target; a compact
   control still needs a precise readout and useful drag range.
   Usable slider travel is an inviolable layout constraint. Reserve the shared
   140 px ordinary minimum after label, guide, value, thumb and end-inset costs
   before allocating space to visualizations or gaps. Reflow or resize the
   surrounding layout when necessary; a wider hit target does not compensate
   for a short track. Avoid excessively long tracks as well: mesh vertex rows
   use 140–220 px of visible travel at normal and compact widths. Verify actual
   painted travel and routed endpoint/intermediate gestures at both sizes.
3. Preserve canvas space. Put only the essential controls in a workspace band.
   Expand a resource for detail instead of permanently showing every edit
   control in each card. Keep a dock resizable, but size its cards and vacant
   area intentionally at common window widths.
   Opening a Trimesh editor alone must leave the comparison shelf closed.
   A View menu choice or an explicit reference import/selection opens it for
   that session; the shelf must not take canvas space or capture Spies before
   the user asks for comparison.
4. Scrollbars appear when content actually overflows and use Cycle's existing
   neutral style. They must not cover the data. A scrollable comparison rail
   should not draw both axes of scrollbars on each card by default.
5. Keep the rendered affordance and actual routed event in agreement. Test the
   full pointer and keyboard sequence, including first use, selection,
   release, undo, preset switch, and Escape. A selected card should not dim or
   lose fidelity merely because it has focus.
6. For visual changes, compare before and after screenshots at the same
   production size and state. Review silhouette, occupied content, empty
   regions, control scale, colour, labels, and surface exposure. Exercise at
   least one smaller window and one representative dense preset.

## Current comparison-dock acceptance checks

The first implementation of sample and Spy comparison exposed several failures
that this document should prevent from recurring. Review the revised dock
against these observable checks:

- Importing a reference and finding it later are discoverable through an
  established application entry point. The source of factory references is
  apparent. Factory references stay in the source menu until the user opens
  them; opening a Trimesh editor does not load one, including a reference left
  open in an earlier app launch. Active comparisons survive preset switches
  only in the current session. The UI does not require guessing what a generic
  `Reference` button does.
- Adding a Spy opens a menu of available sources; an already pinned source is
  recognizable. Commands for one card live on that card or its menu.
- The source menu shares the card row at its fixed left edge. The resize grip
  remains above it, without a separate Compare header. The dock does not
  permanently show separate text buttons for Undo, Redo, Play, and Add Spy or
  a detached note slider.
- A card at rest presents its name and surface without a miniature pitch
  editor, persistent instructional footer, or unnecessary scrollbars. Expanded
  sample editing follows the existing envelope editor. A click on a comparison
  surface selects its reference without invoking Trimesh mesh editing.
- Sample and Spy surfaces are similarly readable at the same display scale.
  Their level relationship is honest; display gain and colour transfer are
  inspectable and testable. A reference surface is not near-black while a Spy
  clips to white under comparable content.
- The dock uses Cycle's navy/charcoal workspace and neutral chrome, with
  semantic signal colour only where it communicates signal or state. The
  sample and Spy views occupy useful width rather than leaving an unexplained
  empty half-dock in the common window size.

## Evidence and related contracts

These rules summarize specific, locally recorded feedback rather than a new
palette invented for this document:

- [Sidebar authoring and icon semantics](chats/1791504256.md): cyan rejection,
  monochrome icons at rest, semantic icons, and density adapting to the
  container. The local Codex history also records the explicit grey Cycle 1
  scrollbar request at timestamp `1791503387`.
- [Cycle V2 ports and compact summaries](chats/1785606789.md): constrained
  semantic colour, full musical labels, glanceable values, and complete slider
  gestures.
- [Expanded editor hierarchy](chats/1788727897.md) and [property-control
  design system](TDD/cycle-v2-property-control-design-system.md): shared
  control groups, meaningful toolbars, space allocation, precision, and
  established action styling.
- [Effect UI visualization](chats/1784473734.md), [Spy output scaling](chats/1788967196.md),
  and [surface-colour experiments](chats/1790615037.md): DSP-faithful views,
  comparable scaling, exact observed values, and inspectable display mapping.
- [Guide and Spy workspace](chats/1786987223.md) and [shared curve
  interaction](chats/1789679961.md): resource ownership, visible comparison
  workspace, and reuse of the existing curve editor behavior.
- The comparison-dock screenshot before this review is
  `.context/comparison.png` (a local, gitignored review artifact). The user's
  2026-10-18 review supplies the current acceptance issues listed above.
