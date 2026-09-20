# Refactor Notes

## Cycle V2 duplicated model and policy queue

Review date: 2026-09-19. These findings come from tracing repeated state and
decision sites after the Cycle 1/Cycle 2 source-allocation comparison. They are
ordered by expected architectural value. Each extraction must delete the
listed duplicate decisions; introducing another facade around them is not
completion.

### Addressed: Publish one indexed presentation-facts view

`GraphPresentationSnapshot` owns the compiled plan, runtime trace, preview
result, graph revision, and preview controls. `NodeCanvas` then retains
separate references to three snapshot members, `NodeCanvasQueryModel` retains
the same graph/compile/runtime/preview tuple, and `NodeCanvasPresentationFrame`
repackages the graph, compile result, preview result, and revisions again.
Consumers also reproduce lookup and derivation policy:

- `NodeCanvasQueryModel` and `NodeCanvasPresentation` each linearly find a
  `NodePreviewResult` by node ID;
- `SignalProbeRail` implements the corresponding probe lookup;
- `NodeCanvasQueryModel` resolves edge domains on demand through
  `GraphValidator`, while `NodeCanvasPresentation` and `SignalProbeRail`
  independently invoke `GraphRenderSemanticResolver`; and
- `GraphRenderSemanticResolver` performs a fresh full domain resolution and
  graph scans for individual presentation queries even though publication has
  already compiled the graph.

Introduce one immutable, indexed presentation-facts view built when a snapshot
is accepted. Compose it from the displayed `NodeGraph` and
`GraphPresentationSnapshot`; retain domain-specific render semantics below
`GraphRenderSemanticResolver`. Give node previews, probe previews, execution
steps, resolved edge domains, and render semantics one lookup owner. Then
delete the long-lived snapshot-member aliases in `NodeCanvas`, the duplicate
preview/probe loops, and per-query full domain resolution. Scale disconnected
graph content and assert unchanged lookup/domain work in addition to semantic
and pixel parity.

Status: addressed. The first slice adds `GraphPresentationFacts` beside each
accepted snapshot. It owns one structural edge index, domain resolution, and
audio-scope analysis, plus indexed node previews, probe previews, runtime
traces, and execution order. Preview-only publications rebuild the small
result indexes while sharing the structural facts. A topology compilation
rebuilds the structure. `NodeCanvasQueryModel` now uses these facts and the
snapshot directly; its separate compile/runtime/preview references, preview
loop, runtime loop, execution-order loop, attachment loop, per-edge full
domain resolution, and per-output full domain resolution are deleted.

The authoritative policy owners are now `GraphDomainResolver` for edge
domains, `GraphRenderSemanticResolver` for render meaning, and
`GraphPresentationFacts` for publication-time indexing. Query callers supply
only a node, port, or edge identity. With 0 and 128 disconnected nodes, 16
repeated domain, preview, and render-semantic queries record zero domain
transfers, validation-edge visits, and node linear scans after publication.

Baseline to after sizes for the first slice are `NodeCanvas.cpp` 2,532 to
2,530 lines, `NodeCanvas.h` 328 to 325, `NodeCanvasQueryModel.cpp` 295 to 263,
and `NodeCanvasQueryModel.h` 48 to 49. `GraphPresentationModel.cpp` grows from
542 to 565 lines to attach and reuse the facts at acceptance;
`GraphPresentationFacts` adds 184 focused lines. The remaining deletion
targets are the scene's signal-edge scans and the automation inspector's
parallel presentation construction.

The second slice passes the snapshot and facts through
`NodeCanvasPresentationFrame`. `NodeCanvasPresentation` now uses the shared
node-preview, render-semantic, edge-domain, and audio-scope facts, while
`SignalProbeRail` uses the shared probe-preview, render-semantic, and
edge-domain facts. Their duplicate preview loops, per-paint audio-scope
analysis, and on-demand full domain resolution are deleted. The frame also
holds one snapshot reference instead of repackaging its compile and preview
members. Focused canvas presentation tests pass 101 assertions in 10 cases;
probe tests excluding the stale Stengah preset fixture pass 308 assertions in
32 cases. `NodeCanvasPresentation.cpp` falls from 1,486 to 1,470 lines and
`SignalProbeRail.cpp` from 527 to 519; their policy responsibilities move to
the existing facts owner rather than another presentation helper. The preset
mismatch is recorded in `audio-bugs.md`.

