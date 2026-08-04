# Cycle 2 Voice Context And Layer Routing

## Status

In progress. A single spectral Voice Context now types each source mesh from
its consuming magnitude or phase branch, and Stengah connects that context to
all three spectral meshes. Stengah also preserves its scratch-envelope
topology. Explicit Pan nodes expose stereo panning, range, and
magnitude mode on the canvas and execute them in flat audio, preview payloads,
and the oscillator-region renderer, including exact Stengah phase pan and range
values. Scratch execution and envelope profiles are implemented;
inherited morph defaults remain incomplete. Envelope purpose, polarity, and
logarithmic scaling are specified in
`cycle-v2-envelope-purpose-routing-and-scaling.md`. Stengah's authored
cube-component guide assignments are now also explicit graph attachments.

## Problem

Cycle 2 originally represented pitch and scratch envelopes as generic Envelope
nodes, repeated a Modulation Triple connection at every morphable node, and had
no persisted spectral-layer panning. Those omissions cannot be repaired by
guessing at extra graph edges: pitch changes voice traversal, scratch replaces
the traversal coordinate selected by a source layer, and spectral panning
changes stereo magnitude and phase construction.

Stengah makes the loss visible. Its Cycle 1 time mesh is intentionally empty,
its scratch envelope is active and selected as scratch channel 0 by its source
layers, and its two active phase layers are hard-panned to opposite channels.

## Authoritative Implementations

- `cycle/src/Audio/Voices/CycleBasedVoice.cpp` owns per-voice pitch, envelope,
  scratch-time, and unison traversal state.
- `cycle/src/Audio/SynthAudioSource.cpp` owns global scratch-envelope buffers.
- `cycle/src/App/CycleMeshLibraryConfig.h` defines which source layer groups may
  consume scratch envelopes and guide curves.
- `cycle/src/Curve/Rasterization/Policies/Graphic/GraphicPolicies.h` defines
  when a selected scratch channel replaces the traversal dimension.
- `lib/src/Curve/Rasterization/Rasterizer/TimeColumnRasterizer.cpp` and
  `cycle/src/UI/VisualDsp.cpp` apply layer pan through
  `Arithmetic::getRelativePan`; the latter also preserves spectral magnitude
  mode/range and phase-offset scaling.
- `cycle-v2/src/Graph/NodeDefinition.cpp` is authoritative for the graph grammar.
- `cycle-v2/src/Runtime/GraphAudioExecutor.*` and
  `cycle-v2/src/Runtime/GraphPreviewExecutor.*` remain authoritative for audio
  and preview traversal. New routing must adapt the Cycle 1 contracts into
  these executors without copying their DSP.

## Proposed Contracts

### Spectral source context

Voice Context selects the spectral family, not magnitude or phase for an
entire traversal path. Each context-connected mesh resolves its concrete
spectral domain from its fixed downstream consumer, including through Add and
Multiply nodes. Explicit legacy `spectralMagnitude` and `spectralPhase` values
remain readable, but new graphs persist `spectral` and display all three values
as Spectral in the editor.

### Voice context defaults

Voice Context gains a composite Modulation Triple input. Its three ordinary
control signals define the default yellow/red/blue traversal controls for the
voice path. A downstream node inherits those controls through voice context
unless that node has an explicit axis connection; an explicit connection
overrides only its attached axis.

The composite socket remains authoring metadata over three ordinary signals.
It must not introduce a hidden modulation matrix, a triplet runtime payload, or
serialized implicit edges at every consumer.

### Envelope profiles and pitch

Envelope uses the four-purpose Control/Volume/Pitch/Scratch contract in
`cycle-v2-envelope-purpose-routing-and-scaling.md`. That document owns render
scale, neutral value, output domain, legal connection kinds, logarithmic DSP,
and scratch traversal. This routing TDD consumes the resulting typed products;
it must not infer purpose from an Envelope title or instance ID.

### Scratch routing

A scratch envelope represents a named traversal channel. Source layers select
a scratch channel and the selected channel may replace their time coordinate
under the Cycle 1 group/axis rules. The graph representation must preserve both
the envelope identity and the layer selection; a single generic attachment
edge without channel and axis semantics is insufficient.

For Stengah, scratch channel 0 is represented by its scratch Envelope node and
attachment edges preserve the source-layer selections. The active magnitude
and phase meshes consume that attachment during block and traversal-grid
rendering. The omitted empty time mesh has no attachment target.

### Spectral layer panning

Layer pan is authored on an explicit Pan node downstream of each
spectral mesh. Audio execution uses the mature `Arithmetic::getPans` contract
to distribute each layer into left and right spectral contributions. The node's
compact canvas surface shows that routing and its range/mode; mesh previews
remain raw geometry views. Unison lane pan remains owned by oscillator
materialization after spectral reconstruction; it must not be folded into the
layer parameter or applied twice.

Magnitude panning must preserve the difference between additive and
multiplicative layers. Phase panning scales each channel's copy of the layer's
phase offset before phase layers are summed. Phase range remains part of that
operation: rasterized normalized phase is multiplied by the authoritative
phase-offset range and `2 * pi`, then by the channel pan coefficient, before
accumulation.

