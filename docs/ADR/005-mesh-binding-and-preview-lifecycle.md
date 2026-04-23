# ADR 005: Centralized Mesh Binding And Preview Lifecycle

## Status

Proposed

## Context

Cycle currently uses meshes from `MeshLibrary` in two different ways at once:

- some clients pull the current mesh on demand from `MeshLibrary`,
- other clients cache a raw `Mesh*` that is pushed into them with `setMesh(...)`.

This split is visible across several subsystems:

- `Interactor::getMesh()` pulls from `MeshLibrary`,
- `MeshRasterizer` stores a borrowed raw `Mesh*`,
- effect UIs such as `IrModellerUI` and `WaveshaperUI` push meshes into both UI rasterizers and DSP objects,
- hover preview flows call `previewMesh(...)` and `previewMeshEnded(...)` directly on individual clients,
- preset load repairs some stale bindings manually in `FileManager::doPostPresetLoad()`.

Recent debugging in `FXRasterizer::calcCrossPoints()` exposed the failure mode clearly: a rasterizer can still hold a pointer to a mesh that is no longer the current library-owned mesh after preset load or mesh replacement.

The current code already contains architectural signals that the model is incomplete:

- `FXRasterizer.cpp` includes an in-code note that the mesh pointer is junk and asks for a lifecycle model.
- `FileManager::doPostPresetLoad()` manually rebinds `IrModellerUI` and `WaveshaperUI` after `Document::open(...)`.
- `MeshLibrary::Listener` exists, but today it is used mainly for update-graph wiring and high-level panel refresh, not as the authoritative rebinding mechanism for all mesh consumers.
- `MeshSelectionClient` and `MeshSelectionClient3D` duplicate part of a lifecycle model for hover preview and selection, but only for selector-driven interactions.

There is also a special-case preview workflow:

- mesh selectors can temporarily preview another mesh on hover,
- preview currently replaces the effective mesh seen by a panel or effect client,
- preview depth is operationally one level deep: base current mesh plus one temporary hover-preview mesh.

The current system therefore has three coupled concerns with no single owner:

1. persistent layer selection within `MeshLibrary`,
2. temporary preview override,
3. rebinding downstream consumers when the effective mesh changes.

That makes mesh state easy to desynchronize across UI rasterizers, DSP-side rasterizers, and intermediary components.

## Decision

We will make `MeshLibrary` the single source of truth for effective mesh selection and treat mesh pointers held by consumers as non-owning snapshots that must be refreshed only through library-originated binding events.

The design decisions are:

- `MeshLibrary` remains the owner of persistent mesh layers.
- mesh consumers stop owning selection state implicitly through cached `setMesh(...)` calls.
- preview state is centralized alongside the layer group, not scattered across individual clients.
- the system exposes one authoritative concept: the effective mesh for a layer group.
- consumers may still cache a raw `Mesh*` for hot-path use, but that cache must be refreshed from a centralized mesh-binding notification path.

Concretely, the model becomes:

- each layer group has a persistent current layer,
- each layer group may also have an optional preview mesh override,
- `getCurrentMesh(group)` returns the effective mesh: preview mesh when active, otherwise the persistent current layer mesh,
- downstream consumers subscribe to effective-mesh changes for the groups they care about,
- mesh selection UI becomes one producer of those state changes rather than a parallel lifecycle system.

The system should not require downstream clients to know whether the effective mesh changed because of:

- normal selection,
- preset load,
- layer replacement,
- preview start,
- preview end.

Most clients only need to know that the effective mesh for a group changed and must be rebound or refreshed.

## Consequences

### Positive

- one source of truth for which mesh is active for a group at any moment,
- no need for ad hoc `setMesh(getMesh())` repair code,
- preset load can rebind consumers through the same path as normal selection and preview,
- preview semantics become explicit and centralized instead of being hidden in selector callbacks,
- UI rasterizers and DSP rasterizers can stay consistent more reliably,
- future debugging improves because the mesh transition path is observable in one subsystem.

### Negative

- `MeshLibrary` becomes responsible for more state and event coordination,
- some existing code that directly calls `setMesh(...)` will need refactoring,
- a migration period will temporarily retain both direct assignment and library-driven rebinding,
- thread-sensitive consumers will still need careful lock boundaries when swapping cached pointers.

## Preview Model

Preview should not be represented as replacing ownership in the layer list.

Instead:

- keep the existing persistent layer list unchanged,
- add an optional preview mesh pointer per layer group,
- compute effective mesh as `previewMesh != nullptr ? previewMesh : currentLayer.mesh`.

This fits the observed workflow better than a list-of-stacks design because current preview behavior is only one level deep and transient.

The important policy is:

- preview is an override, not a mutation of persistent selection.

That keeps layer ownership simple while still allowing hover-preview behavior.

## Alternatives Considered

### Keep the current mixed push/pull model

Rejected.

This is the current state, and it is already producing stale-pointer bugs and manual repair logic after preset load.

### Convert everything to pure pull on every use