The final slice makes scene construction, inline-pan placement, cable probe
resolution, node painting, hover resolution, and area selection consume the
same published `GraphEdgeIndex`. `NodeCanvasScene` no longer scans the complete
edge list once or twice per spectral-layer query, and its public placement and
cable helpers now require an index so a caller cannot silently reintroduce
per-query topology construction. The automation inspector supplies the facts
index when it builds its scene. Its remaining preview loops serialize the full
automation result and do not answer individual presentation queries.

The final deletion audit also removes `SignalProbeRail::renderSemanticForProbe`
and `NodeCanvasPresentation::hasGlobalProcessingIndicator`, the last UI entry
points that could independently run render-semantic or audio-scope analysis.
Production UI code now constructs those structural facts only at snapshot
publication. The focused scene, hit-router, automation, presentation, and probe
suites pass 521 assertions in 49 cases, plus the spectral-probe semantic case.
For this slice, `NodeCanvasPresentation.cpp` falls from 1,470 to 1,466 lines and
`SignalProbeRail.cpp` from 519 to 500. `NodeCanvasScene.cpp` grows from 489 to
524 lines to own indexed route lookup; `NodeCanvas.cpp` grows from 2,530 to
2,570 lines from explicit dependency forwarding without taking on a new
policy. The resulting dependency direction is snapshot publication to facts,
then facts to query, scene, and presentation consumers.

### Addressed: Centralize operation-port layout

`NodeCanvasPresentation.cpp` and `NodeCanvasAuthoring.cpp` contain separate
copies of `OperationPortLayout`, the input-side-to-layout mapping, the layout
cycle, and output-side cycle. Painting and authoring can therefore disagree
about the same node geometry. `NodePortLayout` already owns the equivalent
single-input policy.

Move operation layout classification, cycling, and application into
`NodePortLayout`. Make presentation and authoring consume that contract and
delete both private copies. Cover every layout with one test that checks the
authored port sides, painted port centres, and hit targets together.

Implemented in `NodePortLayout`. It now owns operation-layout support,
classification, cycling, and application, together with the output-side cycle.
`NodeCanvasAuthoring` requests and applies the next layout through that API;
`NodeCanvasPresentation` uses the same next-layout result for its action icon.
Both private enums, classifiers, cycles, and the authoring-side assignment
switch are deleted. The cross-layer layout test covers all four states and
compares the applied input/output sides with `portPresentation` centres and
the scene hit targets. Focused authoring, presentation, and hit-router layout
suites pass 101 assertions in four cases.

The original policy owners shrink as a result: `NodeCanvasAuthoring.cpp` from
1,009 to 947 lines, its header from 176 to 165, and
`NodeCanvasPresentation.cpp` from 1,466 to 1,416. `NodePortLayout.cpp` grows
from 57 to 130 lines as the single focused owner; its header grows from 18 to
32. Presentation and authoring now depend on layout policy, while layout policy
depends only on the graph's `Node` and port-side types.

### Addressed: Centralize Voice Context assignment facts

`GraphTopologyValidator::validateVoiceContextAssignments` and
`GraphCompiler::buildImplicitVoiceContextEdges` separately decide whether a
node accepts a `DomainContext`, scan explicit context assignments, and reason
about the available Voice Context nodes. The duplicated `acceptsContext`
predicate is textually identical, while the surrounding scans construct two
partial models of the same assignment policy.

