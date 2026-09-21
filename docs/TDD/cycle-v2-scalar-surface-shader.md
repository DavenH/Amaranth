# Cycle V2 Scalar-Surface Shader

Status: Implemented

## Implementation Record

### Baseline (2026-09-20)

| File | Lines before work | Existing responsibility relevant to this change |
| --- | ---: | --- |
| `lib/src/UI/Panels/Panel3D.cpp` | 1,031 | Panel orchestration plus per-column value-to-colour conversion and quad-strip geometry |
| `lib/src/UI/Panels/GLPanelRenderer.cpp` | 209 | Shared fixed-function GL submission for Cycle 1 and Cycle V2 panels |
| `cycle-v2/src/Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.cpp` | 106 | Deterministic CPU heatmap and compact/headless path |
| `cycle-v2/src/Nodes/Trimesh/Editor/TrimeshWidget.cpp` | 830 | Trimesh editor composition and routing |
| `cycle-v2/src/UI/NodeCanvas.cpp` | 2,702 | Canvas lifecycle and high-level orchestration |
| `cycle-v2/src/Nodes/Trimesh/Panel/TrimeshPanelHosts.cpp` | 270 | Shared-canvas panel GL setup and rendering |
| `cycle-v2/src/Nodes/Trimesh/Panel/TrimeshPanel3D.cpp` | 163 | Cycle V2 domain/profile adaptation to the mature panel |

The pre-change architecture audit reported the existing Cycle V2 size triggers,
including plan-level triggers for `NodeCanvas.cpp` and review-level triggers for
`TrimeshWidget.cpp`. Neither file is an implementation target for the shared
renderer slice.

### Technical Design

- The mature `Panel3D` grid and interaction path remains authoritative. This
  change reuses its prepared columns, zoom mapping, clipping, surface-cache
  bake lifecycle, and overlay ordering unchanged; it does not evaluate a curve
  or rasterize a mesh.
- A shared scalar-surface material/evaluator below both products owns palette,
  relief, and derivative classification. `TrimeshRenderProfile` translates
  domain semantics into that material; Cycle 1 panels select the equivalent
  material directly at their existing domain boundary.
- A dedicated GL scalar-surface collaborator owned by `GLPanelRenderer` owns
  shader, texture, upload cache, capability detection, and GL-thread cleanup.
  `GLPanelRenderer` remains routing glue rather than absorbing shader policy.
- `Panel3D` submits an immutable column-major scalar-grid view and the exact
  scaled rectangle. A successful shader draw replaces only the old per-column
  colour conversion and quad-strip submission. Failure returns to the existing
  CPU/fixed-function path with the same prepared values and interaction.
- Grid upload identity consists of the source identity/revision, dimensions,
  and value transform. Material-only updates are uniforms. Context recreation
  clears GL handles and forces the next successful draw to upload once.
- The steady-state cost is one cached surface texture draw. A changed product
  performs one `O(columns * rows)` upload. Neither draw path scans the graph,
  mesh, audio resources, or unrelated panel state.

### Ownership And Deletion Targets

- Material policy: shared scalar-surface material plus the profile/domain
  adapter that constructs it.
- GL resource lifetime and upload invalidation: the GL scalar-surface
  collaborator owned by `GLPanelRenderer`.
- CPU fallback: the shared CPU material evaluator, called by
  `TrimeshSurfaceRenderer` and by the retained capability fallback.
- Delete the live `Panel3D` colour-array path once every panel domain is backed
  by the typed material and forced-capability fallback. Until then it is the
  documented fallback and remains isolated behind a failed scalar draw.
- Stable end state: one material contract and evaluator, one shared GL
  renderer, no widget-owned shaders, and no duplicated curve/grid evaluation.

### Implemented Slice (2026-09-20)

- Cycle 1 was migrated first. `Waveform3D` now selects the shared signed
  material, and `Panel3D` submits its authoritative prepared scalar columns to
  the shared renderer. Its retained fixed-function fallback uses a gradient
  generated from the same material contract.
