# Cycle 2 Spy Node

## Purpose

The Spy node is a passive graph inspection node. It lets a user attach a
monitor to an existing signal cable without changing the signal, graph
semantics, timing, channel layout, or downstream behavior.

The core question it answers is: what does the signal at this point in the graph
look like?

Spy is not a generator, transform, effect, mixer, or analyzer that substitutes
synthetic data. It is a non-invasive probe with a faithful visual preview of
the signal flowing through its subject cable.

## Graph Behavior

The Spy node attaches to a subject signal edge instead of inserting itself as an
ordinary processing node between the source and destination.

A Spy node must not modify:

- sample values
- signal domain
- channel layout
- timing
- buffer size
- graph validity
- downstream routing

The subject cable remains the real signal path:

```text
A ---> B
```

With Spy attached, the graph should read visually as a tap/probe:

```text
A --o--> B
    |
   Spy
```

or equivalently as a T/up-tack from the subject cable to the Spy node. The cable
should not be split into `A -> Spy -> B` in the user-facing graph model.

Spy reads the resolved signal data at the attachment point. If the subject cable
is time-domain, Spy previews time-domain data. If it is spectral magnitude, Spy
previews spectral magnitude data. If it is spectral phase, Spy previews spectral
phase data.

## Grid Traversal Data Contract

Concrete audio/spectral signal domains should always carry grid traversal data:

- Time signal
- Spectral magnitude
- Spectral phase

Nodes that create these signals must create corresponding traversal grids.
Nodes that process these signals must process the traversal grids as part of
their signal operation. This includes one-dimensional processors such as
waveshapers: if the node transforms the signal, it should transform the grid
representation consistently.

Nodes that are disabled, bypassed, or otherwise not affecting the signal should
pass traversal grid data through unchanged.

Control nodes and attachment-only nodes are exempt from this grid traversal
contract. Their outputs may use their own preview/rasterization data, but they
are not valid Spy subjects unless they produce one of the concrete signal
domains above.

## Preview Behavior

Spy preview must visualize the real upstream signal data available at that graph
position.

It must not:

- draw fake placeholder waveforms
- generate synthetic demo content
- use unrelated iconography as signal data
- render a generic monitor pattern once connected

Spy should display the subject cable's traversal grid using the same visual
language as the signal being previewed. For example, if Spy is attached after a
Trilinear Mesh, the Spy preview should look like the mesh surface preview. Small
styling differences may be acceptable to indicate "monitor" rather than
"source", but the underlying structure must correspond to the input data.

For mesh/grid previews:

- Preserve subject grid values.
- Preserve subject grid dimensions.
- Preserve signal domain metadata.
- Use the same heatmap/surface renderer as the mesh preview.
- Avoid custom one-off renderers unless they render the same data faithfully.

If there is no connected input or no preview data available, Spy should render
either nothing or a subtle empty state. It should not invent content.

## Visual Semantics

Spy should visually communicate that it is an inspection node, not a processing
node.

The node preview should:

- show the incoming data
- use compact node-preview sizing
- match the visual density of the traversal grid it is previewing
- be readable at canvas zoom levels similar to mesh/effect nodes
- avoid obscuring or distorting the signal shape
- use the domain's existing preview color/profile where possible
- update dynamically when the upstream signal or traversal grid changes

Expected domain-specific rendering:

- Time signal: waveform/mesh heatmap profile
- Spectral magnitude: magnitude heatmap/profile
- Spectral phase: phase heatmap/profile
- Mesh/grid data: same grid surface preview as source

## Insertion Workflow

Spy should be easy to attach to a compatible cable.

When dropping Spy onto a compatible signal cable:

```text
A -> B
```

the graph should become:

```text
A --o--> B
    |
   Spy
```

The resulting graph should compile exactly as before, unless the original graph
was already invalid.

The tap point should stay on the subject cable and the Spy node should be placed
nearby with a short perpendicular branch. The branch may form a T shape or an
up-tack shape depending on available space. The placement should avoid
unnecessary layout shifts and should preserve the original left-to-right signal
flow.

## Compatibility

Spy should be compatible only with concrete audio/spectral signal cables:

- Time signal
- Spectral magnitude
- Spectral phase

Spy should not attach to control cables, attachment cables, context cables, or
other non-signal relationships. The drop affordance should not appear for those
cables.

## Persistence

Spy nodes should serialize and deserialize like other node types.

Saved graph data must preserve:

- node kind
- position
- subject edge or probe attachment
- tap position along the subject cable
- future Spy-specific display settings

Because Spy is transparent, it should not need DSP parameters initially.

## Tests

Unit tests should cover:

- Factory creates Spy with correct title, ports, and default metadata.
- Serializer round-trips Spy nodes.
- Compiler treats Spy as a non-invasive edge probe, not as a processing step in
  the signal path.
- Preview processor requires upstream data and does not generate fake fallback
  content.
- Graph preview executor passes subject grid data, grid dimensions, and domain
  metadata to Spy.
- Canvas preview uses the same surface/heatmap rendering path for grid data.
- Time, magnitude, and phase processors either create, transform, or pass
  traversal grid data consistently.

UI/automation tests should cover:

- Spy appears in the palette/sidebar.
- Spy can be added to the canvas.
- Spy can be attached to a compatible signal cable.
- Spy does not attach to control, attachment, or context cables.
- Graph remains valid after insertion.
- The subject cable remains visually and semantically `A -> B`, with Spy shown
  as a branch/tap.
- Screenshot/regression fixture shows Spy preview matching upstream mesh-like
  data, not placeholder iconography.
- Spy preview updates when the upstream signal changes.

## Non-Goals For First Version

The first Spy node does not need:

- editing controls
- signal recording
- oscilloscope time scrolling
- FFT analysis of arbitrary time input
- selectable probe channels
- multiple inputs
- probe history
- export/copy data
- expanded popup editor

Those can be added later. The foundation must be transparent pass-through plus
faithful visualization of real upstream data.