Create a read-only Voice Context assignment analysis over `GraphEdgeView` and
the graph's node index. It should report context providers, accepting nodes,
explicit assignments, missing assignments, and the single-context implicit
source. The compiler should translate valid facts into implicit edges; the
topology validator should translate invalid facts into issues. Delete both
local predicates and assignment scans. Preserve compiler/validator parity for
zero, one, and multiple contexts with explicit and implicit assignments.

Implemented in `GraphVoiceContextAssignments`. Compilation and topology
validation now consume the same provider, consumer, and explicit-assignment
facts. `GraphValidationContext` retains the same analysis, and proposed context
edges update its assignment map without rescanning graph nodes or unrelated
edges.

### Addressed: Share the typed node-model envelope codec

`CurveNodeDomainCodec`, `TrimeshNodeModelCodec`, and
`UnisonNodeModelCodec` each encode and validate the same `schema`, `version`,
and positive `revision` envelope before delegating to domain payload logic.
Guide Curve deserialization repeats the flat-curve branch of
`CurveNodeDomainCodec` in `readGuideCurveModelJSON`.

Extract a narrow node-model envelope reader/writer that owns only common
metadata validation and diagnostics. Keep mesh, curve, Envelope, and Unison
payload validation in their domain codecs. Route Guide Curve loading through
the flat-curve codec with a Guide-specific default factory, then delete the
special duplicate reader. Malformed-schema/version/revision tests should be
table driven across every registered codec.

Implemented in `NodeModelEnvelopeCodec`, which is the sole writer and reader
for typed model schema, version, positive revision, and named payload fields.
Curve and Envelope, Trimesh, and Unison codecs retain their payload validation
and now consume the validated envelope. `GuideCurveModelCodec` supplies its own
default model and delegates decoding to the flat-curve codec; the duplicated
Guide reader is deleted. `GraphSerializer` depends on that codec rather than a
special deserialization function.

A table-driven contract passes valid defaults and malformed schema, version,
and revision envelopes through the flat curve, Envelope, Guide Curve, Trimesh,
and Unison codecs: 55 assertions in one case. Canonical graph serialization
and Guide noise-seed round trips add 13 passing assertions. The shared codec is
80 focused lines. `TrimeshMeshState.cpp` falls from 125 to 117 lines and
`UnisonNode.cpp` from 185 to 177. `CurveNodeModels.cpp` remains 814 lines because
it replaces the Guide reader with the composed Guide codec and one reusable
flat-curve decode entry point. Domain payload policy stays below the shared
metadata layer.

### Addressed: Compose expanded-editor chrome

Delay, Reverb, Equalizer, Unison, and Modulation expanded editors contain the
same background, border, title font, header layout, and close-button placement
block. Four also repeat enabled-button placement. Delay, Reverb, and Equalizer
repeat local normalized parameter mirroring after `NodePropertySliderRow`
already owns the edit gesture and command lifecycle.

Add a small composed editor-chrome component or paint/layout primitive that
accepts title and enabled/close capabilities. Keep previews, property groups,
and domain controls in each editor. Extend `NodePropertySliderRow` only with
the minimum local-preview callback needed to remove the remaining normalized
parameter mirror loops. Delete the repeated chrome blocks and retain the
existing editor automation states and screenshots.

Implemented in `ExpandedEditorChrome`, a composed collaborator that owns the
shared background, border, title, header layout, close button, and optional
enabled button. Delay, Reverb, Equalizer, Unison, and Modulation editors now
supply only their title and close/enabled actions. Their previews, property
groups, and domain controls remain local. `NodePropertySliderRow` now mirrors
preview values into the bound local node with the registered parameter
normalizer, deleting the three editor-local normalization and mutation loops.

Focused chrome, Delay/Reverb property, Equalizer property and gesture, Unison,
and property-regression suites pass 135 assertions in eight cases. Production
sizes fall from 323 to 277 lines for Delay, 301 to 255 for Reverb, 482 to 439
for Equalizer, 568 to 544 for Unison, and 298 to 288 for Modulation. The shared
chrome is 103 lines; the binding grows from 86 to 114 lines across header and
source. The five editor owners lose 169 lines overall, with chrome policy and
preview-mirroring mechanics each having one owner.

