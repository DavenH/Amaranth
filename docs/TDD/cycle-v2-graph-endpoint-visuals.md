# Cycle V2 graph endpoint visuals

## Status

Implemented.

## Design

Voice Output terminates each voice, and Global Input starts the mixed global
chain. They form a paired handoff, not processing stages. Both use a compact
roundel with matched inward/outward icons and a short two-line label on the
side without a cable. Their 116 × 116 world-unit bounds put the existing
side port at the vertical centre. The final Output retains its meter and adds
a speaker icon in the header. Signal-domain cable colours remain authoritative;
endpoint identity comes from silhouette and icon shape.

`NodeDefinition` owns natural size, `GraphEndpointNodeRenderer` owns the roundel
and caption geometry, `NodeCanvasPresentation` chooses the paint branch and
cached tile extent, and `NodeIconRenderer` remains the SVG authority for both
canvas and palette icons. Graph model, routing, and port hit testing stay
unchanged. Loading an existing preset derives the new natural size from its
node kind while retaining its stored top-left position. Paint and hit work
remain constant per node. The flat-card paint path no longer handles these
two node kinds.

The existing `NodeCanvasPresentation.cpp` was 1,432 lines before this change
and is 1,447 lines after extraction. It coordinates canvas layers, cached node
painting, overlays, and presentation status, with `NodeCanvasScene`,
`NodePreviewRenderer`, and `GraphEndpointNodeRenderer` as stable collaborators.
Endpoint caption placement and roundel drawing live only in the renderer; the
canvas presentation supplies selection and viewport facts. This change does
not add graph lifecycle, routing, or state policy to the presentation class.

## Verification

The focused `cycle-v2-agent-graph-endpoint-visuals.json` fixture passes on a
clean preset: both handoff nodes load at 116 × 116, their centres remain
selectable, and app-side 1728 × 962 screenshots show the labels, matching
icons, and selected outline. The Voice Output caption moves above the circle
when the right side of the viewport has too little space. The existing Output
meter remains in place with a speaker mark in its header. Both standalone
Debug and Release builds pass. Three targeted presentation/definition tests
pass with 57 assertions; palette icon parsing and legibility tests pass with
135 assertions. All three SVGs parse as XML. The Cycle V2 architecture audit,
`git diff --check`, and clang-tidy pass; clang-tidy reports only warnings in
unchanged lines.
