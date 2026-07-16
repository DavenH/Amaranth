# Cycle 2 Semantic Test Matrix

## Purpose

This is the maintained audit of Cycle 2 tests and automation. It maps tests to
product risks instead of treating test or assertion count as evidence.

Classifications:

- **Retain**: guards a distinct semantic or architectural contract.
- **Consolidate**: useful contract, but should move into a stronger aggregate
  test when that boundary is available.
- **Support**: diagnostic/setup automation; not acceptance evidence.
- **Deleted**: no distinct semantic protection.

All test cases in a listed source inherit its classification and risk mapping
unless an exception is named explicitly.

## Risk Register

| ID | Area | Failure guarded |
| --- | --- | --- |
| DEF | Definitions | Registry, schema, factories, ports, defaults, and module capabilities disagree. |
| GED | Graph edits | Invalid or partial graph mutations, incorrect routing, or broken undo semantics. |
| PER | Persistence | Serialized graph/domain meaning, identity, or topology is lost or partially applied. |
| CMP | Compilation | Domain resolution, ordering, slots, latency, or immutable configuration is wrong or stale. |
| RUN | Runtime | Payload routing, retained state, per-voice isolation, or prepared execution is unsafe. |
| DSP | DSP | A node produces numerically incorrect blockwise or gridwise output. |
| INV | Invalidation | Edits fail to rebuild required state, rebuild unrelated state, or leave downstream output stale. |
| CUR | Curve editing | Curve identity, selection, add/move/reshape/delete, publication, or controls regress. |
| ENV | Envelope | Time traversal, lifecycle, morph, markers, parameters, or downstream gain regress. |
| TRI | Trimesh | Mesh rendering, topology, collision, controls, morphing, or panel binding regress. |
| HST | Hosting | Capability discovery, rebinding, teardown, or editor ownership becomes unsafe. |
| VIS | Visual output | OpenGL/JUCE composition, clipping, sizing, or cursor/hover feedback regresses. |

## Catch2 Source Audit