Rejected as the primary design.

For many consumers this is conceptually clean, and `Interactor::getMesh()` already follows it. However, some rasterizers and DSP objects are used in tighter update paths where repeatedly walking back through the repo for every operation is not ideal. A cached non-owning pointer is acceptable, but it must be refreshed from a centralized event source rather than assigned opportunistically.

### Make every consumer store an owning smart pointer

Rejected.

`MeshLibrary` is already the owner of persistent mesh instances. Duplicating ownership into each consumer would increase lifetime complexity and break the model where many clients observe the same current mesh.

### Model preview as a stack per layer group

Rejected for now.

The operational preview depth is only one. A general stack would add noise without immediate benefit. If future workflows require nested preview or transactional mesh overlays, the override slot can later be generalized.

### Keep `MeshSelectionClient` as the main lifecycle API and expand it

Rejected as the primary boundary.

`MeshSelectionClient` is selector-centric and UI-centric. Preset load, programmatic layer changes, and non-selector mesh replacement also need to drive rebinding. The authoritative lifecycle must live below the selector layer.

## Implementation Plan

### Phase 1: Introduce effective-mesh events in `MeshLibrary`

- Add a group-scoped notification for effective mesh changes.
- Include both persistent selection changes and preview start/end in that path.
- Keep existing `layerChanged(...)` for structural metadata and dependency recalculation until callers are migrated.

Suggested event shape:

- `effectiveMeshChanged(int layerGroup, Mesh* mesh)`
- optionally `effectiveMeshAboutToChange(...)` later if a caller truly needs a two-phase swap

Clients that currently care about preview semantics specifically should first be challenged to justify that requirement. Most should only consume the new effective mesh.

### Phase 2: Move preview state into `MeshLibrary`

- Add optional preview mesh storage per layer group.
- Expose methods such as:
  - `beginPreviewMesh(int group, Mesh* previewMesh)`
  - `endPreviewMesh(int group)`
  - `getEffectiveMesh(int group)`
- Make current `getCurrentMesh(int group)` either return the effective mesh or add a separate explicitly named accessor and migrate callers carefully.

Naming must be explicit enough to avoid reintroducing confusion between:

- persistent current layer mesh,
- effective current mesh,
- preview override mesh.

### Phase 3: Rebind mesh consumers through the centralized path

- Convert `IrModellerUI`, `WaveshaperUI`, related rasterizers, and other direct `setMesh(...)` users to subscribe to effective-mesh changes for their layer groups.
- Keep raw `Mesh*` members in rasterizers only as non-owning cached bindings.
- Remove preset-load-specific rebind code in `FileManager::doPostPresetLoad()` once the listeners cover those clients.

Migration priority should start with the highest-risk stale-pointer paths:

- `IrModellerUI` / `IrModeller`,
- `WaveshaperUI`,
- `FXRasterizer`,
- envelope rasterizer clients that currently mix direct assignment and lookup.

### Phase 4: Collapse selector callbacks into the library event model

- Shrink `MeshSelectionClient` so it becomes an intent interface for selection UI, not a lifecycle authority.
- Selector code should request:
  - set persistent current mesh,
  - begin preview,
  - end preview.
- Downstream rebinding should happen from `MeshLibrary` notifications, not by selector code manually touching each consumer.

### Phase 5: Add preset-load rebinding guarantees

- Ensure `MeshLibrary::readJSON(...)` and `readXML(...)` emit the same effective-mesh notifications needed after library reconstruction.
- Ensure document-load listeners can rely on mesh consumers being rebound before post-load DSP updates run.
- Remove special-case post-load mesh rebinding once validated.

## Notes For This Repository

The current pain points that motivate this ADR are visible in these specific patterns:

- `MeshRasterizer::setMesh(...)` stores a borrowed raw pointer with no lifecycle guarantee.
- `FXRasterizer::calcCrossPoints()` already documents the stale-pointer problem in-place.
- `IrModellerUI::setMeshAndUpdate(...)` pushes one mesh into both UI and DSP state, while `getCurrentMesh()` still pulls from `MeshLibrary`.
- `WaveshaperUI` does the same kind of dual wiring.
- `MeshSelectionClient3D::updateEverything(...)` manually pushes mesh changes into rasterizer state.
- `FileManager::doPostPresetLoad()` repairs effect mesh bindings explicitly after document load.

Those are all symptoms of the same architectural gap, not isolated bugs.

## Follow-Up Work

- Add a focused mesh-binding test plan for:
  - preset load,
  - factory preset switch,
  - hover preview start/end,
  - replacing the current mesh from selector load,
  - switching layer groups,
  - DSP effect mesh updates while audio is suspended.
- Decide whether `MeshLibrary::Listener` should be extended or whether a separate `MeshBindingListener` is clearer.
- Audit every direct `setMesh(...)` call site and classify it as:
  - authoritative selection mutation,
  - temporary preview override,
  - downstream cache refresh.
- Remove manual post-load rebinding code once the centralized path is proven.
