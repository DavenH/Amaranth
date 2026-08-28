# Cycle V2 Canvas Chrome Strokes

Status: Implemented (2026-08-28)

## Objective

Give Cycle V2 generic chrome one restrained border-weight hierarchy so resting
structure, active state, and keyboard focus are consistent and do not compete.
Make the hierarchy adjustable from the shared canvas metrics source without
absorbing domain visualization or shape-specific rendering.

## Current Failures

Generic canvas presentation currently uses local border weights of 1, 1.1,
1.2, 1.3, 1.4, 1.6, 1.8, and 2 pixels. Those small differences do not form a
readable system:

- equivalent panel shells use 1, 1.2, 1.3, or 1.5 pixels depending on owner;
- compact selected controls use 1.2, 1.4, 1.6, or 1.8 pixels;
- workspace selection and keyboard focus can both use 2 pixels, weakening the
  focus distinction; and
- changing the application's structural weight requires editing presentation
  literals across unrelated painters.

The baseline production capture is
`/private/tmp/cycle-v2-corner-metrics.png`.

## Authoritative Implementations And Boundaries

- Existing painters remain authoritative for rendering, geometry, state,
  layout, hit targets, and interaction.
- `CanvasChromeMetrics` owns only shared visual measurements. This slice adds
  no runtime look-and-feel object, renderer policy, or state mapping.
- `CanvasChromePalette` remains authoritative for semantic colours and generic
  control-state colour mapping.
- Domain and data presentation remains locally authoritative for cables,
  waveforms, transfer curves, EQ curves and markers, mesh traces and handles,
  sockets, meters, slider indicators, icons, and Guide tethers.

The stable generic hierarchy is:

| Role | Width | Purpose |
| --- | ---: | --- |
| Resting border | 1 px | Ordinary containment and inactive controls |
| Active border | 1.5 px | Hovered, selected, or enabled emphasis |
| Focus ring | 2 px | Keyboard focus and selected canvas-node halo |

Focus remains visible through both dedicated colour and the strongest weight.
Selection without focus uses the middle weight. Fixed-size JUCE chrome consumes
the values directly; scale-dependent domain geometry remains outside this
contract.

## Implementation Slice

1. Add a contract test for the exact ordered stroke hierarchy.
2. Add the three stroke roles to `CanvasChromeMetrics`.
3. Migrate generic canvas, palette, utility, workspace, Guide/Spy, node-shell,
   expanded-editor, selector, and compact-control outlines.
4. Migrate shared property-control focus framing while retaining the precise
   thumb, track, and indicator geometry owned by the property-control system.
5. Compare normal, selected, and focused production states at native scale;
   run focused interaction fixtures, Standalone Debug, and the full suite.

## Negative Boundaries

- Do not change bounds, radii, padding, hit targets, gestures, or focus routing.
- Do not flatten data and domain strokes into chrome metrics merely because
  they share a numerical value.
- Do not change icon path weights, cable widths, preview curves, graph grids,
  socket outlines, meter geometry, slider thumb borders, or mesh handles.
- Do not use colour alone for focus, selection, hover, enabled, or linked state.
- Do not add node-kind branches, adapters, renderer switchboards, or domain
  dependencies to shared presentation code.

## Verification

- Contract tests prove the shared scale is exact and ordered.
- Focused canvas, palette, keyboard, Guide, Spy, property-control, and expanded-
  editor tests remain green.
- Production fixtures exercise resting, selected, and keyboard-focused chrome.
- Native captures compare hierarchy, visual noise, focus strength, and state
  separation at the same window size and backing scale as the baseline.
- Standalone Debug, the full Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review pass before commit.

## Deletion Targets

- Local generic shell weights in node canvas, expanded node editors, utility
  surfaces, and Spy detail presentation.
- Local generic control-state weights in the node palette, workspace dock,
  Guide/Spy vacancies and refresh control, transform choice, Envelope morph
  controls, Trimesh axis controls, and Envelope purpose selector.
- The local property-control focus-frame weight.

## Completion Criteria

- Generic chrome consumes the shared resting, active, and focus widths, with no
  remaining local copies covered by the deletion targets.
- Selection and hover use less visual weight than keyboard focus.
- Domain/data strokes remain locally authoritative.
- Layout, hit geometry, interaction, state routing, and rendering ownership are
  unchanged.
- Focused tests, automation, screenshot review, standalone build, full suite,
  and style checks complete with no new regression.

## Implementation Evidence

- `CanvasChromeMetrics` now owns the exact 1, 1.5, and 2 pixel generic stroke
  roles alongside the shared corner scale. It remains a passive header with no
  drawing, state, layout, graph, or domain behavior.
- Generic node, utility, palette, workspace, Guide/Spy, editor-shell, selector,
  compact-control, and focus-frame outlines consume those roles. Local generic
  weights of 1.1, 1.2, 1.3, 1.4, 1.6, and 1.8 pixels are deleted from the
  migration targets.
- Hover and selection now use the 1.5-pixel active role. Keyboard focus uses a
  dedicated 2-pixel ring; the canvas's primary selected-node halo also retains
  that strongest authoring emphasis. The focused Guide tile combines a
  1.5-pixel domain-coloured outer border with a 2-pixel focus-coloured inset.
- Cables, Guide tethers, preview and transfer curves, EQ strokes and markers,
  mesh traces and handles, sockets, meter segments, slider tracks/thumbs and
  indicators, graph grids, and icon paths retain their existing authoritative
  weights. Scaled spectral-node outlines continue to scale their shared base
  weight with the established canvas factor.
- The focused metrics contract passes 14 assertions across two cases. Shared
  property controls pass 73 assertions, Guide presentation passes 56, and Probe
  presentation passes 330 assertions across 27 cases.
- The normal nine-command canvas fixture and eleven-command focused Guide-tile
  fixture pass with no failed command or filtered warning. Reports and native
  captures are `/private/tmp/cycle-v2-chrome-strokes-report.json`,
  `/private/tmp/cycle-v2-chrome-strokes.png`,
  `/private/tmp/cycle-v2-chrome-strokes-focus-report.json`, and
  `/private/tmp/cycle-v2-chrome-strokes-focus.png`.
- The full 30-command Guide keyboard sequence and 24-command Delay property-
  control sequence pass through real focus, activation, drag, fine adjustment,
  undo, and reset paths. Their reports are
  `/private/tmp/cycle-v2-chrome-strokes-keyboard-report.json` and
  `/private/tmp/cycle-v2-chrome-strokes-property-report.json`.
- Standalone Debug builds successfully with `--parallel 10`. The complete
  Cycle V2 executable runs 535 cases: 534 pass, and the sole failure remains
  the pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion already
  recorded in `docs/TDD/ui-bugs.md`.
- No DSP, analysis, rasterization algorithm, or hot loop changed. Production
  review found no new node-kind or parameter-ID branch, adapter, domain
  dependency, geometry owner, or duplicated rendering path.
