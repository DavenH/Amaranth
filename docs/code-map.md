# Semantic Code Map

Use this map to choose the first directory or interface to inspect. It is a
curated routing index, not a file manifest: this repository has thousands of
tracked files, and an exhaustive inline inventory would consume context while
hiding the important ownership boundaries.

Each mapped description states purpose and useful reuse in at most 120
characters. Run `python3 scripts/check_code_map.py` after editing this file.

## Maintenance Contract

- Map source boundaries, entry points, reusable cores, and canonical workflows.
- Do not add every implementation sibling, test, fixture, asset, preset, or generated file.
- Update the map in the same change that alters a mapped role or path.
- Replace stale entries and merge overlapping entries before adding new ones.
- Keep at most 100 mapped entries and 240 total lines; the checker enforces both budgets.
- Prefer a feature route below when several files collaborate on one behavior.

## Root And Build

- `CMakeLists.txt` — Root build graph; reuse its platform flags and subproject wiring.
- `CMakePresets.json` — Canonical configure, build, and test presets; reuse named presets in automation.
- `.clang-tidy` — Repository lint policy for handwritten C++; exclude generated JUCE sources.
- `README.md` — Product overview, dependency setup, builds, tests, and packaging entry point.
- `AGENTS.md` — Compact always-loaded agent router; keep detailed or conditional guidance elsewhere.

## Shared Library

- `lib/CMakeLists.txt` — Builds AmaranthLib and its tests; reuse its dependency and platform configuration.
- `lib/src/Array/Buffer.h` — Primary numeric buffer abstraction; reuse transforms and reductions before scalar loops.
- `lib/src/Array/VecOps.h` — Platform-vectorized primitives; reuse for bulk math and normalization.
- `lib/src/Algo/` — Shared algorithms, FFT helpers, resampling, and pitch analysis; reuse outside product UI.
- `lib/src/Algo/Pitch/` — Pitch detection and tracking; reused by Oscillo and available to synth features.
- `lib/src/Audio/` — Shared audio plumbing and DSP infrastructure; reuse before product-local utilities.
- `lib/src/Audio/CycleDsp/` — Cycle DSP kernels and state; reuse for Cycle 1/2 audio parity.
- `lib/src/Audio/Filters/` — Filters, convolution, oversampling, and delay primitives; reuse in node processors.
- `lib/src/Curve/Mesh/` — Mesh topology, morph dimensions, and orthogonal projection; shared representation.
- `lib/src/Curve/Rasterization/` — Mesh/curve rasterization and static geometry preparation; shared policies and sampling.
- `lib/src/Curve/Rasterization/Rasterizer/` — Authoritative rasterizer interfaces and implementations.
- `lib/src/Curve/Rasterization/EnvelopePlaybackEngine.h` — Shared envelope playback semantics; reuse across apps.
- `lib/src/Inter/` — Mature mesh/curve interaction and collision behavior; reuse or extract, never copy.
- `lib/src/UI/Panels/` — Shared 2D/3D panel and renderer infrastructure; reuse for mesh-based editors.
- `lib/src/UI/Widgets/` — Shared controls and display widgets; check here before adding app-local controls.
- `lib/src/App/Doc/` — Shared document and serialization support; reuse for persistent app state.
- `lib/tests/` — Shared library contracts for buffers, DSP, mesh, rasterization, interaction, and UI support.
- `lib/JuceLibraryCode/` — Generated JUCE amalgamations; compile as vendored code and never refactor or format.

## Cycle 1