## Addressed: Cache Trimesh preview pitch context at graph publication

`NodePreviewResources::trimeshWidget` resolves pitch context on each widget
access, including compact canvas painting. `PreviewPitchResolver` traverses
authored edges and asks the compiler for implicit Voice Context edges, so a
repaint may repeatedly scan unrelated graph content. Cache each node's pitch
source and key-scale axis when the accepted graph configuration changes, then
pass the selected MIDI note separately. Preserve the compiler's implicit
context rule and invalidate on transient modulation-source edits.

Implemented with `PreviewPitchContextIndex`. `PreviewPitchResolver` remains the
authoritative owner of explicit traversal and the compiler's implicit Voice
Context rule. The index owns the published node-to-pitch binding and the
Modulation Triple dependency map. `NodePreviewResources` owns its lifetime and
now gives `TrimeshWidget` a cached binding plus the independently selected MIDI
note. `NodeCanvas` only announces accepted or transient graph changes at its
existing publication boundaries; it does not perform pitch resolution.

The former `trimeshWidget(Node)` traversal was deleted. Topology publication
rebuilds the bindings, while parameter-only Modulation Triple changes refresh
only the recorded dependents. A counter-based test adds 64 unrelated nodes,
performs 100 widget-equivalent lookups, and observes one graph resolution from
publication and none from lookup. Two transient source changes coalesce into
one parameter refresh without another graph resolution. The key-scale and
canvas-preview suites pass 58 assertions in five cases and 36 assertions in
seven cases; the focused cache case passes ten assertions. The resolver grows
from 187 to 232 lines across header and source, the composed index is 107
lines, preview resources grow from 261 to 278 lines, and the 2,570-line canvas
adds six orchestration lines without a new responsibility or policy branch.

## In progress: Cycle V2 spectral frame renderer ownership

`cycle-v2/src/Runtime/SpectralOscillatorFrameRenderer.cpp` is about 820 lines
after the 2026-09-18 Envelope guide seed change. Its lifecycle methods are the
right narrow place to forward the voice seed to the prepared Envelope bank, but
the file also owns region validation, source rendering, transforms, graph
combining, and output. Extract cohesive source-operation and frame-combining
ownership in a later behavior-preserving slice; keep the current shared
`PreparedCycleEnvelopeBank` and graph plan contract intact.

The first extraction moves transform allocation, frame-size lookup, forward
FFT capture, inactive-bin clearing, inverse FFT, and reconstruction capture to
`SpectralFrameTransformStage`. The stage depends only on FFT buffers and the
spectral capture/core primitives. Graph roles, slot routing, source rendering,
cycle envelopes, and performance scopes remain in the renderer. This deletes
the renderer's transform vector, allocation loop, lookup method, and inline
FFT/IFFT bodies. The renderer falls from 799 to 767 lines and its header from
120 to 119; the new cohesive stage is 137 lines across header and source.

The focused stage reconstruction/capture test passes 27 assertions. The
existing fixed Trimesh-through-FFT case passes 16 assertions and now proves
one forward and one inverse telemetry operation per prepared frame. The split
block spectral-frame case passes 19 assertions. The existing final-active-
harmonic regression still fails identically against the pre-extraction commit,
so it is not evidence against this move. Remaining deletion targets are the
inline time/spectral source bodies and the SpectralLayer/Add/Multiply combining
bodies; those need source-operation and shared binary-combining owners before
this item can be marked addressed.

## Migrated factory guide-curve attack boundaries

The Cycle 1 factory-preset port in `scripts/port_cycle_v1_preset.py` copies
guide vertex phase coordinates into `flatCurve` x coordinates unchanged. Both
Cycle 1's `GuideCurvePadding` and Cycle V2's `GuideCurvePreparation` sample
guide tables from x = 0.05, while many imported curves place their first sharp
attack at x = 0.0625 (or later). The first table samples therefore precede the
authored attack. The source geometry was already present in the initial Cycle V2
factory import (`4ed3d21b`, graph format 4); graph format 7 did not introduce
it. The 2026-09-16 master preset edits and the parallel spectral-control preset
edits move more than 130 guide vertices left across 26 presets, always in the same
direction, but by varying amounts. Those hand edits establish the boundary
intent, not a single safe numeric offset for every vertex.

