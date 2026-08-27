# Cycle V2 IR Audio Resources

## Status

In Progress (technical design approved for implementation, 2026-08-27).

## Objective

Make the Impulse Response editor's Load, Model, and Unload actions truthful,
portable, undoable document operations. Imported audio must affect the prepared
IR configuration without giving the editor ownership of graph state, DSP state,
serialization, or undo.

## Authoritative Behavior And Required Extraction

Cycle 1 is authoritative for the user-visible operations:

- `PitchedSample::load` decodes WAV, AIFF, MP3 when enabled, and Ogg audio,
  selects the first channel for the IR, and normalizes the imported audio;
- `IrModeller::trimWave` removes the inaudible tail using its established
  moving-average threshold and clamps the retained length to 64-16,384 samples;
- direct Load selects the next power-of-two IR size and convolves with the
  imported samples;
- Model applies the established 0.7 input gain and `AutoModeller` fit at the IR
  padding and reduction settings, then uses the generated editable curve; and
- Unload removes the imported audio while retaining the editable curve.

The existing implementation is not an adapter boundary. `IrModellerUI` owns
the modeller call and mutates a mesh under UI/audio locks, while `IrModeller`
owns the live sample and pending DSP actions. Extract two UI-independent pure
operations instead of copying them:

1. move the trim-length calculation into `CycleDsp::IrModel` and make Cycle 1
   call it;
2. expose the existing `AutoModeller` buffer-to-intercepts fit so both the
   legacy interactor adapter and Cycle V2 modelling use the same algorithm.

`PitchedSample` remains the mature decode adapter. Cycle V2 copies its bounded,
normalized first channel into the durable resource representation; it does not
retain the legacy object or its mutable playback state.

## Durable Model

`NodeGraph` owns immutable `AudioSampleResource` values:

- stable resource ID;
- display name only, never an authoritative external path;
- source sample rate for inspection;
- one normalized mono channel, bounded to 16,384 samples.

The graph also owns a generic node-to-audio-resource binding with an opaque
domain mode. IR uses `direct` and `modelled`; generic graph and serializer code
do not interpret those strings.

Resources and bindings serialize inside the `.cyclegraph` document. Sample
values use a portable numeric representation for this bounded first slice.
Loading a project never depends on the original file still existing. The
existing JSON snapshot history therefore restores the asset, binding, mode,
parameters, and curve together for undo and redo.

An imported resource is replaced, not mutated. The semantic command removes an
unreferenced prior resource after rebinding. Unload removes the binding and its
now-unreferenced resource. This prevents editor lifetime, file chooser lifetime,
or realtime state from becoming document state.

## Semantic Command Boundary

Add one generic atomic graph edit that receives a complete prepared resource,
binding mode, optional replacement node model, and any domain parameters that
must change with the resource. It validates the complete edit before mutation,
then returns one consolidated `GraphEditResult` with the target node, DSP,
preview, presentation, model, and resource changes.

`GraphCommandDispatcher` owns snapshot undo and publication as usual. A narrow
node-editor resource command translates the prepared value into this dispatcher
operation. It contains no file decoding, trimming, modelling, IR parameter
mapping, or node-kind switch.

The IR domain owns preparation:

- direct import creates a resource, selects the next supported Size stop, and
  binds it in `direct` mode without replacing the curve;
- model import creates the same resource, builds a new `FlatCurveModel`, selects
  the corresponding Size stop, and binds it in `modelled` mode in the same
  command; and
- unload removes only the binding/resource, so the current editable curve is
  immediately authoritative again.

The DSP configuration factory resolves a binding from the graph only for the
node being configured. `IrSignalProcessor` consumes imported samples in direct
mode and otherwise follows the existing curve rasterization path. Both paths
reuse the existing high-pass preparation and post-gain mapping. Resource IDs
and modes participate in the configuration key; no file access or allocation
occurs on the audio thread.

## UI Contract

The property rail adds a separate Resource section below processing controls:

- **Load Audio** is the primary import action;
- **Model Audio** is a secondary conversion action;
- **Unload** is a lower-emphasis contextual action, enabled only while a
  resource is bound;
- one concise status line names the embedded resource and says whether Direct
  Audio or Modelled Curve is active; and
- while preparation is running, import actions are disabled and the status is
  explicit. Failures keep the prior document state and produce a readable
  status.

The graph view remains the largest region. In direct mode the status explicitly
says that the retained curve is inactive until Unload or Model; Modelled mode
shows the active editable curve. Actions use real focusable buttons, preserve
host Escape behavior, and expose stable automation IDs.

File selection is presentation-only. Decoding and modelling run outside the
audio thread; only the completed immutable result is applied on the message
thread. Closing the editor may cancel presentation of the result, but must not
leave a partial graph mutation.

## Negative Boundaries

- Do not persist an external path as the resource authority.
- Do not let the editor mutate `NodeGraph`, record undo, or publish runtime
  configuration directly.
- Do not copy trimming, modelling, rasterization, prefilter, or convolution
  algorithms into UI or command code.
- Do not retain `PitchedSample`, `FileChooser`, mutable buffers, or UI locks in
  graph state.
- Do not add IR parameter IDs or mode interpretation to generic resource,
  serializer, dispatcher, or node-editor command code.
- Do not approximate Model with a simplified curve or bless fake samples in an
  interaction test when the production decoder and modeller can be exercised.

## Expected Change And Deletion Targets

The durable resource/binding and serializer slice should remain roughly
250-450 production lines. Domain preparation, command translation, and UI are
expected to add roughly 300-500 lines. Unexpected node-kind switching, a
generic adapter above 150 lines, or more than about 900 net production lines is
evidence to stop and review.

Delete or supersede:

- Cycle 1's local trim calculation after extraction;
- the IR automation claim that resource actions are unavailable;
- the property-control TDD's resource-action blocker once all criteria below
  pass; and
- any temporary duplicated resource or model state in the editor.

## Verification

Semantic tests must prove:

- decode, normalization, authoritative trimming, supported-size selection, and
  invalid-file failure;
- graph serialization round-trip with no dependency on the source file;
- duplicate/malformed resource and dangling-binding rejection;
- direct import changes the prepared IR impulse and configuration key;
- model import uses the extracted modeller, replaces the curve, and keeps the
  resource in modelled mode;
- each import is one undo entry restoring resource, binding, Size, curve, and
  downstream refresh; redo restores them;
- unload is one undoable command and retains the curve;
- replacing an import removes its unreferenced old asset; and
- no resource decoding, modelling, or allocation occurs in realtime process
  methods.

UI tests and production automation must prove visible action hierarchy,
focusable targets, busy/error/resource states, contextual Unload, real Load and
Model command paths, first-Escape close, and a production-size screenshot with
no clipped status or stranded rail space.

## Completion Criteria

- Load, Model, and Unload are visible, truthful, and operate through one
  semantic command boundary.
- Imported audio is embedded, portable, bounded, immutable, serialized, and
  restored by undo/redo.
- Direct audio reaches the existing IR configuration and Model reaches the
  existing editable curve through extracted authoritative behavior.
- Resource replacement, unload, missing/corrupt data, and editor destruction
  have deterministic safe behavior.
- Generic infrastructure contains no IR IDs, modes, mappings, or modelling
  behavior.
- All deletion targets, focused tests, Standalone Debug build, style checks,
  automation fixture, filtered-log review, and production screenshot pass.