- `cycle/README.md` — Technical model of Cycle's mesh synthesis, layers, envelopes, effects, and signal path.
- `cycle/CMakeLists.txt` — Cycle standalone/plugin targets and tests; reuse its plugin-client build wiring.
- `cycle/src/App/` — Startup, automation, files, presets, directories, and application lifecycle.
- `cycle/src/App/Initializer.h` — Main Cycle composition root; inspect for service ownership and app wiring.
- `cycle/src/Audio/` — Synth source and product audio orchestration; authoritative Cycle 1 signal behavior.
- `cycle/src/Audio/Effects/` — Mature effect implementations; reuse/extract for Cycle 2 parity rather than recoding.
- `cycle/src/Audio/Voices/` — Voice rendering and state; authoritative oscillator and per-voice behavior.
- `cycle/src/Curve/` — Cycle-specific rasterization contexts and thin product policies.
- `cycle/src/Inter/` — Product interactors for waveform, spectrum, and envelope editors.
- `cycle/src/UI/VisualDsp.h` — Visual DSP coordination used throughout Cycle 1 panels.
- `cycle/src/UI/VertexPanels/` — Mature waveform, spectrum, envelope, effect, and guide editor behavior.
- `cycle/src/Updating/` — Cycle 1 update propagation for morph, scratch, envelope, and spectrum changes.
- `cycle/src/UI/` — Cycle 1 presentation and widgets; authoritative mature editor behavior and look.
- `cycle/JuceLibraryCode/` — Generated JUCE and plugin-client sources; do not edit or use as app code examples.

## Cycle 2

- `cycle-v2/README.md` — Node-workflow overview, current controls, build, test, and automation commands.
- `cycle-v2/CMakeLists.txt` — CycleV2 app, graph migrator, and test target source lists.
- `cycle-v2/src/Main.cpp` — App composition root; App/ owns audio engine, graph files and library lifecycle.
- `cycle-v2/src/App/Automation/` — Socket transport, input, inspection, command routing and semantic authoring adapters.
- `cycle-v2/src/Reference/` — Reference library, pitch/cyclogram analysis, and numerical automation exports.
- `cycle-v2/src/Graph/NodeGraph.h` — Durable graph data model; read through dispatcher-owned edit views in UI code.
- `cycle-v2/src/Graph/GraphDocument.h` — Committed graph ownership, publication, and document observers.
- `cycle-v2/src/Graph/GraphCommandDispatcher.h` — Semantic values, topology, Guides, gestures and staged undoable publication.
- `cycle-v2/src/Graph/GraphDelta.h` — Ordered semantic deltas for atomic graph edits, rollback, undo, and redo.
- `cycle-v2/src/Graph/GraphSerializer.h` — Current graph persistence and migration-facing serialization.
- `cycle-v2/src/Graph/GraphCompiler.h` — Compiles graph topology into executable runtime structure.
- `cycle-v2/src/Graph/GraphValidator.h` — Coordinates graph validation; reuse focused validators beneath it.
- `cycle-v2/src/Graph/NodeDefinition.h` — Node types, ports, defaults, constraints, and synthesis capability metadata.
- `cycle-v2/src/Runtime/GraphRuntime.h` — Owns compiled runtime state and graph execution lifecycle.
- `cycle-v2/src/Runtime/GraphAudioExecutor.h` — Realtime audio and cursor capture; GraphPreviewTiming measures tails for shared PreviewTraversal coordinates.
- `cycle-v2/src/Runtime/GraphPreviewExecutor.h` — Runs scoped preview/analysis products without full audio work.
- `cycle-v2/src/Runtime/NodeModuleRegistry.h` — Registration boundary for node model, DSP, preview, and view modules.
- `cycle-v2/src/Runtime/NodeUpdateGraph.h` — Dependency-driven invalidation and preparation scheduling.
- `cycle-v2/src/Nodes/Trimesh/` — Trimesh model, DSP, panels, editors, and rendering; reuse shared rasterization cores.
- `cycle-v2/src/Graph/MeshDimensionality.h` — Shared axis eligibility and projection; GraphMeshGuideRemap preserves rail targets.
- `cycle-v2/src/UI/Editors/` — Shared morph reconciliation, diagram/input geometry, property controls and keyboard interaction.
- `cycle-v2/src/Nodes/Curve/` — Curve codecs, stable identities, editor adapters and shared panel/zoom/GL hosts.
- `cycle-v2/src/Nodes/Envelope/` — State/playback, surfaces and shared EnvelopeEditCore for panel and headless values/markers.
- `cycle-v2/src/Nodes/Guide/` — Guide attachments, snapshots, heatmaps, preparation, and editing.
- `cycle-v2/src/Nodes/ImpulseResponse/` — Canonical IR import/analysis; effect processors prepare playback-rate kernels.
- `cycle-v2/src/UI/NodeWorkspace.h` — Workspace composition, ephemeral controllers, and audio playback cursor delivery.
- `cycle-v2/src/UI/NodeCanvas.h` — Canvas orchestrator; delegate interaction, scene, presentation, and commands.
- `cycle-v2/src/UI/NodeCanvasInteraction.h` — Pointer/gesture routing; preserve semantic command and cost boundaries.
- `cycle-v2/src/UI/NodeCanvasPresentation.h` — Builds visible canvas presentation from graph/runtime facts.
- `cycle-v2/src/UI/NodeCanvasScene.h` — Retained node scene and geometry used by hit-testing and rendering.
- `cycle-v2/src/UI/NodeEditorHost.h` — Expanded editor lifecycle and factory-owned placement; PanelPlaybackOverlay draws bounded audio cursor frames.
- `cycle-v2/src/UI/NodeCanvasExplanations.cpp` — Preset/node/group explanations, shared editor and attached card orchestration.
- `cycle-v2/src/UI/NodePreviewRenderer.h` — Node/Spy previews, shared heatmap bounds and Trimesh harmonic lanes.
- `cycle-v2/src/UI/SignalProbeCanvas.h` — Spy views share persisted note-off timing; SignalProbeDetailView owns capture-free dragging.
- `cycle-v2/src/UI/InlinePresetBrowser.h` — Preset sidebar workflow and selection behavior.
- `cycle-v2/src/UI/PatternPlaybackSchedule.h` — Shared note ordering and controller preparation for playback and suite export.
- `cycle-v2/tests/` — Graph, runtime, node, UI, serialization, operation-count, and automation contracts.
- `cycle-v2/content/` — Factory graphs, presets, patterns, and samples; product data, not source examples.