If this becomes a preset-migrator pass, target the *factory-imported guide
geometry* by provenance or an exact source signature, and only move the attack
vertices needed to make the intended onset active at x = 0.05. Preserve manual
edits, interior timing, curves, and right-boundary geometry. Inspect remaining
drum presets such as Kicker, Brush Drum, and Stomper against their Cycle 1
source and audio before changing them. A graph-format version or a blanket
0.0125 subtraction would also alter later-authored curves and hand-corrected
presets.

## Cycle V2 presentation gesture and refresh-policy ownership

Status: active in
[`cycle-v2-causal-update-graph.md`](cycle-v2-causal-update-graph.md), reopened
2026-09-15.

The causal planner, pure refresh policy, shared gesture session, scheduler,
request builder, and preview renderer are in production. `GraphPresentationModel`
is smaller, but some editor families and canvas paths still choose refresh
behavior, the broad editor refresh host API remains, and the mod-wheel-specific
model entry point remains. Finish the caller migration and deletion targets in
the active TDD. Do not add another adapter or copy domain rendering behavior
into the shared session. Other architecture concerns from the 2026-09-18
review are tracked in [`cycle-v2-architecture-quality.md`](cycle-v2-architecture-quality.md).

## Share immutable guide products across prepared providers

`TrimeshGuidePreparation::prepare` currently creates one guide provider per
Trimesh configuration and prepares every graph guide. The measured oscillator
optimization adds exact immutable downsampling products (180,812 bytes per
8,192-sample guide plus offsets) to each provider snapshot. Voices sharing that
configuration reuse the products, but separate configurations duplicate them.

Separate immutable guide tables/sampling products from provider-local phase
scratch, and share them by prepared guide identity across configurations.
Preserve graph guide slot mapping, replacement lifetime, and per-render noise
semantics; do not share mutable scratch or add realtime lookup/locking. Consider
preparing only assigned guides once slot mapping has an explicit contract.
This is an off-thread memory/preparation improvement, not a reason to replace
the authoritative `GuideCurveTableDsp` sampling behavior.

## Cycle V2 first-class layer stack ownership

The Cycle 1 preset migration preserves Envelope and Trilinear Mesh enablement
on each source node. Until Cycle V2 has first-class time and spectral layer
stacks, a Trimesh configuration derives additive or multiplicative treatment
from its downstream operation topology while DSP configurations are built.
This keeps one durable owner and avoids audio-thread graph access, but the
downstream inspection is a temporary boundary translation.

Introduce domain-owned layer objects/stacks before adding broader layer
controls. Move `enabled`, range, and operation topology with that owner, then
delete the downstream operation inspection in `NodeDspConfiguration.cpp`.
Pan remains an independent cable operation and reuses Cycle 1's
`Arithmetic::getPans` behavior without another editable bypass flag.

Pan still carries compatibility-era internal names: serialized kind
`spectralLayer`, `NodeKind::SpectralLayer`, `AudioModuleRole::SpectralLayer`,
and `SpectralLayerNodeAudioProcessor`. When the graph format next supports a
kind alias, rename these together behind a read-only `spectralLayer` alias and
keep `PanConfiguration` as the domain-neutral runtime contract. Do not add a
parallel time-Pan node or duplicate its gain law while that naming migration
is pending. Spectral range now belongs to Trimesh and operation mode comes from
Add/Multiply topology, so centered Pan is uniformly removable in every domain.

## Panel line-strip coordinate ownership

`CommonGL::drawLineStrip` scales its caller-owned `BufferXY` in place when its
`scale` argument is true. Reusing that buffer for a second pass can therefore
scale coordinates twice and move an outline or foreground stroke off-panel.
Replace the mutating contract with either renderer-owned transformed scratch or
an explicit preparation step returning panel coordinates, then make repeated
stroke draws non-mutating by default.

