# Cycle V2 Port And Icon Presentation

## Status

Implemented. Shared geometry, neutral presentation semantics, external
input-only icon badges, cable endpoints, hit targets, and focused
normal/reduced-zoom fixtures are in place. This document does not change graph
types, connection compatibility, or DSP.

## Problem

Cycle V2 currently gives ordinary signals, configuration attachments,
Modulation Triple bundles, and cable endpoints separate socket geometry and
colour rules. Ports that perform the same interaction consequently appear at
different sizes. Attachment type is also encoded primarily through colour,
which will become noisy and ambiguous as the attachment vocabulary grows.

Port icons drawn inside preview rectangles compete with the visualization and
can be mistaken for preview content. Semantic badges instead belong outside the
node, immediately before the boundary socket.

## Decision

Use colour only for the three primary processing signal domains: time,
spectral magnitude, and spectral phase. All other ports and their cables use a
shared monochrome palette. Attachment semantics are communicated by a small
descriptive icon, not by assigning another colour.

For an icon-bearing left-side input, the socket is the outermost visual element
and the icon forms a tab against the node boundary. The visual order is:

```text
cable ───○ [icon]│ node content
```

Output ports and plain colour-coded inputs remain boundary sockets. Semantic
input icons sit outside the node on the node-facing side of their sockets. The
badge slightly overlaps the node edge so it reads as an attached tab rather
than a floating label. Neither form changes node or preview content bounds.

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

Top and bottom input ports use the same socket and icon measurements, arranged
outward as appropriate. Side-port centres use 44 world units of vertical
spacing, a 29% increase from the original 34 units. Icon placement is
presentation geometry only; it does not change node size or preview layout.

The initial semantic icons are:

| Port semantic | Icon concept |
| --- | --- |
| Modulation Triple | neutral socket with a yellow/red/blue flag; red/blue for Envelope destinations |
| Pitch envelope | existing Envelope Pitch mark |
| Unison configuration | fanned voice lines |
| Voice context | established Voice Context node mark |
| Scratch attachment | existing Envelope Scratch mark |

Icons use the same visual weight and remain distinguishable at normal canvas
zoom. The established Pitch, Scratch, and Voice Context marks retain their
existing accents, and the modulation flag reuses the established morph-axis
colours; sockets and cables remain neutral. Shape remains the primary semantic
distinction. Exact artwork belongs to the shared icon set, not individual node
painters. A short port name remains available through hover text and
accessibility metadata; an icon is not the only machine-readable description.
Existing Pitch, Scratch, and Voice Context artwork is reused through its
authoritative renderer rather than copied into the port icon set.

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
44-unit side-port spacing, and the attached icon lane. Ordinary, Modulation
Triple, and Unison sockets, cable endpoints, presentation bounds, and scene
targets consume that geometry.

## Implementation Evidence

- `NodePortVisualResolver` translates authoritative `PortDomain`,
  `AttachmentType`, `PortPurpose`, and direction into neutral/primary colour
  and `PortVisualSemantic` values. It contains no `NodeKind` switch.
- `NodePortIconRenderer` parses and caches the modulation and Unison sources
  once, and delegates Pitch, Scratch, and Voice Context to their established
  renderers. Modulation destinations use either the Y/R/B or R/B flag.
- `NodePortSocketRenderer` is the single circular socket painter used by
  ordinary ports, compact Modulation nodes, bundle destinations, and bundle
  cable endpoints.
- Port badges are outside node bounds and meet the preview edge. Their sockets
  sit on the cable-facing side of the badge. Output ports, icon-bearing inputs,
  and plain colour-coded signal inputs never reduce preview content.
- Existing `ModulationCableBundle` routing and graph compatibility are reused
  unchanged. Icon-bearing endpoint centres and their matching hit targets move
  outward together; only this socket/icon presentation geometry is translated
  at the UI boundary.
- `cycle-v2-agent-port-icons.json` captures disconnected, connected/selected,
  and reduced-zoom states. The final review artifacts are
  `/private/tmp/cycle-v2-port-icons-disconnected.png`,
  `/private/tmp/cycle-v2-port-icons-connected.png`, and
  `/private/tmp/cycle-v2-port-icons-reduced.png`.

## Acceptance Criteria

- Every preview-node socket has the same base diameter and aligned hit target.
- For semantic inputs, the socket appears on the cable-facing side of an icon
  tab attached to the node edge; plain ports remain at the boundary.
- Side input rows have at least 25% more vertical separation than the original
  34-unit layout.
- Input icons and right-side outputs do not reduce node preview content.
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
- Screenshot review confirms that no port or icon is clipped and that preview
  content is unchanged by icon presence.

## Non-Goals

This work does not redesign cable routing, connection compatibility, node
content, or expanded editors. It also does not use icons to replace explicit
property text: preview summaries remain responsible for presenting important
node values in readable language.