The signal path must become honestly stereo-aware before importing these
values. A scalar `pan` parameter that is ignored by mono execution or applied
after IFFT would not match Cycle 1. Stengah's two phase layers provide the
parity fixture: their imported pan values are 1.0 and 0.0.

### Stereo spectral execution slice

The first complete implementation slice is the authored spectral-layer path:

1. An explicit Pan node follows each spectral Trilinear Mesh and
   persists the source-layer processing required by its resolved domain: pan
   and range for phase, and pan, range, and additive/multiplicative mode for
   magnitude. The canvas shows those values; Trilinear Mesh owns geometry and
   traversal only.
2. Spectral source and arithmetic routes are prepared with two-channel storage
   independent of the current pan value. Changing pan must remain a parameter
   publication and must not require graph recompilation.
3. Pan execution produces independent left and right contributions
   from its mono mesh field using the Cycle 1 layer formulas. Add and Multiply operate on
   both channels and preserve the multiplicative neutral value where a layer's
   pan coefficient is below one.
4. IFFT reconstructs both channel spectra. The spectral frame renderer passes
   those distinct frames to the existing per-lane reconstruction and Unison
   materialization boundary instead of copying one mono frame.
5. Flat audio and preview execution consume the same parameter/configuration
   and stereo payload contracts. They may adapt the domain renderer but must
   not retain a second implementation of spectral layer formulas.
6. The Stengah graph persists phase-layer pans 1.0 and 0.0 plus their authored
   phase ranges. Golden tests prove the opposing phase contributions survive
   serialization, compilation, preview, IFFT, and oscillator materialization.

Implemented initially with parameters on Trilinear Mesh. That representation
made the graph look unchanged and hid a real signal-processing stage inside a
geometry node. The stable end state is an explicit Pan node rendered
as a cable-inline pan dial rather than a full-size processing panel. Selection
still exposes its range and magnitude-mode parameters for authoring.
`SpectralLayerCore` remains shared with Cycle 1 for the mature magnitude and
phase formulas; flat FFT/IFFT, probes, and traversal payloads preserve both channels;
and the oscillator-region slot graph reconstructs distinct left and right
frames before existing Unison materialization. Parameter changes invalidate DSP
state without changing graph topology.
The Channel palette names the operation Pan. Its inner dial uses conventional
vertical knob dragging, while its visible outer ring retains ordinary node
movement through a circular hit boundary and four-way cursor. Cable sockets
remain available at the left and right edges. The
Stengah automation fixture proves both gestures and verifies that a dial drag
does not change edge count. Cursor routing publishes the resolved affordance to
the active JUCE mouse source as well as the component under the pointer; the
focused cursor fixture covers the Pan dial, its move ring, and a Trimesh editor
control. Pan gestures snap within 5% of hard left, centre, and hard right.
An open expanded editor captures pointer input only inside its visible panel;
canvas nodes outside it, including Pan dials and move rings, retain their normal
gestures. The Pan hit region is the complete visible circular chrome rather
than the smaller rectangular node body, so cursor and drag ownership cannot
flicker at the ring boundary.

Compact and expanded Trimesh views consume one render-profile contract. Domain
and scale policy are both part of mesh-data and sprite invalidation: changing a
downstream Pan between additive/unipolar and multiplicative/bipolar semantics
must rebuild both views from the same authored mesh values. A combined Stengah
regression sequence opens the magnitude editor, adjusts and moves its downstream
Pan, checks stable hover affordances, and compares the two presentations.

## Boundaries And End State

- Do not infer envelope roles from node IDs, titles, graph position, or the
  first compatible destination.
- Do not connect scratch to every mesh merely to make the canvas look complete.
- Do not duplicate the Modulation Triple edges in serialized imported graphs
  once voice-context inheritance exists.
- Do not approximate spectral panning with a downstream mono balance control.
- Do not hide spectral range, magnitude mode, or pan inside Trilinear Mesh;
  these are signal operations and must remain visible on the canvas.
- Keep single-control routing operations proportional to their visual payload;
  Pan occupies a knob-sized interruption in its signal cable.
- Migrate generic scratch attachment edges to typed scratch-channel routing
  without losing their source-layer selections.
- Remove repeated imported morph edges when voice-context defaults and explicit
  override semantics are implemented.

## Completion Criteria

- Envelope profile selection changes its display scale, output domain, and
  legal connection grammar atomically.
- Pitch envelope output is bipolar and can connect only to Voice Context.
- Voice Context carries per-voice pitch state and inherited morph defaults.
- Explicit yellow/red/blue connections override inherited axes independently.
- Scratch channel identity and source-layer selection survive save/load and
  reproduce Cycle 1 traversal in audio and preview.
- Stereo spectral execution reproduces additive/multiplicative magnitude pan
  and phase-offset pan, including Stengah's opposing phase layers.
- Focused validator, compiler, audio/preview parity, serialization, and UI
  automation tests cover the complete routing sequences.