## Addressed: Cycle V2 Trimesh model presentation extraction

`cycle-v2/src/Nodes/Trimesh/Model/TrimeshNodeModel.cpp` owns durable/live mesh
state, but its `renderGrid` path also constructs Trimesh DSP processors and
applies `TrimeshRenderProfile`. The source-layout migration makes that existing
Model-to-Dsp/Rendering dependency visible; it does not copy or approximate the
mature rasterization behavior to hide it.

Extract render-grid preparation behind a Trimesh presentation service that
accepts the model's prepared mesh/state and delegates to the existing
blockwise/gridwise DSP and render profile. Keep mesh identity, publication,
selection, and revision behavior in `TrimeshNodeModel`. Do not move rendering
policy into the model or introduce a second curve-evaluation implementation.

Implemented in `TrimeshGridRenderService`. The panel data source now owns the
presentation call, and `TrimeshNodeModel` no longer imports or constructs DSP
or rendering policy. The service delegates to the existing blockwise,
gridwise, and render-profile implementations.

## Cycle V2 Envelope curve panel decomposition

`cycle-v2/src/Nodes/Effect2D/EnvelopeCurvePanel.cpp` is a cohesive
Envelope-domain implementation, but it has grown beyond 1,100 lines while
combining interaction, marker/seam editing, drawing, background-grid policy,
automation inspection, and vertical view framing.

Extract narrowly owned Envelope presentation helpers without moving behavior
into the generic flat-curve panel or controller. A useful first boundary is a
view-presentation object that owns logarithmic grid classification and
vertical framing over the existing `Panel2D`/`ZoomPanel` implementation. Keep
mesh interaction, rasterizer ownership, and marker topology in the Envelope
panel and avoid a node-kind switchboard.

## Cycle V2 guide attachment target semantics

Status: cube-component identity implemented; generic ownership cleanup open
after the 2026-09-17 Envelope guide parity work.

The mature Cycle 1 mesh contract attaches a guide channel to a `VertCube`
component through `guideCurveChans[field]`. Cycle 2 currently authors and draws
`guide.vertex.<index>.<field>` graph targets. A vertex target cannot identify the
same interpolation region and should not become a compatibility fiction.

The graph now uses cube-component targets for Trimesh and Envelope, and both
reuse one Guide preparation core. The remaining `TrimeshGuideAttachmentMenu`
and `TrimeshGuideAttachmentTarget` filenames and command-service method names
also serve Envelope. Move these shared UI boundary types into the Guide module
and give them neutral names, without copying attachment behavior or adding a
node-kind switchboard. Keep the node-family-specific selection lookup in each
editor.

## Cycle V2 concrete editor registry decomposition

`cycle-v2/src/UI/ConcreteNodeEditors.cpp` remains 844 lines after the
Modulation editor was extracted into its own cohesive translation unit. The
remaining file mixes effect parameter editors, curve editor adapters, and the
Trilinear Mesh editor adapter.

Suggested direction:

- extract the effect parameter editor and its factory;
- move curve and Trilinear Mesh editor adapters beside their domain editor
  implementations; and
- leave `ConcreteNodeEditors.cpp` as a small registry assembly point, or replace
  it with domain factory registration.

This is not required for modulation bundle behavior; the current work reduced
the file by approximately 160 lines and did not add another concrete editor to
it.

## Cycle V2 Realtime Payload Storage And Host Audio

Status: open after the 2026-07-23 runtime-boundary audit.

The authoritative runtime is `GraphAudioExecutor`: it owns compiled routing,
retained per-node/per-voice processors, and prepared vector-backed payload
slots. Representative steady-state execution performs no `operator new`, but
`AudioProcessWorkArena` is capacity metadata rather than the aligned arena
described by `cycle-v2-node-module-runtime.md`.