| Source | Classification | Risks | Authoritative contract and limitations |
| --- | --- | --- | --- |
| `TestNodeDefinition.cpp` | Retain | DEF | All definitions have coherent unique schemas and runtime metadata comes from that single source. |
| `TestGraphNodeFactory.cpp` | Retain | DEF, GED | Factory materializes canonical definitions and editor insertion assigns unique identity. Some per-kind examples intentionally protect menu-visible defaults and node sizing. |
| `TestGraphValidator.cpp` | Retain | GED, CMP | Graph grammar and contextual domain/channel resolution accept and reject concrete product cases with diagnostics. |
| `TestGraphEditor.cpp` | Retain | GED | Connection orientation, attachments, splice rules, removal, parameter normalization, and error outcomes mutate the aggregate correctly. |
| `TestGraphTransaction.cpp` | Retain | GED | Compound changes commit atomically and failed changes leave the graph untouched. |
| `TestGraphSerializer.cpp` | Retain | PER | XML round trips graph metadata, parameters, port sides, and attachments; unsupported input is rejected. Curve and Trimesh domain payloads require their own model tests. |
| `TestGraphCompiler.cpp` | Retain | CMP | Validated graphs compile to deterministic scheduling, domains, slots, lifetimes, latency, and immutable configurations; failures retain prior publication. |
| `TestGraphRuntime.cpp` | Retain | RUN | Runtime traces and attachment views preserve their compiled semantic roles. |
| `TestGraphInvalidation.cpp` | Retain | INV | Signal, attachment, preview, and graph-semantic dependency propagation is precise. |
| `TestNodeUpdateGraph.cpp` | Retain | INV, TRI | Trimesh local/panel/downstream invalidation decisions compose without broad refreshes. |
| `TestNodeDspConfiguration.cpp` | Retain | CMP, RUN | Publication keys/revisions, failure retention, and shared ownership remain valid across publisher replacement. |
| `TestNodeModuleRegistry.cpp` | Retain | DEF, RUN | Executable/preview capability, Cycle 1 adapter provenance, and preview authority remain explicit. The former role-label string test was deleted as presentation trivia with no consumer contract. |
| `TestNodeAudioProcessor.cpp` | Retain | DSP, RUN, ENV | Production processors prove numerical block/grid behavior, parameter effects, tails, channel policy, Envelope lifecycle, and mature-effect policies. Factory-only assertions are retained as one registry-to-executable integration check, not numerical evidence. |
| `TestGraphAudioExecutor.cpp` | Retain | RUN, DSP, INV, ENV | Compiled graphs route exact payloads, isolate voices, retain state, avoid realtime allocation, apply Envelope phase per column, and now prove published Waveshaper/Envelope edits alter exact node and downstream output. |
| `TestFftNodeDsp.cpp` | Retain | DSP | FFT grid columns and IFFT carry/round-trip semantics are numerical contracts. |
| `TestGraphPreviewExecutor.cpp` | Retain | RUN, INV, VIS | Preview scheduling and runtime taps consume actual compiled/audio payloads, including serialized mesh edits and spy grids. Image composition remains an OS-capture concern. |
| `TestNodePreviewProcessor.cpp` | Consolidate | DSP, VIS | Numerical summary/parameter behavior is useful; broad “factory creates” and “covers summaries” cases should fold into registered preview contract tests when the preview registry is next changed. |
| `TestGraphRenderSemanticResolver.cpp` | Retain | DEF, VIS | Render scale/domain decisions are derived from voice and downstream semantic context. |
| `TestNodeCanvasArchitecture.cpp` | Retain | GED, INV, HST, VIS | Viewport/hit geometry, document commands, presentation revision rejection, automation routing, view capabilities, and attachment scene geometry share authoritative state. |
| `TestNodeEditorHost.cpp` | Retain | HST | Host follows registered capability and stable node identity while reusing, replacing, and safely destroying editors without `NodeCanvas`. Real delegate/event delivery is native-test territory. |
| `TestEffect2DMeshState.cpp` | Retain | PER, CUR | Canonical ordered flat transfer representation round trips. The malformed-partial-parse test was deleted because typed snapshots reject malformed state atomically; blessing partial recovery contradicted that contract. |
| `TestEnvelopeMeshState.cpp` | Retain | PER, ENV | Envelope cubes, morph vertices, and lifecycle markers round trip; absent/malformed state is rejected. |
| `TestCurveNodeModels.cpp` | Retain | PER, CUR, ENV, GED, CMP | Stable flat/cube identity, atomic edits, typed publication, selection, undo coalescing, controls, immutable configurations, defaults, and repository presets are domain contracts. |
| `TestTrimeshInvalidation.cpp` | Retain | INV, TRI | Primary-axis, morph, mesh, control, profile, and panel-context changes request precise raster work. |
| `TestTrimeshNodeDsp.cpp` | Retain | TRI, DSP, VIS | Numerical mesh DSP, node model edits/selection, shared panel/rasterizer binding, and bounded side-panel rendering are guarded. Legacy indexed parameter cases are retained only until typed Trimesh topology replaces that persistence surface. |

## Deleted And Consolidation Decisions

| Test | Decision | Reason or destination |
| --- | --- | --- |
| `Effect 2D mesh state skips malformed triples` | Deleted | Enforced partial recovery from an obsolete flat string boundary and conflicted with atomic typed-snapshot rejection. Malformed state remains covered in `TestCurveNodeModels.cpp`. |
| `Node module role labels are stable` | Deleted | Asserted two unused display strings and guarded no runtime, serialization, or UI contract. |
| `Node preview processor factory creates preview modules` | Consolidate later | Retained temporarily as the registry-to-factory integration example; fold into a table-driven registered-preview contract when preview registration changes. |
| `Preview processors cover mesh, image, and output summaries` | Consolidate later | Retained numerical examples should be combined with the specific summary tests without losing parameter and upstream-input cases. |

## Cycle V2 Automation Audit

