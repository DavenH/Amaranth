# Cycle 2 Signal Traversal Grid

## Purpose

Cycle 2 signal nodes need a shared signal representation that carries both the
ordinary blockwise signal and the gridwise traversal data used by node previews,
Spy probes, and graph inspection.

The current implementation treats traversal grids as preview-only data. That is
not sufficient. If a signal path transforms the audible signal, it must also
transform the traversal grid that represents that signal.

The goal is to make traversal data a first-class part of time, magnitude, and
phase signal processing.

## Signal Payload Contract

Concrete signal domains should always carry traversal grid data:

- Time signal
- Spectral magnitude
- Spectral phase

Each signal payload should include:

- blockwise samples for the current audio/control block
- domain metadata
- channel layout metadata
- traversal grid values
- traversal grid dimensions
- enough metadata to interpret grid arity and axis layout

The C++ ownership model should keep those concerns separated:

```cpp
struct SignalBlock {
    std::vector<float> samples;
};

struct SignalTraversalGrid {
    std::vector<float> values;
    size_t columns {};
    size_t rows {};
};

struct SignalPayload {
    PortDomain domain {};
    ChannelLayout channelLayout {};
    SignalBlock block;
    SignalTraversalGrid traversalGrid;
};
```

`SignalBlock` is deliberately only samples. It should not own traversal-grid,
domain, or layout metadata.

`SignalTraversalGrid` is deliberately only grid values and dimensions. It
should not duplicate signal domain or layout metadata.

`SignalPayload` is the unit that moves across graph ports. It owns signal
metadata plus the blockwise and gridwise representations.

`AudioProcessContext` should publish outputs only through a vector of
`SignalPayload` values. It should not also carry a separate single `output`
field, because that creates two competing sources of truth for node results.

Control, context, and attachment-only relationships are outside this contract.
They may have their own preview/rasterization data, but they are not required to
carry audio/spectral traversal grids.

## Node Responsibilities

Every node that creates a concrete signal must create a traversal grid.

Every node that processes a concrete signal must process its traversal grid with
the same operation it applies to the blockwise signal.

Every node that is disabled, bypassed, or semantically transparent must pass
the traversal grid through unchanged.

Nodes must not silently substitute unrelated preview data for the traversal
grid. A node preview may have local UI content, but Spy and cable probes should
observe the signal traversal payload, not the node editor's decorative or
configuration preview.

## Suggested Processing Abstraction

Use a shared base class or helper layer for signal processors that can apply the
same blockwise operation across traversal columns.

The base abstraction should allow a node to define its normal one-dimensional
calculation once, then reuse it for gridwise processing by:

1. Preparing the same node DSP state for the desired domain/layout.
2. Running the normal blockwise calculation for the current block.
3. Iterating traversal columns.
4. Advancing the yellow/time morph cursor between column calculations when the
   node's source data depends on a morph position.
5. Applying the same operation to each column.
6. Publishing both blockwise output and gridwise output as one coherent signal
   payload.

This may be a templated base class if compile-time specialization is useful,
but the key requirement is behavioral reuse. Avoid duplicating a second
hand-written grid algorithm for every node when the correct behavior is
"perform the same operation over each traversal column."

## Blockwise Processing As The Canonical Path

Gridwise processing should be produced by repeatedly using the node's blockwise
processing path. The blockwise path is the canonical implementation of each
node's signal behavior.

This is important because state, carry buffers, overlap behavior, phase
continuity, filter history, convolution tails, and other temporal details
already belong to the blockwise processor. Grid rendering should not
reimplement those details in a parallel algorithm.

The same blockwise processing implementation should serve:

- realtime audio-thread processing
- main-thread/UI traversal grid generation

The UI traversal path should feed the processor one traversal column at a time
and collect the resulting columns into a grid.

For stateful processors, grid generation should follow an explicit lifecycle:

1. Create or acquire a UI/thread-local processor state.
2. Clear/reset that state before beginning a complete grid traversal render.
3. Process traversal columns in order using the normal blockwise operation.
4. Preserve natural state/carry behavior between columns when that is part of
   the node's intended traversal semantics.
5. Flush/finalize after the grid render if the processor needs to publish tails,
   overlap carry, or delayed output.

This gives grid rendering the same behavior as repeated blockwise processing,
without duplicating DSP logic.

## Stateful Processor Isolation

Node processors may be stateful. Realtime audio and UI traversal rendering must
not share mutable DSP state.

At minimum, state should be separated by execution context:

- audio-thread realtime state
- main-thread/UI grid-render state
- test/offline state as needed

