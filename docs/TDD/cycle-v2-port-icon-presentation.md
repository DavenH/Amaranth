# Cycle V2 Port And Icon Presentation

## Status

Proposed. This document defines the shared preview-node port presentation
contract. It does not change graph types, connection compatibility, or DSP.

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

The mirrored order applies to a right-side output. The icon is inside the node
and adjacent to the socket; it is never drawn inside the socket or outside the
node boundary.

Every preview node reserves the same port-icon gutter on each side that owns
ports. Content layout begins after that gutter even when a particular port has
no icon. This keeps socket centres, icons, and main content aligned across node
families and prevents node-specific margin fixes.

## Shared Presentation Contract

One UI presentation primitive owns:

- socket diameter and stroke;
- hover, connected, selected, and pending-connection emphasis;
- the larger invisible hit target;
- icon bounds, spacing, and mirroring for input/output sides;
- colour selection from connection semantics;
- cable endpoint geometry.

All sockets are circular and have one base visual diameter. Direction is shown
by boundary position and cable direction, not by making inputs and outputs
different shapes. Interaction state may change the halo or stroke but must not
change the base diameter or move the socket centre.

The icon gutter is a layout reservation, not padding around the complete node.
Left and right gutters have equal width. Top and bottom ports use the same
socket and icon measurements, rotated or arranged inward as appropriate.
Nodes grow to fit their port stack and content; ports do not overflow or become
clipped to preserve an arbitrary node size.

The initial semantic icons are:

| Port semantic | Icon concept |
| --- | --- |
| Modulation Triple | three-part modulation mark |
| Pitch envelope | note or pitch-curve mark |
| Unison configuration | fanned voice lines |
| Voice context | compact voice/context mark |
| Scratch attachment | traversal/scratch mark |

Icons are monochrome, use the same visual weight, and remain distinguishable
at normal canvas zoom. Their exact artwork belongs to the shared icon set, not
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

Preview-node layout exposes reserved left, right, top, and bottom port gutters
to node content renderers. Voice Context is the first consumer, but the layout
API must be node-agnostic and adopted by all preview nodes rather than becoming
a Voice Context exception.

## Acceptance Criteria

- Every preview-node socket has the same base diameter and aligned hit target.
- The socket remains at the node boundary; its icon appears immediately inside
  the node and never overlaps the socket, cable, selection outline, or content.
- Nodes reserve consistent icon gutters on every side containing ports.
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
