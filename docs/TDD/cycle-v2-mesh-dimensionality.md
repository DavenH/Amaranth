# Unified Mesh Dimensionality

Status: Implemented and verified — native fixture timing caveat recorded in ui-bugs.

## Contract

Trimesh uses Static, Yellow, Yellow/Red, and Yellow/Red/Blue morphing.
Envelope retains Static, Yellow, Red/Blue, and Yellow/Red/Blue modes. The shared
axis policy takes the two-axis plane as domain data. New nodes start Static. Reduction projects dismissed axes
at authored positions and extrudes orthogonal rails, retaining variation on
surviving axes. Expansion is initially neutral; undo restores removed content.
Envelope morph is latched at note-on, independently of playback time.

Modulation Triple configures Trimesh and Envelope default profiles. Defaults
are voiceTime/keyScale/modWheel and unmapped/keyScale/modWheel respectively.
Unmapped falls back to authored position; explicit cables take precedence.

## Ownership and reuse

Shared mesh dimensionality owns active-axis and logical-corner policy;
shared rasterization owns projection and reduced evaluation. Existing
TrilinearMeshSlicer, guide policies, and curve builders remain authoritative.
Envelope marker/padding/playback and oscillator continuity remain separate.
GraphCommandDispatcher owns atomic dimensional conversion and undo. The
compiler owns default routing and excludes inactive modulation dependencies.
Editors translate gestures to commands and compose shared morph presentation.

Mode conversion inspects the affected mesh at the mode boundary; corner deduplication
uses an ordered map. Movement scales with changed
logical points and incident cells: no graph copies, serialization, unrelated
resource preparation, or hidden-axis collision constraints. Audio allocation
is forbidden. Static geometry is prepared outside audio and never sliced at
note-on or during playback; guide updates reuse prepared geometry.

## Compatibility

Graph format 9 migrates formats 7 and 8 after earlier migration stages. Existing
Trimeshes retain 3D; Envelopes retain Red/Blue and project Yellow=0 onto full
orthogonal Yellow poles. Legacy mapping profiles preserve existing sources.
Cube persistence remains; genuine reduced topology is future shared work.

## Completion and deletion targets

- [x] Shared dimensionality, projection, semantic command, and undo.
- [x] Versioned graph and Cycle 1 converter migration.
- [x] Separate mapping profiles, unmapped fallback, eligible dependencies.
- [x] Shared absolute morph inputs and full Envelope Yellow note-on support.
- [x] Static preparation and reduced evaluation; no realtime allocations.
- [x] Dimension-aware controls, ports, logical edits, and collision behavior.
- [x] Remove forced Envelope Yellow=0, unconditional Yellow pairing, and
      duplicate input-resolution decisions from Cycle 2 production paths.
- [x] Semantic/complexity tests, app build, app-side gestures and screenshots,
      architecture/style/code-map checks and responsibility review.
- [x] Native morph/keyboard acceptance and composited OpenGL screenshots.
      Exact-PID activation avoids System Events aliasing parallel CycleV2 instances.
      Screen Recording is now available; no privacy settings were changed by the agent.

## Verification

- `CycleV2` and `CycleV2_tests` build with `--parallel 10`.
- 16 focused cases / 1,901 assertions pass, including dynamic Envelope realtime
  allocation/lock checks; 24 related runtime cases / 309 assertions also pass.
- All dimensional transitions for both families round-trip and undo/redo.
  Cancellation restores projected geometry and remapped component guides.
- Guided migration matches mature playback at nine Red/Blue positions through
  sustain and release (1e-5 tolerance); marker membership remains durable.
- Reduced evaluation matches mature interpolation at boundaries in time,
  magnitude, and phase domains. Static oscillator ordinary/chained playback
  reports zero slices; Static Envelope note-on reports zero materializations.
- Yellow note-on offset, retrigger, held-note latching, profile precedence,
  independent fallback, configuration refresh, equivalent-buffer sharing,
  inactive dependency omission and cable restoration pass.
- Two logical vertex updates compose and undo with zero graph/mesh copies or
  serialization. Incident-owner visit count stays constant after adding 100
  unrelated cells. Hidden morph-coordinate edits are rejected.
- 67 Cycle 1 converter tests pass. Five old synthetic fixtures now contain valid
  envelope geometry so they exercise projection, rather than raw malformed JSON.
- 55 app commands pass: selector pointer click, reduced rail multi-update/commit/
  undo, all modes, and normal/compact captures. Artifacts in `.context/` include
  `dimensions-final-tests.log`, `dimensions-regression-focused.log`,
  `dimensions-converter.log`, `dimensions-ui-run.json`, and screenshots
  `envelope-dimensions-{0,1,2,3,compact}.png`, `modulation-profiles*.png`.
- Code-map and whitespace checks pass. Architecture audit size triggers reviewed
  below. Initial native permission limitations were resolved during follow-up verification.
- Broader selections still expose the empty-mesh UI crash recorded in ui-bugs,
  a scratch ControlSignal regression and strict pitch-guide lane parity failure
  recorded in audio-bugs. These are not covered by a claim of full-suite success.

## Responsibility review

Baseline -> after lines for the large production files:

