# Cycle V2 Hover Interaction Audit

Status: immediate morph-help repair implemented; repository guidance recorded.

## Scope

This audit followed a regression where the morph shortcut card appeared only
after clicking before hovering. It covers the Cycle V2 component hierarchy,
tooltip ownership, canvas hit routing, automation input, and the OpenGL canvas
underlay.

## Existing Interaction Paths

Cycle V2 has three legitimate pointer paths:

1. Ordinary JUCE child components receive native `mouseEnter`, `mouseMove`,
   and `mouseExit` callbacks. Buttons, sliders, and the morph shortcut target
   belong on this path.
2. Shapes painted directly into `NodeCanvas` have no child component to receive
   events. `NodeCanvas::updateHoverAt` and its hit router therefore own hover
   classification for nodes, cables, guides, and other canvas geometry.
3. The legacy panel bridge forwards component events into an `Interactor`.
   This remains separate from Cycle V2 canvas routing.

The OpenGL context is attached as the canvas underlay. It does not require
ordinary child controls to duplicate native event dispatch, and it was not the
cause of this regression.

## Findings

### `TooltipWindow` is not an immediate event-driven popover

JUCE 8's `TooltipWindow` polls the main `MouseInputSource` every 123 ms. Its
native state machine also suppresses display when the pointer matches the last
mouse-down position and retains a 500 ms post-dismissal interval. A zero delay
does not remove those rules. That behavior is appropriate for conventional
delayed tooltips, but not for an instructional card that must appear as soon as
the pointer enters a visible glyph.

### Tooltip ownership had become duplicated

The canvas already owns the general-purpose `TooltipWindow`. The Envelope
editor also owned one, and each morph shortcut helper added another. This left
several global mouse listeners polling the same native peer. JUCE explicitly
recommends one tooltip window for a component area.

The repair removes the Envelope-local and morph-local tooltip windows. The
canvas tooltip window remains the authority for ordinary `TooltipClient`
labels.

### Synthetic pointer commands are not native-event proof

`CycleV2AutomationInput::pointer` calls component mouse methods directly. It
does not move or update JUCE's `MouseInputSource`. Such a command can test a
component callback or domain result, but it cannot validate `TooltipWindow`,
global mouse listeners, `getComponentUnderMouse`, native enter/exit routing, or
OS cursor behavior.

Native-hover regressions therefore require a fresh app launch and actual macOS
pointer movement. The pointer must begin outside the target so an enter
transition is delivered; a component screenshot alone is insufficient.

## Implemented Boundary

The morph shortcut helper now uses JUCE's local hover primitives:

- a transparent 24-by-24 child component owns the hover target;
- native `mouseEnter`/`mouseMove` shows a JUCE `BubbleComponent`;
- native `mouseExit` hides it;
- the bubble uses JUCE's native anchored placement and does not intercept the
  pointer;
- a helper-local JUCE timer reconciles visibility against the target's screen
  bounds at 60 Hz while the glyph exists, because the OpenGL-attached macOS
  peer can report the correct component under the pointer without delivering
  the corresponding enter/move callback;
- no global listener, click path, last-mouse-down condition, or post-dismissal
  state is involved.

This is an O(1) local interaction. The helper translates a native component
hover into presentation visibility only; it does not own canvas hit testing or
editor state.

## Guardrails

- Use one general `TooltipWindow` per top-level editor area.
- Use `TooltipClient` for conventional delayed text tips.
- Use native component enter/exit with a passive `BubbleComponent` for an
  immediate anchored help card. If a production native-input test proves that
  the peer misses entry, keep reconciliation local to the target and free of
  click state.
- Use the canvas hit router only for painted geometry that has no component.
- Do not accept direct-method automation as proof of native hover behavior.
- Record both the pre-click hover and leave/dismiss states in production-size
  native captures.

Replacing JUCE rendering with an OpenGL-only UI would not remove the painted
canvas hit-routing requirement; it would expand that manual responsibility to
controls that currently receive native JUCE routing correctly. The useful
boundary is to keep ordinary controls component-native and reserve explicit
hit routing for genuinely componentless canvas geometry.

## Verification

A fresh Cycle V2 process opened the Envelope editor with the pointer parked
outside the glyph. One native macOS pointer movement into the glyph displayed
the card without a preceding click; moving back outside dismissed it. The
hover capture is `.context/native-morph-hover-final-crop.png`.

## Architecture Size Review

`EnvelopeEditorComponent.cpp` was 925 lines before this repair and is 923 lines
after it. Its responsibilities remain expanded Envelope composition, layout,
model/control synchronization, editor gesture routing, and automation-state
projection. Its stable collaborators are `CurveExpandedEditorComponent`,
`EnvelopeMorphControls`, property controls, and the shared editor command
boundary. This change removes tooltip lifecycle policy from that file; the
top-level canvas owns conventional tooltips, while the morph helper owns only
its immediate local card. No new Envelope responsibility or duplicate policy
site was introduced, so this slice does not require an additional extraction
from the already-reviewed editor file.