## Other Products

- `oscillo/README.md` — Oscillo purpose, source landmarks, build, tests, and packaging.
- `oscillo/src/` — Standalone pitch-analysis UI and audio processor over shared pitch/DSP infrastructure.
- `oscillo/tests/` — Oscillo pitch trace, audio processor, and utility regressions.
- `installer/README.md` — Installer manifest schema, product boundaries, build, and packaging workflow.
- `installer/src/Main.cpp` — Product-neutral installer application and artifact-copy flow.

## Design, History, And Operations

- `docs/engineering-policy.md` — Detailed architecture, graph, complexity, hot-path, UI, and test rules.
- `docs/style-guide.md` — Authoritative C++ formatting and code-shaping conventions; do not inline into AGENTS.md.
- `docs/juce.md` — JUCE integration rules and capability checklist; consult before generic framework code.
- `docs/ADR/` — Accepted architectural decisions; use to understand stable direction and ownership.
- `docs/TDD/` — Feature designs, active status and evidence/acceptance rules; start with README.md.
- `docs/TDD/cycle-automation-coordinator.md` — Exact-instance ownership and cross-workspace GUI automation design.
- `docs/TDD/refactors.md` — Shared refactor backlog; check before adding another architectural wish.
- `docs/TDD/ui-bugs.md` — UI/runtime incident ledger for incidental failures and focused repro evidence.
- `docs/TDD/audio-bugs.md` — Realtime audio incident ledger and unresolved pipeline failures.
- `docs/cycle-agent-runbook.md` — Focused fixtures, native gestures, screenshots, state assertions, IPC, GL, and crash workflow.
- `docs/chats/` — Durable user preferences and workflow decisions; not a substitute for architecture docs.
- `scripts/cycle_automation.py` — Canonical local Cycle automation entry point for exact-PID lifecycle, fixtures, and capture.
- `scripts/cycle_automation_support/` — Product profiles, identity, leases, lifecycle, and persistent JSON-lines session/REPL.
- `scripts/fixtures/cycle1-smoke-suite.json` — Cycle 1 smoke fixture catalog and optional visual check.
- `scripts/fixtures/cycle-v2-smoke-suite.json` — Cycle 2 smoke fixture catalog and optional visual checks.
- `scripts/launch_cycle_app.swift` — Exact-PID NSWorkspace launch and native window activation for parallel workspaces.
- `scripts/capture_cycle_screenshot.swift`, `scripts/capture_cycle_window.swift` — PID-window screenshot, region crop, and video helpers.
- `scripts/run_cycle_agent.sh`, `scripts/run_cycle_v2_agent.sh` — Product aliases for fixture runs.
- `scripts/capture_cycle_ui.sh`, `scripts/capture_cycle_v2_ui.sh` — Product aliases for screenshots.
- `scripts/cycle_v2_architecture_audit.py` — Report CycleV2 file-size review triggers for touched production code.
- `scripts/check_code_map.py` — Validate semantic-map paths, description lengths, and fixed size budgets.
- `scripts/compare_cycle_audio.py` — Render declared-equivalent Cycle 1/2 presets and compare audio output.
- `scripts/fixtures/` — Stable automation scenarios; reuse a focused fixture before running broad smoke suites.