- `GLScalarSurfaceRenderer`, owned by `GLPanelRenderer`, uploads one
  single-channel floating-point texture and shades one rectangle. It owns
  program/texture creation and destruction, stable-product upload
  invalidation, uniforms, and the forced-capability fallback boundary.
- Cycle V2 adapts `TrimeshRenderProfile` to the shared typed material and gives
  expanded panels a stable scalar-product revision. Spectral pitch-column
  mapping remains profile-owned and is performed before the renderer
  boundary.
- Compact and headless rendering use a revision-keyed CPU image made by the
  shared evaluator. The former live per-cell `Graphics` fill loop and the
  profile-local gradient formula were deleted.
- Expanded products use a fixed 768 by 320 grid. Resizing changes presentation
  bounds without changing the product resolution or scalar texture cache key.
- Shader capability can be disabled with
  `CYCLE_DISABLE_SCALAR_SURFACE_SHADER`; the existing panel fallback then
  renders the same domain palette instead of producing an empty surface.

### Post-Implementation Architecture Review

| File | Lines after work | Review result |
| --- | ---: | --- |
| `lib/src/UI/Panels/Panel3D.cpp` | 1,069 | Remains panel orchestration; 38 lines adapt the locked authoritative grid to an immutable render view. Material and GL policy stay outside. |
| `lib/src/UI/Panels/GLPanelRenderer.cpp` | 218 | Nine routing/lifecycle lines; shader policy is delegated to the dedicated collaborator. |
| `lib/src/UI/Panels/GLScalarSurfaceRenderer.cpp` | 562 | Cohesive GL resource, upload-cache, uniform, draw, and opt-in parity-validation owner. |
| `lib/src/UI/Panels/ScalarSurfaceMaterial.cpp` | 198 | Single palette, derivative, and CPU-reference policy owner. |
| `cycle-v2/src/Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.cpp` | 90 | Reduced by 16 lines and now delegates colour/relief policy. |
| `cycle-v2/src/Nodes/Trimesh/Editor/TrimeshWidget.cpp` | 830 | No growth; its existing composition role only selects the fixed expanded product resolution. |
| `cycle-v2/src/UI/NodeCanvas.cpp` | 2,714 | Remains the shared-context lifecycle owner; context counters and the narrow automation recreate hook sit beside the existing attach/detach callbacks. |
| `cycle-v2/src/Nodes/Trimesh/Panel/TrimeshPanelHosts.cpp` | 276 | Six lifecycle lines release renderer resources while the shared context is current. |
| `cycle-v2/src/Nodes/Trimesh/Panel/TrimeshPanel3D.cpp` | 168 | Five adapter lines translate profile semantics to the shared material. |

The after-change architecture audit reports the same 20 pre-existing size
triggers and no new trigger. `TrimeshWidget.cpp` remains a review-level file,
but this slice adds no lines or policy to it. `NodeCanvas.cpp` was already a
plan-level file and grew by 12 lines for lifecycle counters and an automation
entry point. Those members are cohesive with its existing ownership of the
shared `OpenGLContext`; extracting them would split the attach/detach lifecycle
across owners. Material selection has one profile/domain decision site, GL
resource and cache policy has one renderer owner, and callers provide only
scalar data, bounds, revision, and material facts. No graph, mesh, curve, or
DSP behavior was copied.
`CycleV2Automation.cpp` remains a review-level command router at 891 lines; its
seven added lines only dispatch the context-recreation command to the existing
workspace/canvas owner and introduce no rendering or lifecycle policy.

### Verification Record

- `AmaranthLib_tests '[surface-material]'`: 87 assertions in four cases cover
  exact anchors, signed monotonicity, flat/plane/ridge/valley derivatives,
  sub-threshold ripple rejection, and upload-cache invalidation.
- Focused Cycle V2 surface, spectral RGBA parity, orientation, resize,
  magnitude-scale, and phase-mapping tests: 101 assertions across six cases.
- Automation registry and OpenGL lifecycle diagnostics: 19 assertions across
  two cases.
- Standalone Debug `Cycle` and `CycleV2` builds completed with
  `--parallel 10`.