| File | Lines | Ownership / stable collaborators |
|---|---:|---|
| GraphCompiler.cpp | 1535 -> 1568 | Plan orchestration; shared MeshDimensionality filters eligibility; mappings remain configuration data. |
| NodeCanvasPresentation.cpp | 1511 -> 1516 | Canvas target/layout presentation; bundle visibility delegates to ModulationCableBundle. |
| GraphSerializer.cpp | 1322 -> 1328 | Persistence orchestration; geometry migration lives in MeshDimensionalityMigration. |
| EnvelopeCurvePanel.cpp | 1295 -> 1305 | Mature envelope interaction adapter; shared MorphDimensions owns logical corners. |
| GraphAudioExecutor.cpp | 1256 -> 1256 | Execution lifecycle; only profile-aware source selection changes. |
| NodeCanvasAutomationInspector.cpp | 1139 -> 1161 | Existing target serialization; adds selector discovery, no edit policy. |
| EnvelopeEditorComponent.cpp | 923 -> 1011 | Domain tools and layout; shared morph layout and renderer compose controls; surface cache is separate. |
| TrimeshWidget.cpp | 888 -> 961 | Existing model/view adapter; shared MorphControlLayout supplies stable geometry; CurvePanelHost owns compact snapshots. |
| TrimeshSidePanelRenderer.cpp | 912 -> 737 | Painting authority shared by both families; MorphControlLayout owns geometry; the 96-line plane painter is separate. |
| CurveNodeModels.cpp | 862 -> 870 | Typed model identities/codecs; shared projection canonicalizes new Static envelopes. |
| NodeEditorCommandService.cpp | 849 -> 851 | Gesture transaction owner; forwards dimension policy into existing semantic deltas. |
| NodeDefinition.cpp | 824 -> 848 | Declarative node schemas and defaults only. |
| GraphCommandDispatcher.cpp | 809 -> 819 | Transaction orchestration; dimensional edit implementation is GraphMeshCommands.cpp. |

The existing large compiler/serializer/executor remain cohesive orchestration
boundaries. No projection, interpolation, smoothing, or marker algorithm was
copied into them. Shared library policy depends only on mesh/rasterization;
graph adapters add typed ownership and undo; editors consume that policy.
Future compiler growth should extract default-input planning with context
assignment as its collaborator, rather than adding node-family branches.
Envelope panel interaction remains the mature authority; future reduced
persistent topology must replace cube storage for **both** families together,
while retaining logical edit/undo APIs and the shared rasterization authority.

Deletion review: Cycle 2's forced Yellow=0 note-on interface, Red/Blue-only
configuration fields, unconditional Yellow pole pairing, and duplicate Envelope
plane painter are removed. Cycle 1's two-value materializer forwarding remains
as an explicit compatibility API. Component guide Time slots retain their
established meaning; no Vertex::Time rename was introduced.


## Follow-up: predictable reduction and editor continuity

Use the group label **Curve morphing**, with coloured axis ticks on each pill.
Trimesh reduction follows YRB -> YR -> Y -> Static. Preserve the current primary
axis while it remains active; fall back to Yellow only when that axis is dismissed.
Do not move surviving morph rails, gain, or vertex parameter rows as modes change.
In 2D show a plane with the cube's colour blending, selected geometry, linked
corner highlights, projected cursor lines, and axis rails. Keep the cube in 3D.
Use the established curve panel snapshot host for compact Static Trimesh previews,
removing the separate glow/trace painter. Host adapters translate lifecycle only.

Hovering an option that dismisses axes warns that the current position will be
baked and dismissed variation discarded, with ordinary Undo available. No modal
confirmation. Format 9 promotes format-8 Red/Blue 2D Trimeshes to 3D so their
existing variation is preserved; new 2D edits use Yellow/Red. Envelope migration
and playback time stay unchanged.

Follow-up acceptance: surviving controls have identical bounds in every mode;
Yellow primary and key-scale orientation stay stable through the descending sweep;
projection retains Yellow/Red variation; preview uses shared panel snapshots;
mode-specific diagram, ticks, warning hover, pointer/keyboard and undo are verified
at normal and compact widths.

Follow-up proof:

- 63 focused editor, dimensionality, mapping and runtime cases / 2,736 assertions
  pass; 67 converter cases pass. CycleV2 and tests build with `--parallel 10`.
- Both the original 55-command fixture and the 44-command refinement fixture
  pass. Surviving control bounds match across every mode; Yellow stays primary.
- Native morph drag and keyboard shortcuts pass for Trimesh and Envelope. Native
  checks caught and fixed rejected Envelope control publications caused by the
  omitted dimension setting. Keyboard tests now author their required dimensions
  explicitly and check durable values, retained dimensionality and Undo.
- Shared CurvePanelHost replaces Trimesh's duplicate 2D GL lifecycle and compact
  glow painter. Trimesh retains its gesture callbacks through a typed deletion
  hook; 3D lifecycle stays in TrimeshPanelHosts. No domain algorithm was copied.
- Shared selector options own ticks and dismissal text; JUCE BubbleMessageComponent
  owns callout presentation, with the established keyboard-help pointer polling.
  Shared axis policy owns both YR Trimesh and RB Envelope planes.
- Code-map, whitespace, architecture audit and style review pass. Large-file
  responsibilities remain as above; NodePreviewRenderer only dispatches the new
  Static preview to the existing shared snapshot host.
- Evidence: `.context/morph-refinement-{regression,converter,native-smoke}.log`,
  `morph-refinement-ui-run.json`, `dimensions-ui-regression-run.json`, and native
  screenshots `morph-refinement-native-{3,2,2-compact,compact-preview,warning}.png`.
  Warning text is visibly rendered by the JUCE callout in the native capture.
- Native point dragging passes through the shared curve host. The extended
  with-spies capture scenario has intermittent fast-drag delivery, recorded in
  ui-bugs; one complete run passed drag, Undo and captures. This does not establish
  broad native-suite timing reliability.
