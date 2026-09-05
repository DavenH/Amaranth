# Cycle V2 Editor Header Geometry

Status: Implemented (2026-08-28)

## Objective

Give Cycle V2 editor headers two explicit, reusable geometry profiles so titles
and header actions align within each editor family. Preserve the denser embedded
header used by canvas curve and mesh editors instead of forcing it into the
larger full property-editor profile.

## Current Failures

The full Delay, Reverb, Equalizer, Unison, and Modulation editors independently
repeat nearly identical title and close-button literals. Voice Context uses the
same visual family but places its title and close button three pixels higher.
Title rectangles also extend beneath header actions, so sufficiently long or
localized titles can collide with Enabled and Close controls.

The embedded curve and Trimesh editors consistently use a smaller profile, but
own duplicate 34-pixel header and 22-pixel close geometry. Trimesh secondary
text extends beneath its close affordance.

Production baselines are:

- `/private/tmp/cycle-v2-typography-delay.png`;
- `/private/tmp/cycle-v2-typography-eq.png`; and
- `/private/tmp/cycle-v2-typography-voice.png`.

## Authoritative Implementations And Boundaries

- Each editor remains authoritative for its title, colours, paint order,
  controls, component ownership, close action, enabled command, content layout,
  and automation state.
- `EditorChromeLayout` owns only deterministic header rectangles. It does not
  paint, create components, bind commands, or branch on node kind.
- `CanvasChromeMetrics` owns the shared measurements consumed by that layout.
- Full property editors use a 44-pixel header. Embedded curve and mesh editors
  use a 34-pixel header to preserve content density and their established
  section-title hierarchy.

The full-editor profile is:

| Measurement | Value |
| --- | ---: |
| Header height | 44 px |
| Horizontal title inset | 18 px |
| Vertical title inset | 8 px |
| Close control | 28 px |
| Close right inset | 14 px |
| Enabled control | 88 × 24 px |
| Action gap | 12 px |

The embedded-editor profile is:

| Measurement | Value |
| --- | ---: |
| Header height | 34 px |
| Horizontal title inset | 13 px |
| Vertical title inset | 4 px |
| Close control | 22 px |
| Close right inset | 11 px |
| Title-to-close gap | 8 px |

Header controls and title bounds share the header centre line. Title bounds end
before the first action rather than continuing beneath it.

## Implementation Slice

1. Add exact geometry tests for full headers with and without Enabled, plus the
   embedded header profile.
2. Add the semantic measurements to `CanvasChromeMetrics` and implement pure
   rectangle layout functions in `EditorChromeLayout`.
3. Migrate Delay, Reverb, Equalizer, Unison, Voice Context, and Modulation full
   editor headers.
4. Migrate Curve and Trimesh embedded headers without changing their content
   bounds or interaction ownership.
5. Compare representative editors at native scale and exercise close, Enabled,
   keyboard, and constrained-width behavior through existing automation.

## Negative Boundaries

- Do not create a shared editor base class or shared painter.
- Do not move command binding, component ownership, editor content layout, or
  node-specific state into generic UI infrastructure.
- Do not change preview bounds, property rows, slider geometry, action labels,
  title strings, colours, typography, panel size, or expanded-editor content.
- Do not make Close or Enabled visually larger merely because their layout is
  centralized; visual footprint and hit bounds remain the same size.
- Do not merge the full and embedded profiles or add node-kind branches.

## Verification

- Contract tests assert every output rectangle at a representative width and
  prove title bounds do not overlap actions.
- Existing editor-host, property-control, expanded-editor, Guide, and keyboard
  tests remain green.
- Native screenshots compare Delay, Equalizer, Voice Context, IR/curve, and
  Trimesh headers at the same production size as their baselines.
- Standalone Debug, the full Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review pass before commit.

## Deletion Targets

- Repeated full-editor title rectangles and Close/Enabled bounds.
- Voice Context's local header height and vertically offset close geometry.
- Curve and Trimesh local header-height constants and close-bound calculations.
- Embedded title rectangles that continue beneath the close affordance.

## Completion Criteria

- All listed full editors consume one full-header layout contract.
- Curve and Trimesh consume one embedded-header layout contract.
- Equivalent controls share exact bounds and centre lines within their family.
- Title rectangles reserve action space at normal and constrained widths.
- Component ownership, commands, content geometry, interaction, and rendering
  remain locally authoritative.
- Tests, automation, screenshots, standalone build, full suite, and style checks
  complete with no new regression.

## Implementation Evidence

- `CanvasChromeMetrics` now owns both header measurement profiles, and the
  header-only `EditorChromeLayout` computes deterministic rectangles without
  painting, component ownership, commands, node kinds, or domain state.
- Delay, Reverb, Equalizer, Unison, Voice Context, and Modulation consume the
  full profile. Voice Context's title and close control now share the same
  22-pixel header centre line as the other full editors.
- Curve and Trimesh consume the embedded profile. Their 34-pixel header and
  22-pixel close footprint are unchanged, while title bounds now reserve an
  explicit eight-pixel gap before Close. Trimesh secondary text no longer
  extends underneath its close affordance.
- Exact layout tests pass 19 assertions across enabled, action-only, normal-
  width, and constrained-width headers. Effect-editor tests pass 48 assertions,
  Voice Context passes 43, Unison layout passes four, and expanded-editor
  routing passes six.
- Delay, Equalizer, Voice Context, and Trimesh production fixtures pass all 71
  commands. Native captures are `/private/tmp/cycle-v2-header-delay.png`,
  `/private/tmp/cycle-v2-header-eq.png`,
  `/private/tmp/cycle-v2-header-voice.png`, and
  `/private/tmp/cycle-v2-header-trimesh.png`.
- The IR resource fixture passes all five semantic commands. Its external
  screenshot helper again could not resolve a window rectangle; the shared
  Curve header is exercised by the Trimesh and expanded-editor tests, and IR
  ownership and control state remain unchanged.
- Standalone Debug builds successfully with `--parallel 10`. The complete
  Cycle V2 executable runs 538 cases: 537 pass, and the sole failure remains
  the pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion already
  recorded in `ui-bugs.md`.
- `git diff --check` and line-length review pass. `clang-tidy` is unavailable.
  No DSP, visualization algorithm, rasterization path, or hot loop changed.