- Production fixtures passed for Cycle 1, Cycle V2 compact, Cycle V2 expanded,
  and forced shader disable. The expanded report records a 768 by 320 product;
  the Cycle 1 diagnostics report `GL_NO_ERROR` at a 2.0 rendering scale.
- With `CYCLE_VALIDATE_SCALAR_SURFACE_SHADER=1`, the renderer draws a
  representative slope/curvature fixture for signed amplitude, unipolar
  magnitude, and bipolar phase into an offscreen framebuffer. GPU RGBA readback
  agrees with the CPU reference within the explicit 4/255 per-channel
  tolerance; the observed maximum error is 3/255. Ordinary rendering performs
  no readback.
- `cycle-v2-agent-scalar-surface-lifecycle.json` recreates the shared context
  in process. Diagnostics advance from create/close counts 1/0 to 2/1, the
  expanded editor remains present, and the shader parity validation passes
  both before and after recreation, proving a clean resource rebuild and draw.
- Visual artifacts: `/tmp/cycle-scalar-v1.png`,
  `/tmp/cycle-scalar-v2-compact.png`, and
  `/tmp/cycle-scalar-v2-expanded.png`. The expanded capture is 1728 by 962 and
  includes the production 733 by 399 shared-GL panel.
- `git diff --check` and the scalar-math hot-loop self-check pass. The local
  environment does not provide `clang-tidy`, so that optional check could not
  run.

### Visual And Preset-Lifecycle Correction (2026-09-20)

- The initial absolute-curvature accent emphasized the discontinuities of a
  linearly filtered scalar texture and produced paired, blotchy outlines. The
  corrected evaluator first applies the same five-tap cross smoothing on CPU
  and GPU, then uses signed curvature: ridges receive a restrained luminance
  lift and valleys receive cavity shadow. Relief no longer blends toward a
  separate accent hue.
- Spectral magnitude again samples the authoritative 512-pixel
  `burntalum_png` asset. The shader owns a static palette texture in addition
  to its single live scalar-data texture; this does not change scalar upload
  identity or steady-state graph/render cost.
- The GPU readback tolerance is explicitly 5/255 after the wider derivative
  stencil; the observed maximum error is 5/255 before and after context
  recreation.
- Cycle V2 preview resources retain only the durable document graph. A
  transient editing overlay is passed explicitly for the preview call that
  consumes it, so live guide context remains current without replacing the
  retained pointer and dangling after gesture completion.
- Cycle 1 unison visualization now sizes its scratch arena from the largest
  actual preset column. It no longer assumes all retained columns match the
  current FFT-order-derived size during preset replacement.
- Regression fixtures alternate five Cycle V2 graph loads and seven Cycle 1
  preset loads. Both completed with no new crash report. Production captures:
  `/tmp/cycle-surface-relief-v1.png`,
  `/tmp/cycle-surface-relief-v2.png`, and
  `/tmp/cycle-surface-magnitude-v2.png`.

The correction touches existing Cycle V2 size-trigger files only as narrow
orchestration adapters. `NodeCanvas` owns the current editing-graph read and
passes it to the preview boundary; `NodeCanvasPresentation` packages the graph
already present in its immutable frame; `NodePreviewRenderer` forwards that
fact without deciding lifecycle. `NodePreviewResources` remains the owner of
preview widgets, cached sprites, pitch contexts, and their graph-lifetime
boundary: its retained pointer is durable, while transient graphs are
call-scoped. `NodeCanvasEditorCoordinator` supplies the durable document graph
for its durable-node path. No second lifecycle decision site or graph copy was
introduced. The shared GL renderer is 646 lines after the correction, below
its review trigger, and remains cohesive around shader compilation, its scalar
and static palette textures, upload invalidation, draw state, cleanup, and
opt-in parity validation. The architecture audit still reports the same 20
  pre-existing size triggers.

### Pearl Palette And Relief Correction (2026-09-20)

- Signed amplitude now uses a shared nine-stop scalar palette instead of the
  original three-anchor interpolation. Deep indigo troughs rise through blue
  and periwinkle before converging narrowly on dark low-chroma plum at zero;
  positive values rise through warm rose, apricot, peach, and pale amber. The
  lookup has no coordinate, band, or region input, so equal scalar values
  always receive the same base colour.