| Fixture or runner | Classification | Risks | Contract or limitation |
| --- | --- | --- | --- |
| `test_cycle_v2_native_edit_smoke.py` | Retain, acceptance | GED, PER, CUR, ENV, TRI, INV, HST, VIS | One persistent app receives real macOS events. It creates nodes from the palette, replaces a connection, inserts a Delay into a cable with explicit clearance from both neighbours, deletes, undoes/redoes, and saves/reloads exact topology without using Spy. It also asserts hover before mouse-down, add/move/reshape/delete state, both curve polarities, stable Envelope parameters, Envelope movement/publication, Trimesh collision/controls/morph, downstream audio, and safe teardown. |
| `cycle-v2-agent-graph-editing.json` | Retain | GED | Front-door graph command smoke. |
| `cycle-v2-agent-graph-roundtrip.json` | Retain | PER | Application-level save/reload graph round trip. |
| `cycle-v2-agent-audio-capture.json`, `cycle-v2-agent-with-spies.json`, `cycle-v2-agent-spy-preview.json` | Retain | DSP, INV, VIS | Runtime output and spy integration fixtures; screenshots are supporting visual evidence, not numerical DSP oracles. |
| `cycle-v2-agent-effect2d-interaction.json`, `cycle-v2-agent-trimesh-controls.json`, `cycle-v2-agent-mesh-controls.json` | Support | CUR, ENV, TRI | Fast semantic/in-app diagnostics. They bypass AppKit/JUCE delivery and cannot satisfy native interaction acceptance. |
| `cycle-v2-agent-effect2d-panels.json`, `cycle-v2-agent-mesh-controls-os-screenshot.json`, `cycle-v2-agent-waveshaper-os-screenshot.json` | Retain | VIS | Target discovery plus OS capture for OpenGL content, sizing, and composition. |
| `cycle-v2-agent-opengl-diagnostics.json`, `cycle-v2-agent-opengl-diagnostics-os-screenshot.json` | Retain | HST, VIS | Context lifetime/GL diagnostics and OS-level evidence. |
| `cycle-v2-agent-pointer.json`, `cycle-v2-agent-pointer-targets.json`, `cycle-v2-agent-pointer-target-replay.json` | Support | HST | Automation protocol diagnostics only; successful synthetic pointer delivery is not product interaction evidence. |
| `cycle-v2-agent-menu-palette.json` | Retain | DEF, GED | Registered palette capability reaches graph commands. |
| `cycle-v2-agent-readonly.json`, `cycle-v2-agent-screenshot.json` | Support | VIS | Harness health and capture diagnostics, not semantic gates. |

## Authoritative Contract Commands

Focused Catch2 filters:

```sh
build/standalone-debug/cycle-v2/CycleV2_tests "[graph]"
build/standalone-debug/cycle-v2/CycleV2_tests "[configuration]"
build/standalone-debug/cycle-v2/CycleV2_tests "[runtime]"
build/standalone-debug/cycle-v2/CycleV2_tests "[curve-model]"
build/standalone-debug/cycle-v2/CycleV2_tests "[invalidation]"
build/standalone-debug/cycle-v2/CycleV2_tests "[canvas]"
```

Native acceptance:

```sh
scripts/test_cycle_v2_native_edit_smoke.py
```

## Residual Risks

| Risk | Current limitation | Required boundary |
| --- | --- | --- |
| Trimesh durable topology | Live edits and sparse vertex parameters do not encode full topology. | Implement the typed topology snapshot recorded in `refactors.md`, then add exact save/reload identity coverage. |
| Cycle 1/Cycle 2 DSP parity breadth | IR policy and shared rasterizers have parity evidence, but not every streaming effect has a common cross-version harness. | Extract or expose shared domain cores as nodes are hardened; never create duplicate test formulas. |
| OpenGL pixel stability | OS capture proves presence/composition but no stable image-difference threshold exists across GPU/scale configurations. | Keep state assertions authoritative and use targeted human-reviewed OS captures for GL-only composition. |
| Native CI availability | AppKit/JUCE acceptance requires macOS Accessibility permission and foreground ownership. | Run locally and on a dedicated macOS UI runner; deterministic host/domain tests remain cross-platform gates. |