The next storage slice should preserve compiled routing, processor identity,
DSP configuration, and node behavior unchanged. It should translate owning
realtime payload vectors into aligned arena ownership with non-owning
`Buffer<float>` process views, then delete allocation-capable vector mutation
from the realtime API. Diagnostic and preview results may remain value types
outside that boundary.

Live host audio is a later integration slice. The current standalone app is a
node workspace, and automation audio capture renders offline. Connect an
immutable prepared plan to a JUCE audio/MIDI callback only after the arena/view
boundary is explicit; do not embed device lifecycle or MIDI voice allocation in
`GraphAudioExecutor`.

## Cycle V2 JSON Graph and Typed Node Models

Status: Production refactor complete; native verification blocked (2026-07-22).

Active TDD:
[`cycle-v2-json-graph-and-typed-node-models.md`](cycle-v2-json-graph-and-typed-node-models.md).

Completed:

- Replaced XML `.cyclegraph` persistence with deterministic canonical JSON.
- Removed escaped `mesh.topology` and `curve.modelSnapshot` parameters.
- Separated scalar parameters, aggregate model publication, and editor state.
- Converted bundled presets without a graph-format compatibility layer.
- Added conflict-checked model replacement, undo/redo, semantic persistence
  tests, and native save/reload coverage.
- Replaced `var` payload holders with immutable concrete Trimesh, Envelope,
  and flat-curve snapshots.
- Removed runtime and presentation JSON reconstruction from synchronization,
  DSP preparation, preview, and audio paths.
- Added decode instrumentation proving already-loaded graphs remain outside
  JSON during presentation and runtime consumption.
- Renamed graph snapshot and automation outputs to `.cyclegraph`.
- Persisted authored port-side overrides without duplicating definition-owned
  port declarations, restoring the reviewed bundled layouts.

Remaining verification:

- Complete one reliable native macOS save/reload run across Trimesh, Envelope,
  and flat curves. The full suite and standalone build pass, but the native
  fixture currently misses pointer gestures nondeterministically and can fail
  before its persistence assertions. Keep the TDD in progress until this gate
  runs reliably or the native fixture is repaired and passes.

This is the prerequisite persistence/model boundary for the causal update
graph and must be completed before that boundary is considered closed.

## Cycle 2 OpenGL Cable Tessellation

The prototype GL cable renderer exposed platform-dependent artifacts with wide
`GL_LINE_STRIP` strokes and hand-built triangle strips: square cutouts on
curves, disappearing vertical sections, and poor selected-line readability.
Cycle 2 currently keeps cables on the JUCE path renderer while the background
remains OpenGL-backed.

Suggested direction:

- Re-enable GL cables only after adding a real stroked-path tessellator that
  emits joins, caps, and dash runs as explicit geometry.
- Treat selection as a separate narrow highlight stroke rather than by making
  the halo heavily opaque.
- Keep node shells and node contents in the same render layer unless the whole
  node widget moves to GL, because split shell/content rendering breaks
  overlap z-order.

## Cycle V2 Trimesh Panel Test Fixture Ownership

Status: open after the 2026-09-11 audio-parity verification run.

`Trimesh Panel3D reads node-backed columns through lib data retriever` crashes
in isolation at `SingletonRepo.h:54`. The test constructs a bare
`SingletonRepo` and then constructs `TrimeshPanel3D`, whose inherited panel
initialization expects registered singleton dependencies. Repair the fixture to
provide the real minimal panel environment, or move the data-retriever contract
below panel construction. Do not weaken `SingletonRepo` lookup or add nullable
production behavior for this test. This failure is unrelated to the Voice
Context/Envelope parity slice; its focused runtime Envelope tests pass.

## Envelope Morph State Ownership

Status: closed 2026-09-17.

The Envelope editor adapter now reads Red/Blue from node parameters, and
`EnvelopeNodeModel` no longer caches them. Morph-only `CurveNodeModelState`
revisions share immutable Envelope geometry and carry the authored scalar
values. The existing serialized schema and one-command undo behavior remain.