- Signed surfaces are opaque and have material-local luminance bounds. Their
  stronger diffuse, specular, ridge, and cavity response does not alter the
  established spectral-magnitude or bipolar-phase materials. Lighting remains
  a scalar multiplier after palette lookup and therefore cannot rotate hue or
  move the zero boundary.
- The CPU evaluator owns the palette stops and uploads them as shader uniforms,
  avoiding a second hard-coded GPU palette. The shader matches the CPU
  evaluator's eight-bit palette quantization before relief modulation. GPU
  parity uses an explicit 8/255 limit for driver sampling differences at the
  smoothed texture edges; the observed maximum is 7/255 before and after
  in-process context recreation.
- Focused material tests now include 50 assertions across five cases, including
  the periwinkle negative shoulder, plum neutral, peach positive branch,
  coordinate-independent constant-scalar rendering, luminance-only relief,
  and exact preservation of the legacy burnt-alum magnitude palette.
- Production Cycle 1 review capture:
  `/tmp/cycle-surface-pearl-final-v1-clean.png`. The capture shows smooth
  directional embossing without the paired curvature outlines or stippled
  calculation-noise appearance of the earlier derivative shader.
- The seven-load Cycle 1 and five-graph Cycle 2 churn fixtures completed with
  no crash or OpenGL error. The focused Cycle 2 render-profile case passes 52
  assertions with the opaque signed material, and final GPU validation passes
  at 7/255 both before and after context recreation.

### Signal Spy Contrast Normalization (2026-09-20)

- Signal Spies previously applied their soft-sign time transform directly to
  raw audio amplitude. Ordinary levels therefore occupied only a narrow range
  around scalar zero and appeared uniformly plum and desaturated even though
  the shared material could render the full palette.
- `PreviewContrastNormalization` is the authoritative display-only peak
  normalizer. It extracts the mature normalization formerly local to
  `DefaultOutputPreview`; both default-output presentation and Signal Spy
  presentation now reuse it. Raw probe values, statistics, audio products, and
  authored Trimesh grids remain unchanged.
- Time Spies normalize to a peak of three before the existing soft-sign curve,
  placing their strongest values at 0.125 and 0.875 rather than clipping to the
  palette endpoints. Spectral-magnitude Spies normalize their visible bins to
  unity before logarithmic display mapping; DC is excluded from the peak just
  as it is excluded from the displayed logarithmic rows. Phase retains its
  existing domain normalization.
- This adds one bounded `O(columns * rows)` pass over the local render product.
  It does not traverse the graph or mutate runtime data, and Mesh Surface
  previews retain absolute authored gain. The old private default-output
  normalizer was deleted rather than copied.
- Focused `[probe][ui]` coverage includes low-level chroma, gain-invariant time
  and magnitude Spy images, above-unity detail, DC exclusion, and a negative
  assertion that Trimesh previews are not normalized. Production captures:
  `/tmp/cycle-v2-spy-compact.png` and `/tmp/cycle-v2-spy-detail.png`.

The architecture audit remains at the same 20 pre-existing triggers.
`NodePreviewRenderer.cpp` grows from 1,032 to 1,042 lines and remains the
preview-role/domain routing and painting owner; it supplies only role,
dimensions, and local values to the normalization boundary. The new 53-line
runtime helper owns peak selection and scaling, while `TrimeshRenderProfile`
continues to own domain mapping and `TrimeshSurfaceRenderer` continues to own
image generation. The former duplicate decision site in
`DefaultOutputPreview.cpp` was deleted, reducing that file to 59 lines.

### Resolution-Correct Pseudo-Normal Relief (2026-09-21)

- The shared material retains the nine signed-amplitude anchors, but their
  scalar positions are now explicit. Cool dusk and warm dusk stops flank a
  narrow charcoal zero interval. A neutral-specific luminance floor prevents
  a steep zero crossing from turning the semantic neutral into an artificial
  black contour; it does not move or recolour the sign boundary.