## Feature Routes

| Need | Start here | Reuse/ownership rule |
| --- | --- | --- |
| Numeric/DSP bulk work | `lib/src/Array/Buffer.h`, `VecOps.h` | Prefer vector operations; keep hot loops minimal. |
| Mesh geometry or traversal | `lib/src/Curve/Mesh/` | Shared mesh types own topology; do not clone logic in UI. |
| Rasterization or sampling | `lib/src/Curve/Rasterization/` | Compose policies/builders/samplers before adding algorithms. |
| Mature editor interaction | `lib/src/Inter/`, `cycle/src/Inter/` | Reuse/extract Cycle 1 behavior and preserve phase costs. |
| Cycle 2 semantic edit | `GraphCommandDispatcher.h`, `scripts/create_cycle_v2_candidates.py` | `applyNodeEdits` authors atomic candidates; dispatcher owns validation, undo, and publication. |
| Synthesis discovery | `Graph/SynthesisCapabilities.h` | Export metadata, physical conversions and canonical spectral authoring coordinates. |
| Cycle 2 audio execution | `cycle-v2/src/Runtime/GraphAudioExecutor.h`, `cycle-v2/src/Runtime/RealtimeGraphRenderer.h`, `cycle-v2/src/Runtime/VoiceNoteState.h` | Separate graph execution, timed voice events and bounded held-note/glide policy. |
| Performance suites | `scripts/render_cycle_v2_performance_suite.py` | Reuse captureAudio and coordinator; keep scheduling outside DSP. |
| Cycle 2 node feature | matching `cycle-v2/src/Nodes/<Domain>/` | Keep model, DSP, editor, panel, and rendering responsibilities local. |
| Canvas interaction/rendering | `cycle-v2/src/UI/NodeCanvas*.{h,cpp}` | Keep `NodeCanvas` orchestration-only; reuse scene and policy collaborators. |
| UI regression | `docs/cycle-agent-runbook.md`, `scripts/test_cycle_v2_native_edit_smoke.py` | Check pointer feedback, revision/publication and preparation costs; inspect filtered logs first. |
| Explained examples / sax studies | `scripts/create_cycle_v2_saxophone_articulation.py`, `scripts/create_cycle_v2_saxophone_anchors.py`, `scripts/create_cycle_v2_reed_saxophone.py` | Author expressive, measured-anchor and compact pulse/spectrum/body studies through canonical edits and capture. |
| Audition comparisons / reference listening | `scripts/report_cycle_auditions.py`, `scripts/create_saxophone_reference_audit.py`, `scripts/cycle_saxophone_reed_audit.py`, `scripts/cycle_study_artifacts.py` | Reuse saved metadata, declared levels, provenance and immutable study boundaries. |
| Labelled reference / pitch evidence | `scripts/cycle_reference_study.py`, `scripts/cycle_reference_pitch_evidence.py`, `scripts/cycle_saxophone_anchor_analysis.py` | Reuse cached analysis; retain confidence and effective pitch-path provenance. |
| Preset/graph persistence | `GraphSerializer.h`, product `content/` | Preserve migrations and canonical on-disk formats. |

## Fast On-Demand Discovery

```sh
rg --files cycle-v2/src/Graph
rg -n 'GraphCommandDispatcher|editingGraph' cycle-v2/src cycle-v2/tests
rg -n 'class .*Rasterizer|struct .*Rasterizer' lib/src/Curve cycle/src/Curve
rg -n 'Buffer|VecOps' lib/src cycle/src cycle-v2/src
git log --all --oneline -- <path>
git log -S'<symbol>' --all -- <areas>
rg --files cycle/src | rg '/tests/'
```
