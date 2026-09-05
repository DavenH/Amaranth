# Cycle V2 Canvas Chrome Metrics

Status: Implemented (2026-08-28)

## Objective

Give Cycle V2 canvas chrome one restrained semantic corner-radius scale so its
silhouette can be tuned holistically without making every surface the same
shape. Reduce the current stack-of-rounded-cards effect while preserving
layout, hit geometry, renderer ownership, and domain-specific shapes.

## Current Failures

General canvas presentation currently chooses corner radii locally from an
unstructured range of 2, 3, 4, 5, 6, 7, 8, and 10 pixels. In particular:

- palette groups, workspace tiles, vacancies, utility panels, node shells, and
  expanded editors independently use large radii;
- nested surfaces frequently repeat those large radii, making the interface
  read as a stack of soft cards rather than a precise authoring tool;
- equivalent compact controls use different radii in the palette, workspace
  dock, keyboard, probe rail, and transform editor; and
- changing the overall silhouette requires finding presentation literals
  across unrelated renderers rather than adjusting one shared design system.

## Authoritative Implementations And Boundaries

- Each existing painter remains authoritative for its rendering, state,
  layout, hit targets, and lifecycle.
- `CanvasChromeMetrics` owns only shared visual measurements. It does not draw,
  lay out components, or introduce runtime look-and-feel ownership.
- The CPU and OpenGL renderers may consume the metrics directly, but renderer
  behavior remains in their existing implementations.
- Domain geometry remains authoritative for sockets, circles, keyboard keys,
  slider tracks and thumbs, meter segments, curve handles, and true pills.

The stable end state is a header-only metrics namespace alongside
`CanvasChromePalette`. It provides a five-step scale used by semantic role:

| Role | Radius | Typical use |
| --- | ---: | --- |
| Micro | 2 px | Minimap marks and other tiny rectangular indicators |
| Inset | 3 px | Plot wells, thumbnails, and viewport overlays |
| Control | 4 px | Compact buttons and segmented choices |
| Tile | 5 px | Palette entries, resource tiles, and vacancies |
| Panel | 6 px | Node shells, utility surfaces, and expanded panels |

The values are intentionally close together and capped at six pixels. Scale-
dependent canvas objects multiply the relevant reference value by the existing
canvas scale; fixed-size component chrome uses the value directly.

## Implementation Slice

1. Add a contract test that establishes the restrained, ordered scale and its
   semantic aliases.
2. Add the shared metrics header.
3. Migrate generic canvas chrome: node shells, palette, minimap, utility dock,
   workspace dock, keyboard navigation, Guide/Spy chrome, shared previews, and
   generic overlay panels.
4. Migrate expanded node-editor shells and their generic plot wells while
   retaining their domain rendering and control geometry.
5. Review production screenshots at native scale, then run focused interaction
   fixtures, the standalone build, and the complete Cycle V2 test suite.

## Negative Boundaries

- Do not change component bounds, padding, key proportions, hit targets, or
  gesture behavior.
- Do not replace derived pills, circles, sockets, slider thumbs/tracks, meter
  segments, or domain handles with shared chrome radii.
- Do not move rendering behavior into the metrics header or a JUCE
  `LookAndFeel` subclass. Manual JUCE and OpenGL painters need the same values;
  a passive shared metrics source is the correct boundary.
- Do not alter colours, typography, iconography, DSP, graph behavior, or node
  editor ownership in this slice.
- Do not add node-kind switches, compatibility adapters, or duplicated
  rendering paths.

## Verification

- Contract tests prove the shared scale is ordered, restrained, and maps each
  semantic role to its intended step.
- Focused canvas, keyboard, dock, Guide, Spy, and editor tests remain green.
- Production automation proves interaction and layout are unchanged.
- Native-scale screenshots verify that nested panels feel more precise without
  losing grouping or state clarity.
- Standalone Debug, the full Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review pass before commit.

## Deletion Targets

- Local general-purpose corner-radius constants and literals in canvas chrome,
  workspace chrome, node shells, common previews, and expanded editor shells.
- The `CanvasUtilityDock::cornerRadius` layout constant, whose presentation
  value does not belong to layout ownership.
- General plot-well corner copies in Delay, Reverb, Equalizer, Unison, Curve,
  and Trimesh presentation.

## Completion Criteria

- Generic canvas chrome uses `CanvasChromeMetrics` for its corner radii, with
  no remaining local copies covered by the deletion targets.
- No shared generic chrome radius exceeds six reference pixels.
- Domain- and shape-derived rounding remains locally authoritative.
- Layout, hit geometry, focus indication, editor state, and graph behavior are
  unchanged.
- Focused tests, automation, screenshot review, standalone build, full suite,
  and style checks complete with no new regression.

## Implementation Evidence

- Added the 11-line header-only `CanvasChromeMetrics` scale with five ordered
  roles from 2 through 6 reference pixels. It owns no drawing, layout, state,
  graph, domain, or interaction behavior.
- Canvas nodes, minimap, palette, utility surfaces, workspace controls, Guide
  and Spy chrome, keyboard navigation, shared previews, expanded editor shells,
  and generic plot wells now consume the semantic roles. The former 7, 8, and
  10 pixel general-purpose radii in those paths are deleted.
- Shape-derived rounding remains local for the Voice Context pill, sockets,
  circles, slider tracks and thumbs, meter segments, keyboard keys, Guide
  terminals, and the three-pixel overflow track. The last uses half its actual
  height, so changing the semantic scale cannot distort it.
- The focused metrics contract passes 9 assertions. Keyboard presentation
  passes 58 assertions, the shared palette passes 35, Guide presentation passes
  56, and Probe presentation passes 330 assertions across 27 cases.
- The nine-command production canvas fixture passes with the established
  190-pixel dock, 50/50 split, and 276-by-112 keyboard unchanged. Its report,
  filtered log, and native capture are
  `/private/tmp/cycle-v2-corner-metrics-report.json`,
  `/private/tmp/cycle-v2-corner-metrics-logs.txt`, and
  `/private/tmp/cycle-v2-corner-metrics.png`.
- Delay and Equalizer expanded-editor fixtures pass with no failed command or
  filtered warning. Native captures are
  `/private/tmp/cycle-v2-corner-delay.png` and
  `/private/tmp/cycle-v2-corner-equalizer.png`; their shells remain legible and
  grouped while no longer presenting a conspicuous 10-pixel silhouette.
- Standalone Debug builds successfully with `--parallel 10`. The complete
  Cycle V2 executable runs 534 cases: 533 pass, and the sole failure remains
  the pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion already
  recorded in `docs/TDD/ui-bugs.md`.
- No DSP, analysis, rasterization algorithm, or hot loop changed. Production
  review found no new node-kind branch, adapter, domain dependency, geometry
  owner, or duplicated renderer behavior.