- The former five-tap emboss calculation was deleted. CPU and GPU evaluators
  now sample deliberate normalized radii, derive aspect-correct gradients,
  construct a three-dimensional pseudo-normal, and apply normal-based diffuse
  plus Blinn-Phong specular lighting. Broad forms therefore retain coherent
  lighting as grid resolution changes.
- Three projected-light samples provide bounded directional horizon shadow.
  Small- and large-radius curvature contribute only a restrained cavity term.
  The centre scalar remains authoritative for palette lookup; neither wider
  sample path blurs or replaces it.
- Lighting is evaluated in linear RGB. Specular blends toward sign-specific
  pearl tints, while diffuse, horizon shadow, and cavity remain luminance
  effects. The CPU transfer functions use one-time lookup tables, avoiding
  scalar power calls in the image loop. The remaining scalar square root is a
  deliberate fallback for one coupled three-component normal per output
  pixel; expressing it through `Buffer` would require several product-sized
  temporaries and would not remove the per-pixel cross-channel dependency.
- Display aspect is passed to Cycle V2 compact, expanded, and Signal Spy CPU
  images and participates in compact-image cache identity. Cycle 1 and Cycle
  V2 expanded panels continue to obtain the same fact from their shared
  `ScalarSurfaceRenderData::bounds` boundary.
- GPU uniform translation was extracted to the 127-line
  `GLScalarSurfaceUniforms.h`. `GLScalarSurfaceRenderer.cpp` is 717 lines and
  remains responsible only for shader/resource lifecycle, texture upload,
  draw submission, and parity validation. `ScalarSurfaceMaterial.cpp` is 512
  lines and remains the single CPU material/reference owner. The touched
  Cycle V2 review-trigger files grew only through aspect translation:
  `NodePreviewRenderer.cpp` is 1,052 lines and `TrimeshWidget.cpp` is 839
  lines; neither gained material or sampling policy.
- Focused material coverage now includes scalar-only palette semantics, narrow
  zero continuity, uniform flat fields, mirrored pseudo-normal response,
  view-dependent specular response, resolution-stable broad planes,
  directional horizon shadow, constant-offset invariance, and legacy spectral
  palette preservation. The opt-in offscreen GPU comparison passes at 2/255
  maximum channel error.
- Production review artifacts are
  `/private/tmp/cycle-surface-normal-final-v1.png`,
  `/private/tmp/cycle-surface-normal-v2-expanded.png`, and
  `/private/tmp/cycle-v2-spy-detail.png`. Seven-load Cycle 1 and five-graph
  Cycle V2 preset-churn fixtures completed with no failed command or new crash.
  Existing startup `FileManager.cpp:174` and Trimesh `Curve.cpp:56/57`
  assertions remain recorded in `ui-bugs.md`.

## Objective

Move live Trimesh heatmap colouring and relief shading from per-cell CPU paint
and per-column colour arrays into one shared OpenGL scalar-surface renderer.
The renderer receives the authoritative prepared scalar grid and applies
semantic colour, smooth pseudo-surface lighting, and derivative-gated detail on
the GPU. It must not evaluate curves, rasterize a mesh, or invent signal detail.

The initial product is the orthographic top-down heatmap used by compact and
expanded Trimesh presentation. A navigable geometric surface may reuse the
material in a later slice, but is not a prerequisite for replacing the live
heatmap path.

## Visual References

The calm same-palette treatment is the preferred baseline. The curvature-hue
variant demonstrates a possible optional detail channel, not the default.

| Preferred amplitude and same-palette edge treatment | Optional restrained curvature-hue treatment |
| --- | --- |
| <img src="figures/cycle-v2-visual-language/surface-palette-accent.png" alt="Blue troughs, charcoal zero region, and orange peaks with same-palette edge highlights" width="420"> | <img src="figures/cycle-v2-visual-language/surface-curvature-accent.png" alt="Blue troughs, charcoal zero region, and orange peaks with faint curvature hue seams" width="420"> |

Both mockups exaggerate node scale and selection chrome. They specify the
surface material only. Production review occurs at the real compact-node and
expanded-editor sizes.

## Current Authoritative Implementations

- `TrimeshGridRenderService` and `TrimeshRenderData` own the prepared scalar
  grid consumed by presentation. The shader must consume that result rather
  than slice or evaluate a mesh again.