The UI path must not clear, advance, or flush the audio-thread state while
rendering a grid preview. Likewise, realtime audio processing must not depend on
state mutations performed for UI previews.

Shared immutable node parameters, meshes, curves, and coefficients are fine.
Mutable DSP history is not.

This implies that a node's processor interface should distinguish between:

- configuration/state that can be shared safely
- execution state that belongs to one processing context
- scratch buffers that can be reused only within a single context/thread

## Arity Model

Signal operations need explicit arity handling. Useful categories:

- Scalar: one value or parameter applied everywhere
- Vector: one one-dimensional signal/block
- Matrix/Grid: traversal grid columns and rows

Processors should choose an output arity based on their inputs and operation.

General rules:

- Scalar with vector -> vector
- Scalar with matrix -> matrix
- Vector with vector -> vector
- Vector with matrix -> matrix, applying the vector to each column
- Matrix with matrix -> matrix, applying the operation elementwise

Dimensions will match by a centralized x-axis and y-axis resolution configuration.
Where the y-axis is measured in samples in the time domain, the number of harmonics will be half this for the frequency domain.

Processing should not silently truncate in arbitrary ways.

## Add And Multiply

Add and Multiply are the first arithmetic nodes that need this arity behavior.

Add:

- scalar + scalar -> scalar
- scalar + vector -> vector
- scalar + matrix -> matrix
- vector + vector -> vector
- vector + matrix -> matrix, adding the vector to each column
- matrix + matrix -> matrix, elementwise addition

Multiply:

- scalar * scalar -> scalar
- scalar * vector -> vector
- scalar * matrix -> matrix
- vector * vector -> vector
- vector * matrix -> matrix, multiplying each grid column by the vector
- matrix * matrix -> matrix, elementwise multiplication

Envelope outputs are vector outputs. Therefore:

```text
grid signal * envelope vector -> grid signal
```

means the envelope value at each row/sample is multiplied into every traversal
column of the grid.

This is the expected behavior for using envelopes as amplitude/control factors
over time, magnitude, or phase-compatible signal grids, subject to existing
domain validity rules.

## Source Nodes

Source nodes must create grid data, not just a blockwise signal.

Trilinear Mesh:

- already has explicit gridwise rendering machinery
- should publish that grid as part of the signal payload
- should not rely on the preview executor as the only owner of grid data

Wave and image sources:

- should create traversal grids appropriate to their domain and source model, at the configured domain resolution (through resampling if needed)
- if they are effectively one-dimensional, they may create a repeated or
  degenerate grid, but it must still be explicit and consistent

FFT:

- should transform time-domain traversal grids into magnitude and phase grids
- existing `FftGridwiseDsp` is the likely starting point
- the graph runtime/preview path must actually use it

IFFT:

- should transform magnitude/phase traversal grids back into a time-domain grid
- cyclic/acyclic overlap behavior should apply consistently to blockwise and
  gridwise processing

## Effect Nodes

One-dimensional effects still need gridwise behavior.

Waveshaper:

- applies its transfer curve to blockwise time samples
- applies the same transfer curve to every value in the traversal grid
- disabled/bypassed waveshaper passes both blockwise and gridwise data through
- current cycle-v2 runtime applies persisted `enabled`, `pre`, and `post`
  parameters to block and traversal data
- editable `effect.vertices` persistence feeds a node-specific waveshaper DSP
  adapter that reuses the shared FXRasterizer transfer-table semantics for
  block and traversal-grid values
- traversal-grid and UI preview processing deliberately use the transfer table
  directly without oversampling. This matches the Cycle 1 graphics policy:
  independent visualization columns are not continuous audio and should not
  inherit oversampler delay-line artifacts.
- realtime audio oversampling/latency remains an audio-thread adapter concern,
  not part of the traversal-grid payload contract.

IR modeller / convolution:

- applies the impulse/convolution process to blockwise time samples
- applies the same process across traversal columns
- convolution state/carry resets before the full traversal render and then
  carries naturally across columns so the grid shows impulse tails over time
- disabled/bypassed IR passes both blockwise and gridwise data through

Delay/Reverb:

- transform blockwise samples and traversal columns through node-owned effect
  processors
- reset effect state before the full traversal render, then process columns in
  order so delay history, feedback, and reverb tails remain visible over time

## Current Implementation Status

Implemented:

- `SignalBlock`, `SignalTraversalGrid`, and `SignalPayload` separate sample,
  grid, and signal metadata ownership.
