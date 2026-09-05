# Cycle V2 Canvas Layer Profiling and Caching

## Status

Implemented.

## Problem

Full Cycle V2 JUCE canvas paints average roughly 50-70 ms in focused Debug
fixtures and materially delay asynchronous preview publication. Existing frame
metrics identify the full paint as expensive but do not attribute that time to
the renderer's major layers. Choosing a cache without that attribution would
be speculative and could add invalidation complexity around the wrong work.

## Authoritative Implementations

- `NodeCanvasPresentation` remains the authoritative paint sequence and owns
  all canvas layer composition.
- `NodeCanvasScene`, node preview results, Guide/Spy presentation state, and
  `CanvasUtilityDock` remain the authoritative inputs to their existing
  renderers.
- `CanvasPerformanceMetrics` remains the fixed-storage collector and exports
  resettable duration distributions.
- `NodeCanvas` remains the frame/invalidation owner and composes presentation
  metrics into automation output.

Presentation reports timing through a narrow observer interface. The observer
may translate stage identifiers and durations into metrics; it must not alter
render order, invalidation, graph state, or presentation behavior.

## Layer Measurement Contract

Every full JUCE paint reports fixed-histogram duration distributions for:

- presentation-frame preparation;
- backdrop and relationship tether;
- cables and cable annotations;
- nodes and node previews;
- canvas utilities (minimap, legend, palette, and console);
- Guide shelf;
- Spy rail;
- dock chrome, relationship terminal, and Spy detail.

Status-only clipped paints continue to use the existing frame metric and do
not report full-layer samples. Recording performs no allocation in the paint
path. Automation reset clears layer distributions with the existing canvas
measurement window.

## Cache Selection Rule

Run the existing Trimesh/Spy and Delay slider fixtures through the external
session transport so message-loop paints occur between commands. Cache only a
layer that is both dominant in those distributions and has an authoritative,
compact invalidation key already available from its inputs.

The cache must:

- preserve the existing renderer as the only producer of pixels;
- key reuse on every input that can change the layer's visible output;
- invalidate on size, scale, viewport, graph, presentation, selection, hover,
  and dock state as applicable to the chosen layer;
- expose hit/miss counts and cached/rendered duration so the before/after
  effect is measurable;
- avoid copying node, cable, Guide/Spy, or preview rendering behavior.

If the dominant layer lacks a safe compact key, stop at the authoritative
boundary and document the prerequisite extraction rather than approximating
invalidation.

## Baseline Evidence

Both baselines used the standalone Debug build and the external session
transport, with message-loop settling between gesture commands.

The Trimesh/Spy point-edit workload produced nine full JUCE paints averaging
42.95 ms (155.74 ms maximum). The node layer was dominant at 24.25 ms average
and 107.49 ms maximum. Cables averaged 5.31 ms, the Spy rail 5.12 ms, and
utilities 7.27 ms with one 63.47 ms outlier.

The Delay beats-slider workload produced six full JUCE paints averaging
19.82 ms (84.79 ms maximum). The node layer was again the largest repeatable
layer at 9.04 ms average and 38.97 ms maximum. Cables averaged 3.81 ms;
utilities averaged 5.79 ms because of one 32.88 ms outlier. The parameter
updates themselves averaged 0.09 ms, confirming that presentation rather than
semantic edit dispatch dominates the gesture's visible cost.

The selected first cache boundary is therefore the node layer. Its key must
cover image geometry and display scale, viewport transform, committed or
transient graph content, preview presentation content, selection/hover state,
and any global preview context consumed by node painters.

## Implemented Cache

`NodeCanvasNodeLayerCache` owns transparent HiDPI sprites for visible nodes;
`NodeCanvasPresentation::paintNode` remains the sole producer of their pixels.
The cache compares immutable node presentation state (bounds, parameters,
ports, model identity, and editor state) and keys reuse on presentation and
viewport revisions, per-node preview-resource state, selection, display scale,
sprite bounds, and the global Unison/Voice context where applicable. Entries
that leave the visible set are released at the end of the frame.

The metrics export `presentationCache.nodeLayer` with per-node hit/miss counts.
The hit-duration distribution contains all-hit node-layer frames; the
miss-duration distribution contains mixed or all-miss frames.

## Optimization Evidence

The external-session Delay workload recorded 74 hits and 16 misses. Node-layer
mean fell from 9.04 ms to 4.11 ms (55%); total JUCE-paint mean fell from
19.82 ms to 18.11 ms despite an unrelated utilities outlier.

The Trimesh/Spy workload recorded 83 hits and 15 misses. Node-layer mean fell
from 24.25 ms to 16.10 ms (34%), and total JUCE-paint mean fell from 42.95 ms
to 31.90 ms (26%). Five of seven measured node-layer frames were all-cache-hit
frames averaging 0.025 ms. The remaining maximum is an authoritative preview
publication miss and is visible in the miss distribution rather than hidden.

Screenshots after parameter, popup-occlusion, and selection changes retained
the expected node previews and composition at native display scale. Focused
cache tests cover node-content, presentation, viewport, preview-resource,
context, selection, scale, and eviction invalidation.

The standalone Debug target builds successfully. The two focused cache and
metrics tests pass (38 assertions total). The complete Cycle V2 executable ran
512 test cases with 511 passing; its sole failure is the pre-existing canvas
edge-hover help-text regression recorded in `docs/TDD/ui-bugs.md`, outside the
paint/cache path.

## Completion Criteria

- Unit tests cover stage aggregation, export, and reset.
- Trimesh/Spy and Delay baselines identify the dominant JUCE layer.
- The selected cache reuses the authoritative renderer with complete
  invalidation coverage and records hits/misses.
- The same fixtures demonstrate reduced full-paint cost without hiding stale
  or superseded work.
- Focused visual/interaction automation remains correct.
- Cycle V2 tests and standalone build pass; style and diff checks are clean.

## Stable End State

Presentation-stage metrics remain permanent diagnostic infrastructure. The
first cache is a presentation-owned implementation detail behind the existing
paint API. No compatibility adapter or duplicated renderer is introduced.