- `TrimeshRenderProfile` owns domain semantics, scale policy, pitch/frequency
  remapping, and the surface/curve/slice presentation contract.
- `TrimeshSurfaceRenderer` is the current CPU reference and headless/offline
  image path. It creates compact heatmaps and remains the deterministic
  fallback for contexts without the required OpenGL capability.
- `Panel3D` currently maps scalar values to CPU colours, and
  `GLPanelRenderer::drawSurfaceColumn` submits those colours and 2D vertices as
  fixed-function quad strips. Cycle V2's expanded Trimesh editor reaches this
  mature panel implementation through the existing shared panel host.
- `NodeCanvas` owns the Cycle V2 OpenGL context lifecycle.

No new implementation may duplicate curve evaluation, Trimesh slicing,
frequency remapping, preview normalization, or editor interaction.

## Semantic Material Contract

### Signed amplitude

For bipolar time-domain surfaces, value is the semantic truth:

- the negative extreme maps to deep navy/ultramarine;
- values approaching zero from below may pass through a brighter periwinkle
  shoulder before converging on the neutral boundary;
- zero maps to low-chroma charcoal or dark plum, never teal or orange;
- the positive branch passes through warm rose and apricot to a pale
  peach/amber extreme; and
- colour is a continuous function of scalar value only. Local lighting may
  change luminance, but must not move the zero boundary.

The shader receives display-normalized values after the authoritative profile
mapping. For a bipolar profile, `0.5` is exactly neutral. Lighting and detail
may modulate the resulting colour within bounded limits but may not reclassify
its sign.

Unipolar magnitude and bipolar phase profiles retain their existing domain
semantics. This work does not force the time-domain palette onto every signal
domain.

### Relief

The fragment shader samples immediate horizontal and vertical neighbours from
the scalar texture. Central differences form a pseudo-normal for restrained
diffuse and specular response. A second derivative or equivalent local
curvature measure identifies ridges, valleys, and IR-modelled ripples.

- Flat regions receive no derivative accent.
- A constant planar ramp may receive smooth directional lighting but no
  curvature seam.
- A localized ridge or valley may receive a narrow accent whose width is
  bounded in display pixels and whose strength falls quickly below threshold.
- The default accent stays inside the signed-amplitude palette: negative
  detail becomes slightly brighter or more saturated blue, and positive detail
  becomes slightly brighter or more saturated amber. Near-zero detail may use
  a fine neutral silver highlight.
- A later optional material may use low-opacity cyan/violet seams for opposite
  curvature signs. It must remain subordinate to the amplitude palette and
  must not tint broad regions.

The material must remain smooth and high-resolution. It must not add noise,
grain, micro-ripples, polygon facets, displaced geometry, or terrain texture.

## Rendering Boundary

Introduce a narrowly owned scalar-surface rendering operation beneath the
panel and Cycle V2 presentation layers. The intended data flow is:

```text
Trimesh render service
    -> profile-owned display transform
    -> immutable scalar grid snapshot
    -> cached single-channel GL texture
    -> full-rectangle fragment shader
```

The live top-down view draws one rectangle. Grid dimensions, texel size,
palette anchors, opacity, derivative thresholds, relief gain, light direction,
and specular strength are explicit shader inputs. Material policy must be a
typed C++ value produced from `TrimeshRenderProfile`, not duplicated constants
spread between widgets and GLSL source.

Prefer a floating-point single-channel texture when the active context
supports it. Capability detection and any packed fallback belong inside the GL
renderer. A missing shader or texture capability selects the CPU reference
renderer; it must not produce an empty surface or a different domain mapping.

The scalar texture cache key includes scalar-product identity or revision,
dimensions, and any profile transformation that changes uploaded samples.
Palette and lighting-only changes update uniforms without uploading the grid.
Context creation, shader compilation, texture creation, and deletion occur on
the owning GL thread.

## CPU Reference And Snapshot Policy

Keep one CPU material evaluator for semantic tests, headless automation,
embedded preset previews, and a capability fallback. Refactor
`TrimeshSurfaceRenderer` to use that evaluator rather than preserve a second
unrelated palette formula.