- `AudioProcessContext` publishes outputs only as `SignalPayload` values.
- Wave/image sources publish deterministic source-shaped traversal grids rather
  than repeated copies of the block vector.
- Trilinear mesh source publishes traversal columns from `TrimeshGridwiseDsp`.
- FFT transforms time traversal grids into magnitude/phase traversal grids.
- IFFT transforms magnitude/phase traversal grids back into time traversal grids.
- Add/Multiply handle scalar, vector, and grid operands through one operand
  lookup path.
- Passthrough/bypassed processors preserve traversal grids.
- Waveshaper applies the same persisted scalar transform to block samples and
  traversal-grid values, and preserves both when disabled.
- Guide, IR, and waveshaper expanded editor controls persist their scalar
  control values into graph-owned node parameters.
- Guide, IR, and waveshaper 2D panel vertices persist into graph-owned
  `effect.vertices` node parameters.
- Waveshaper runtime processing delegates to a node-specific DSP adapter that
  builds its transfer table through `FXRasterizer` and applies the same
  transfer to block samples and traversal-grid values.
- Unary signal processing has explicit block/grid lifecycle hooks so stateful
  processors can reset traversal state without leaking from the visible block.
- IR runtime processing delegates to a node-specific DSP adapter that builds an
  impulse through `FXRasterizer` and applies convolution with
  `ConvReverb::basicConvolve` to block samples and traversal-grid columns.
- Delay runtime processing applies a Cycle 1-derived spin-delay adapter to block
  samples and traversal-grid columns.
- Reverb runtime processing applies a Cycle 1-derived kernel-generation adapter
  and `ConvReverb::basicConvolve` to block samples and traversal-grid columns.
- Binary matrix-with-matrix traversal processing requires matching grid
  dimensions; mismatches explicitly produce no traversal grid instead of
  silently choosing one shape.
- Shared runtime traversal helpers live in runtime helper/processor modules
  rather than in `NodeAudioProcessor.cpp`.

Future hardening:

- Expand sample-rate and tempo metadata usage beyond delay into every realtime
  processor that needs host timing.
- Generalize Cycle 1 effect contracts so Cycle 2 adapters can reuse the exact
  runtime effect implementations without UI/singleton dependencies.
- Move vector-backed runtime payload storage into a preallocated graph work
  arena before realtime audio-thread hardening.

## Spy Relationship

Spy should observe the signal traversal payload on a subject cable.

Spy should not depend on whichever preview data the previous previewable node
happened to emit. The Spy preview should be a faithful rendering of the signal
payload at its tap point.

If a node changes the signal but does not update traversal data, a Spy after
that node is wrong by definition.

## Preview Relationship

Node previews and expanded editors may show local configuration UI, such as a
waveshaper transfer curve or IR curve. That is separate from the signal
traversal payload.

A node can have both:

- a configuration/editor preview showing its parameters
- a signal traversal preview showing the processed output payload

Spy always uses the signal traversal preview.

## Implementation Notes

The existing `AudioProcessBlock` is too small for this contract. It currently
stores samples, domain, and channel layout only. Introduce a richer signal
payload type rather than bolting grid metadata onto unrelated preview structs.

Potential direction:

- `SignalPayload` or `SignalBlock`
- blockwise samples
- optional traversal grid
- domain and channel layout
- arity metadata
- helpers for scalar/vector/matrix promotion
- helpers for applying unary and binary block operations across grid columns

Avoid keeping traversal data exclusively in `GraphPreviewExecutor` or
`NodePreviewResult`. Those should consume the signal payload, not be the source
of truth.

## Tests

Unit tests should cover:

- Source nodes produce traversal grids for concrete signal domains.
- Disabled/bypassed processors pass grids through unchanged.
- Waveshaper applies the same transfer curve to each grid value.
- IR/convolution applies its process across traversal columns.
- Add handles scalar/vector/matrix combinations.
- Multiply handles scalar/vector/matrix combinations.
- Envelope vector multiplied by a grid applies to every grid column.
- FFT transforms time grids into magnitude/phase grids.
- IFFT transforms magnitude/phase grids back to time grids.
- Spy after each processing node sees the transformed grid, not the upstream
  pre-transform grid.
- Preview rendering consumes the signal payload's grid metadata and values.

Automation/UI tests should cover:

- Spy after mesh source matches source grid.
- Spy after Add shows added grid data.
- Spy after Multiply with an envelope shows envelope-scaled grid data.
- Spy after Waveshaper shows waveshaped grid data.
- Spy after FFT/IFFT shows the correct domain grid.
