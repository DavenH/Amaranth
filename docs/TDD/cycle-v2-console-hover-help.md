# Cycle V2 Console Hover Help

## Contract

Hover explanations belong in the existing upper-left canvas console. Control
tooltips, dynamic selector hints, and morph shortcut help must not cover the
canvas or editor. Leaving a control restores the ordinary canvas/edit status.

## Ownership and complexity

`NodeCanvasPresentation::paintStatus` and `CanvasUtilityDock::layout` remain the
console rendering and geometry owners. JUCE `TooltipClient` remains the text
contract, including dynamic values. The canvas's existing timer resolves the
native pointer position through JUCE component hit-testing, then its ancestors
within this app window; it caches only
the current string and requests a status-region repaint when that string changes.
Hit-testing follows the component hierarchy; ancestor text lookup is O(depth),
independent of graph or mesh size. No document,
audio, undo, preparation, or graph-publication work belongs to this path.

Delete the global `TooltipWindow`, segmented-selector bubble/timer, and morph
shortcut bubble/timer. Preserve the 24 px morph help target, its accessible
description, and existing keyboard behavior; expose its text as a tooltip.
Selector providers continue computing help from current model state.

`NodeCanvas` remains over the size review threshold. This change only adds
timer-to-presentation delegation and console readback; help lookup lives in
the existing chrome presentation owner. Canvas interaction/overlay extraction
in `refactors.md` remains the larger decomposition plan. No new domain algorithm
or interaction state machine is added to that orchestrator. The selector and
morph helpers become smaller through deletion of their overlay machinery.

## Verification

Focused status tests cover descendant targeting, nearest help, dynamic values,
leaving controls/window, and dynamic selector providers. Existing Trimesh and
Envelope tests cover the retained shortcut target, help content, and keyboard
gestures. Native pointer checks verify console publication and no bubble after
the former tooltip delay. `snapshotState.consoleText` exposes the rendered text.

## Top-bar proposal scope

The separate mockups explore an integrated 56 px strip, a 72 px instrument-first
strip, and an 84 px two-row compact-window layout. Preserve preview Play/Record/
Edit, octave controls, MOD and graph-mapped CCs. Use radial controls with relative
dragging, fine adjustment, and numeric values; visual radius must not determine
drag resolution. The supplied reference governs the shallow two-octave keyboard.
These layouts are proposals; production keyboard/layout changes await selection.

## Results

The standalone app and test target build. Five focused status/Trimesh/Envelope
cases pass 120 assertions. Native MOD, octave-down and Play hover checks pass,
including departure and a dwell beyond the previous tooltip delay. Evidence:
`.context/console-hover-native-results.json`, `.context/console-hover-tests.log`.
The architecture audit and semantic code-map check pass; existing canvas size
triggers retain the extraction plan above.

The older `cycle-v2-agent-hover-help.json` fixture is stale: it addresses removed
`magnitudeLayer1Process` and its synthetic-pointer repaint totals differ from
native timer activity. Its failure is recorded in `ui-bugs.md`; it does not
establish a regression in the new native control-help checks.