Do not read pixels back from the GPU for ordinary compact rendering. Compact
nodes should either draw through the canvas OpenGL pass into their declared
bounds or consume a cached presentation snapshot produced only when the scalar
product changes. The selected route must preserve node overlap and clipping and
must not create one OpenGL context per node.

## Complexity And Lifecycle Contract

For a grid with `C * R` scalar samples:

- a changed scalar product performs one `O(C * R)` upload;
- an unchanged frame performs no grid mapping, allocation, upload, or CPU
  per-cell/per-column colour generation;
- drawing is one bounded GPU surface draw plus an optional grid overlay;
- palette or lighting changes are `O(1)` CPU work plus a redraw;
- resizing changes the viewport and uniforms, not the scalar product; and
- no shader, texture, or presentation operation runs on the audio thread.

Live movement may regenerate only the surface product required by that edit.
It must not copy a `NodeGraph`, deep-copy an unrelated mesh, serialize state,
or prepare durable audio resources.

## Shared 3D Material Follow-up

A later perspectival editor may submit an indexed grid with amplitude as the
geometric height and use actual or grid-derived normals. It should reuse the
same profile-produced palette anchors and material controls. The 3D editor must
not fork the signed-amplitude mapping or become the implementation behind the
top-down view.

The mature Cycle 1 `Interactor3D` and panel interaction are the authoritative
starting points for orbit, projection, selection, and slicing. Before porting,
document pointer-down, movement, and commit costs and preserve their asymptotic
behavior.

## Implementation Slices

1. Extract a typed surface-material description and CPU reference evaluator
   from the existing profile/renderer without changing output.
2. Add the shared scalar texture, shader program, capability fallback, and GL
   lifecycle beneath the panel renderer. Migrate the expanded top-down surface
   while preserving the existing mesh and interaction path.
3. Add derivative relief and the signed time-domain palette behind explicit
   material parameters. Keep the same-palette accent as the default.
4. Integrate compact Trimesh surfaces with the single canvas GL context or a
   revision-keyed snapshot cache; delete the superseded live CPU colour path.
5. Consider the navigable 3D surface as a separate interaction TDD after the
   2D semantic material and performance contracts pass.

## Tests And Visual Proof

- Exact anchor tests cover negative extreme, zero, and positive extreme.
- Dense samples on each side of zero prove monotonic signed colour mapping and
  prove that zero remains low-chroma.
- Synthetic flat, planar-ramp, ridge, valley, and low-amplitude-ripple grids
  verify derivative classification without relying on a screenshot alone.
- A flat grid has zero edge-mask coverage. A planar ramp has zero curvature
  accent. Localized features do not recolour unrelated flat samples.
- CPU reference and GPU readback agree within an explicit per-channel tolerance
  for every domain profile and representative derivative fixture.
- Upload/allocation counters prove that repeated unchanged frames perform no
  scalar upload or CPU colour conversion.
- Context destruction and recreation rebuild resources without stale handles,
  crashes, or a blank first frame.
- A forced capability failure produces the CPU fallback with the same semantic
  palette.
- Production-size captures cover a compact node, the expanded top-down editor,
  a high-DPI scale, and a resize. Review the neutral boundary, subtle IR detail,
  clipping, and absence of invented texture.

## Architecture Review And Deletion Targets

Before implementation, record baseline sizes for `Panel3D.cpp`,
`GLPanelRenderer.cpp`, `TrimeshSurfaceRenderer.cpp`, `TrimeshWidget.cpp`, and
the selected canvas integration files. Run the Cycle V2 architecture audit and
name the single owner of material policy, GL resource lifetime, cache
invalidation, and CPU fallback.

Completion requires:

- no second mesh rasterizer or curve evaluator;
- no widget-specific shader fork;
- no per-frame CPU conversion of an unchanged live scalar grid into RGB cells
  or colour arrays;
- deletion of the superseded live top-down colour-array/per-cell path after all
  callers use the scalar renderer or documented fallback; and
- retention of the CPU reference only for headless/offline output and genuine
  capability fallback.
