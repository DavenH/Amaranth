# Cycle V2 Canvas Chrome Typography

Status: Implemented (2026-08-28)

## Objective

Give Cycle V2 application chrome one restrained semantic type scale so compact
captions, ordinary controls, section headings, and editor titles have a clear
and repeatable hierarchy. Keep data, musical, and visualization typography
with the renderers that own its meaning.

## Current Failures

Equivalent generic text currently uses local sizes of 9, 9.4, 9.8, 10, 10.5,
10.6, 11, 11.2, 12, 14, 16, 17, 18, 19, and 20 pixels. These near-duplicates
do not communicate distinct roles:

- full editor titles use 17 or 18 pixels depending on node kind;
- ordinary control and navigation labels use 10.6, 11, 11.2, or 12 pixels;
- compact captions use 9, 9.4, 9.8, or 10 pixels; and
- canvas-node titles use 18, 19, or 20 pixels before applying the same zoom.

The production baseline is `/private/tmp/cycle-v2-chrome-strokes.png`, with
the focused state at `/private/tmp/cycle-v2-chrome-strokes-focus.png`.

## Authoritative Implementations And Boundaries

- Existing components and painters remain authoritative for strings, font
  style, colour, justification, bounds, truncation, interaction, and layout.
- `CanvasChromeMetrics` owns only shared generic visual measurements. It does
  not choose text content or draw text.
- Fixed-size JUCE chrome consumes the scale directly. Canvas-world chrome
  retains its established viewport zoom multiplication.
- Plot, data, musical, and visualization renderers remain authoritative for
  their own annotation sizes because those sizes depend on local density and
  legibility constraints.

The generic semantic scale is:

| Role | Size | Purpose |
| --- | ---: | --- |
| Micro text | 9 px | Space-constrained chrome such as palette categories |
| Caption text | 10.5 px | Secondary summaries and compact choice text |
| Label text | 12 px | Ordinary controls, values, navigation, and tile names |
| Section title | 14 px | Local panel headings and prominent status text |
| Editor title | 18 px | Full editors and canvas-node titles |

The ratios are intentionally modest. Hierarchy also comes from colour,
position, whitespace, and font style rather than large jumps in type size.

## Implementation Slice

1. Add a contract test for the exact ordered semantic type scale.
2. Add the five roles to `CanvasChromeMetrics`.
3. Migrate generic node-canvas, palette, status, workspace, Guide/Spy,
   property-control, transform, detail-header, expanded-editor, and full-editor
   text to the appropriate role.
4. Preserve established zoom multiplication for canvas-node titles and badges.
5. Compare canvas, Guide, IR, Voice Context, Delay, and Equalizer production
   states at native scale before completing the slice.

## Negative Boundaries

- Do not change strings, bounds, padding, control sizes, hit targets, gestures,
  focus routing, or component ownership.
- Do not centralize plot ticks, axis labels, landmark labels, key labels,
  waveform or transfer annotations, mesh data labels, or preview labels.
- Do not prescribe font family or weight in this slice. Existing purposeful
  emphasis such as the IR sample heading remains locally owned.
- Do not add a runtime look-and-feel object, node-kind switchboard, renderer
  adapter, or domain dependency to the metrics header.
- Do not reduce accessibility or force text into bounds that no longer fit.

## Verification

- Contract tests prove the shared scale is exact and strictly ordered.
- Focused property-control, Guide, Spy, canvas, and expanded-editor tests remain
  green.
- Production captures exercise the normal canvas, compact workspace chrome,
  and representative full editors at native scale.
- Standalone Debug, the full Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review pass before commit.

## Deletion Targets

- Local generic font sizes in node-canvas chrome and canvas-node titles.
- Local generic font sizes in shared property controls and compact transform
  controls, excluding slider landmark annotations.
- Local generic font sizes in workspace summaries, Guide/Spy headings, tiles,
  vacancies, and Probe detail headers.
- Local generic title sizes in full and expanded node editors.

## Completion Criteria

- Generic chrome covered by the deletion targets consumes the five semantic
  roles with no remaining local numerical copies.
- Equivalent titles and labels use equivalent sizes across node kinds.
- Domain-specific typography remains locally authoritative.
- Geometry, interaction, rendering ownership, and zoom behavior are unchanged.
- Focused tests, automation, screenshot review, standalone build, full suite,
  and style checks complete with no new regression.

## Implementation Evidence

- `CanvasChromeMetrics` now owns the exact 9, 10.5, 12, 14, and 18 pixel
  semantic type roles alongside the shared radius and stroke scales. It remains
  a passive header with no drawing, layout, state, graph, or domain behavior.
- Generic palette, canvas-node, workspace, Guide/Spy, Probe detail, property-
  control, transform, expanded-editor, and full-editor text consumes the scale.
  Voice Context now shares the 18-pixel full-editor title role; modulation-node
  primary text now shares the same zoomed canvas-title role as other nodes.
- Local axis, landmark, plot-tick, preview, musical, mesh-data, pitch, and probe-
  marker sizes remain with their authoritative renderers. No strings, bounds,
  hit targets, gestures, focus routing, font styles, or renderer ownership
  changed.
- The focused metrics contract passes 23 assertions across three cases. Shared
  property controls pass 73 assertions, Guide presentation passes 56, and Probe
  presentation passes 330 assertions across 27 cases.
- The normal canvas fixture passes nine commands. Delay and Equalizer editor
  fixtures pass seven and four commands, and the Voice Context attachment and
  interaction fixture passes all 52 commands. Their native captures are
  `/private/tmp/cycle-v2-typography-canvas.png`,
  `/private/tmp/cycle-v2-typography-delay.png`,
  `/private/tmp/cycle-v2-typography-eq.png`, and
  `/private/tmp/cycle-v2-typography-voice.png`.
- The IR resource fixture passes all five commands, including the `IR sample`
  semantic assertion. Its migrated bold heading retains the same 12-pixel
  rendering shown in `/private/tmp/cycle-v2-ir-hertz-readout.png`; the attempted
  fresh external capture could not resolve the application window rectangle.
- The complete 24-command Delay property-control sequence and 30-command Guide
  keyboard sequence pass. The Guide run emitted only the pre-existing
  intermittent CoreMIDI startup assertion already recorded in `ui-bugs.md`.
- Standalone Debug builds successfully with `--parallel 10`. The complete
  Cycle V2 executable runs 536 cases: 535 pass, and the sole failure remains
  the pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion already
  recorded in `ui-bugs.md`.
- `git diff --check` and line-length review pass. `clang-tidy` is unavailable.
  No DSP, analysis, rasterization algorithm, or hot loop changed; scalar math
  found in the touched files is pre-existing and outside modified lines.
