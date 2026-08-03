# Cycle V2 Port And Icon Presentation

## Status

Implemented. Shared geometry, neutral presentation semantics, input-only icon
artwork, conditional icon gutters, cable endpoints, hit targets, and focused
normal/reduced-zoom fixtures are in place. This document does not change graph
types, connection compatibility, or DSP.

## Problem

Cycle V2 currently gives ordinary signals, configuration attachments,
Modulation Triple bundles, and cable endpoints separate socket geometry and
colour rules. Ports that perform the same interaction consequently appear at
different sizes. Attachment type is also encoded primarily through colour,
which will become noisy and ambiguous as the attachment vocabulary grows.

Preview nodes do not reserve a consistent place for port descriptions. A new
port can therefore collide with content, sit partly outside the node, or force
one node to invent a local layout.

## Decision

Use colour only for the three primary processing signal domains: time,
spectral magnitude, and spectral phase. All other ports and their cables use a
shared monochrome palette. Attachment semantics are communicated by a small
descriptive icon, not by assigning another colour.

The socket is always the outermost visual element at the node boundary. For a
left-side input, the visual order is:

```text
cable ───○ [icon] node content
```

Output ports remain plain sockets and do not reserve an icon gutter. The icon
is inside the destination node and adjacent to its input socket; it is never
drawn inside the socket or outside the node boundary.

Every preview node reserves the same port-icon gutter on each side that owns an
input icon, except the right side, which never takes content margin. Plain
colour-coded signal ports do not reserve a gutter. This keeps socket centres,
icons, and main content aligned across node families without shrinking previews
that have no semantic input icons.

## Shared Presentation Contract

One UI presentation primitive owns:

- socket diameter and stroke;
- hover, connected, selected, and pending-connection emphasis;
- the larger invisible hit target;
- input-icon bounds and spacing;
- colour selection from connection semantics;
- cable endpoint geometry.

All sockets are circular and have one base visual diameter. Direction is shown
by boundary position and cable direction, not by making inputs and outputs
different shapes. Interaction state may change the halo or stroke but must not
change the base diameter or move the socket centre.

The icon gutter is a layout reservation, not padding around the complete node.
Top and bottom input ports use the same socket and icon measurements, rotated
or arranged inward as appropriate. Nodes grow to fit their port stack and
content; ports do not overflow or become clipped to preserve an arbitrary node
size.

The initial semantic icons are:

| Port semantic | Icon concept |
| --- | --- |
| Modulation Triple | neutral socket with a yellow/red/blue flag; red/blue for Envelope destinations |
| Pitch envelope | note or pitch-curve mark |
| Unison configuration | fanned voice lines |
| Voice context | compact voice/context mark |
| Scratch attachment | traversal/scratch mark |

Icons are monochrome, use the same visual weight, and remain distinguishable
at normal canvas zoom. The modulation flag is the deliberate exception: its
two or three bands reuse the established morph-axis colours while the socket
and cable remain neutral. The flag silhouette and band count preserve the
distinction without colour. Exact artwork belongs to the shared icon set, not
individual node painters. A short port name remains available through hover
text and accessibility metadata; an icon is not the only machine-readable
description.

## Ownership And Migration

The authoritative graph semantics remain `Port`, `PortDomain`,
`ConnectionKind`, `AttachmentType`, and `PortPurpose`. A presentation resolver
translates those values into a small `PortVisualSemantic`; painters must not
switch on `NodeKind` to rediscover attachment meaning.

The shared primitive replaces the independent socket sizing in
`NodeCanvasPresentation`, `NodeCableRenderer`, `ModulationCableBundle`, and
`NodeCanvasScene` hit testing. Existing node-specific socket painters are
deletion targets. Composite cable rendering may retain its internal line
layout, but its endpoint must obey the shared socket contract.

The completed migration uses `NodePortGeometry` as the common owner of the
8.4 px reference socket diameter, reference zoom, independent hit padding,
icon bounds, and gutter dimensions. Ordinary, Modulation Triple, and Unison
sockets, cable endpoints, presentation bounds, and scene targets consume that
geometry.

Preview-node layout exposes conditional left, top, and bottom input-icon
gutters to node content renderers while keeping the right content margin
unchanged. Voice Context is the first custom-painter consumer, but the layout
API is node-agnostic and all standard previews consume it through
`NodePreviewRenderer`.

## Implementation Evidence

- `NodePortVisualResolver` translates authoritative `PortDomain`,
  `AttachmentType`, `PortPurpose`, and direction into neutral/primary colour
  and `PortVisualSemantic` values. `NodePortLayout` additionally consumes typed
  modulation-slot metadata for bundled destinations. Neither contains a
  `NodeKind` switch.
- `NodePortIconRenderer` parses and caches the six SVG sources once. Modulation
  destinations use the same renderer with either the Y/R/B or R/B flag.
- `NodePortLayout` reserves a gutter only when typed input metadata resolves to
  an actual icon. Output ports and plain colour-coded signal inputs do not
  reduce preview content.
- Existing `ModulationCableBundle` routing, graph compatibility, scene centres,
  and hit geometry are reused unchanged. Only endpoint/socket presentation is
  translated at the UI boundary.
- `cycle-v2-agent-port-icons.json` captures disconnected, connected/selected,
  and reduced-zoom states. The final review artifacts are
  `/private/tmp/cycle-v2-port-icons-disconnected.png`,
  `/private/tmp/cycle-v2-port-icons-connected.png`, and
  `/private/tmp/cycle-v2-port-icons-reduced.png`.

## Acceptance Criteria

- Every preview-node socket has the same base diameter and aligned hit target.
- The socket remains at the node boundary; its icon appears immediately inside
  the node and never overlaps the socket, cable, selection outline, or content.
- Nodes reserve consistent gutters only for actual input icons, without
  reducing previews for plain signal ports or right-side outputs.
- Only time, spectral magnitude, and spectral phase use semantic port colour;
  other ports and cables use the monochrome palette.
- Modulation Triple, Pitch, Unison, Voice Context, and Scratch attachments are
  distinguishable by icon without colour.
- Connection, hover, selection, and pending states remain readable without
  changing socket size.
- Hit testing covers the common invisible target and does not depend on the
  semantic icon or painted socket diameter.
- Focused UI fixtures cover left and right ports, connected and disconnected
  states, selected and unselected nodes, normal zoom, and reduced zoom.
- Screenshot review confirms that no port or icon is clipped and that node
  content begins after the reserved gutter.

## Non-Goals

This work does not redesign cable routing, connection compatibility, node
content, or expanded editors. It also does not use icons to replace explicit
property text: preview summaries remain responsible for presenting important
node values in readable language.
