# Cycle V2 Synthesis Capability Discovery

Status: Implemented. Implements PR 3 of the agent preset-authoring roadmap.

## Ownership and contract

`NodeDefinitionRegistry` remains authoritative for node types, ports, parameter
constraints/defaults/impacts, model codecs and scopes. Extend its definitions with
physical conversion callbacks and known control-adoption timing. Existing
`EffectParameterMapping`, `IrModel`, `CycleDelay` and runtime/editor conversion owners supply
all numerical conversions unchanged; discovery never copies their formulas.
`NodeModelCodec` supplies schema identity/version and a valid serialized default
example. An example is explicitly not a complete JSON Schema or a promise that
arbitrary model replacement is exposed by automation.

A read-only graph-layer exporter translates registered facts to JSON. Automation
adds only two routes: inspect capabilities and evaluate an existing physical
conversion. No editor instance, graph mutation, runtime preparation, audio device,
undo transaction or external service is needed. Timing describes current known
behaviour: Trimesh morph inputs are continuous; Envelope morph is sampled at
note-on. Other uncertain signal adoption remains unspecified. Stored authoring
configuration is distinct from a live performance modulation input.

## Files and size envelope

- `NodeDefinition.h/.cpp`: append parameter metadata and bind canonical mappings;
  approximately 150 new lines. Keep registry composition local.
- `NodeGraph.h`: trailing port timing metadata only; no serialization change.
- `Graph/SynthesisCapabilities.h/.cpp`: JSON translation and validated conversion;
  approximately 300 lines, no node-kind policy branches.
- Automation registry/switch plus narrow command adapter: approximately 60 lines.
- Focused tests, command documentation and code-map route.

The existing definition implementation is 824 lines. It remains one cohesive
registry composition owner; JSON translation stays in the new exporter. No
compatibility path is introduced, so there is no deletion debt.

## Acceptance and negative criteria

- Every registered type exports canonical ports, parameter constraints/defaults,
  processing capabilities and model schema/default examples.
- Exported values can drive valid GraphNodeStateEditor parameter edits. Exact physical
  conversions agree with canonical conversions, including contextual delay beats,
  sample-rate-dependent reverb length, and quantized IR size normalization.
- Invalid conversion requests reject unknown types/parameters, unavailable
  inverses, non-finite values and out-of-range stored values without mutation.
- Known Envelope/Trimesh timing remains distinguishable; unknown timing and
  unverified perceptual consequences are never advertised as continuous or known.
- Read-only export has no UI dependency, copied DSP/conversion math or parallel
  parameter/range/model schema table. It scales with registered metadata and
  default model examples; it never scales with an open document's unrelated data.
- Build exact app/test targets, run focused semantic tests and architecture/map
  checks, and exercise both command routes through sequential agent automation.

## Completion evidence and responsibility review

- Built `CycleV2` and `CycleV2_tests` with `standalone-debug`,
  `BUILD_TESTING=ON`, and `--parallel 10` in one directory. The tests preset's
  global `BUILD_TESTING` flag suppresses the application entry point, so app
  verification uses the standalone flags and target-specific test definitions.
  The final app freshness build followed the last production edit.
- Focused capability, definition, runtime morph and Envelope modulation tests
  passed: 19 cases, 2,869 assertions. Exported defaults drive actual graph edits
  and codec decoding. Conversion checks cover canonical Hz/dB/gain/contextual
  beats/kernel seconds, IR normalization, and invalid domains/inverses/contexts.
- The exact app passed `cycle-v2-agent-synthesis-capabilities.json` (8 commands).
  Report assertions verified 25 types, continuous Trimesh vs note-on-latched
  Envelope inputs, 2500 Hz inverse conversion, 22.5 dB Waveshaper gain,
  1.75 beats in a seven-beat measure, quantized IR samples, file output and
  identical before/after graph exports. No editor or pointer action was used.
- `git diff --check`, the architecture audit and code-map check passed. The map
  has 98 entries. No runtime assertion or crash was observed.

Production C++ adds 527 lines and removes 39. Largest touched sources are
`NodeDefinition.cpp` (824 -> 954), `CycleV2Automation.cpp` (1025 -> 1029), and
`NodeGraph.h` (464 -> 472). The registry still composes node definitions; physical
callbacks bind existing DSP owners beside their parameter declarations. The
new exporter (231 lines) owns representation and conversion-request validation,
with no NodeKind branches. The automation adapter (70 lines) owns JSON argument
and path handling. The existing automation switch adds only two routes.
The graph header remains a cohesive model/type boundary; the added enum and
trailing port metadata introduce no mutation, serialization, or lifecycle owner.
No duplicate algorithms, editor dependencies, compatibility layer or deletion
debt remain. Export cost is O(registered metadata + serialized default examples),
independent of document content; realtime and gesture complexity are unchanged.

Model codec identity/version/default examples are intentionally distinct from a
complete formal JSON Schema and from future bulk model-authoring commands.
Unmapped values, uncertain adoption timing and unverified perceptual effects
remain unspecified. All acceptance and negative criteria for this slice are met.
