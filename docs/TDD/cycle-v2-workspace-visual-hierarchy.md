# Cycle V2 Workspace Visual Hierarchy

Status: Second hierarchy slice implemented; graph-probe relocation remains a
separate design decision

## Objective

Make the Cycle V2 canvas and expanded Trimesh editor read as one deliberate
workspace without hiding authored graph structure during ordinary use. Preserve
the three established information-bearing colour systems:

- yellow, red, and blue for the morph axes;
- cyan, butterscotch, and purple for signal domains;
- the established envelope-section colours.

Selection and persistent chrome must not compete with those semantics.

## Design Contract

The node graph is the primary ordinary workspace. The palette, performance
keyboard, minimap, compact signal legend, Curve Guide shelf, and signal probes
are secondary canvas utilities. An expanded editor whose module blocks the
canvas is a focused workspace state, not another peer utility.

This slice establishes these geometry rules at production size:

- the Trimesh editor is centred on the whole canvas and occupies 86% of its
  width and 82% of its height, subject to the existing 18-pixel safe margin;
- blocking editors suppress interactive canvas chrome and dim the graph behind
  the editor, so spy previews cannot paint across editor content;
- the Curve Guide shelf is narrower, retains image-first tiles, uses a
  directionally correct collapse control, and names creation as `New`;
- the four-entry signal legend is one low-emphasis horizontal row without an
  enclosing outline;
- selection uses one neutral outline and restrained halo. Morph, signal, and
  envelope colours remain semantic content rather than selection decoration;
- preview transport moves beside the keyboard keys without increasing their
  vertical budget.

## Ownership And Reuse

- `NodeViewModuleRegistry` remains authoritative for expanded-editor sizing.
- `NodeCanvasEditorCoordinator` remains authoritative for editor bounds and
  blocking policy.
- `NodeCanvasPresentation` remains authoritative for canvas-layer ordering and
  chrome suppression.
- `WorkspaceDock`, `GuideCurveShelf`, and `CanvasUtilityDock` remain the layout
  owners for their respective utilities.
- `PerformanceKeyboardPanel` continues to reuse `AmaranthMidiKeyboard` for key
  geometry and complete MIDI gestures. Only its containing controls move.

No graph, DSP, serialization, undo, or domain-colour behavior is copied or
changed.

## Deferred Decisions

- Whether signal probes should return to the graph as tethered preview nodes.
- Whether `Spies` should be renamed `Wiretaps`.
- Moving the probe refresh policy (`On Release` / `Live`) out of the dock and
  into an application menu; do not remove the only control before that owner
  exists.
- A source-to-sink flow composition for the default graph. This needs a visual
  mockup before changing stored node positions.
- Whether a second Voice Context justifies showing the Trimesh context port.
- Optional close/hide controls for utilities beyond the existing dock states.
- A dedicated output-node emphasis treatment.

## Follow-up Design Contract

The second slice corrects the production-size composition without changing the
graph model or the three domain-colour systems:

- the modal focus scrim dims only the canvas outside the expanded editor;
- opening and dismissing an overlay immediately invalidates both component and
  OpenGL presentation;
- the signal-probe rail, its label, and its collapse affordance do not exist
  visually or interactively until the graph contains a probe;
- probe refresh policy moves to a two-choice `File > Spy Refresh` menu;
- the minimap and Curve Guide shelf share a 210-pixel width;
- the compact legend sits at the lower-right edge of the primary graph area,
  immediately left of the guide shelf, rather than determining shelf height;
- keyboard playback progress is horizontal and centred beneath the keys. The
  keyboard group receives a perceptible surface contrast without a heavy
  outline.

`NodeCanvasPresentation` remains the authoritative layer-order owner,
`CanvasUtilityDock` owns utility geometry, `WorkspaceDock` owns guide/probe
geometry, `WorkspaceDockInteractionController` owns probe refresh settings,
and `MainWindow` only routes the menu choice through `NodeWorkspace`. No graph
mutation, DSP, serialization, or domain rendering is duplicated.

Follow-up size review:

| File | Baseline | After | Change |
| --- | ---: | ---: | ---: |
| `Main.cpp` | 380 | 437 | +57 |
| `CycleV2Automation.cpp` | 2,012 | 2,030 | +18 |
| `NodeCanvas.cpp` | 2,582 | 2,594 | +12 |
| `NodeCanvasAutomationInspector.cpp` | 1,109 | 1,109 | 0 |
| `NodeCanvasPresentation.cpp` | 1,481 | 1,486 | +5 |

The large-file additions stay within existing ownership: automation exposes
the two new menu choices without adding product policy, `NodeCanvas` reports
probe visibility and invalidates overlay transitions, and presentation applies
the established layer-order rule. `MainWindow` remains below the review
threshold and owns the actual File menu. No new kind switch, graph policy, or
cross-subsystem adapter was introduced, so an extraction would add indirection
without removing a responsibility from these owners.

## Architecture Review

Baseline and after sizes:

| File | Baseline | After | Change |
| --- | ---: | ---: | ---: |
| `NodeCanvas.cpp` | 2,572 | 2,582 | +10 |
| `NodeCanvasPresentation.cpp` | 1,462 | 1,481 | +19 |

Both files already exceed the architecture-audit plan threshold. The added
responsibilities remain cohesive with their existing owners:

- `NodeCanvas` coordinates component bounds and input precedence. It asks
  `NodeCanvasEditorCoordinator` for the authoritative blocking policy and gives
  a blocking editor precedence over dock and canvas input. It does not decide
  which node kinds block or reproduce editor geometry.
- `NodeCanvasPresentation` owns canvas layer order. It consumes the existing
  editor-occlusion fact to suppress utilities and dock layers and apply the
  focus scrim. It does not inspect node kinds or editor internals.

Stable collaborators are `NodeCanvasEditorCoordinator`, `WorkspaceDock`,
`CanvasUtilityDock`, and `NodeWorkspace`. Policy ownership remains singular:
editor blocking belongs to the coordinator/module registry; editor size belongs
to the module registry; canvas chrome order belongs to presentation; keyboard
visibility belongs to `NodeWorkspace`. There is no new lifecycle, transaction,
invalidation, or eligibility decision duplicated across callers. Extracting
these few call-site decisions would add a facade without deleting an old path,
so no extraction is justified for this slice.

Deletion targets are limited to the superseded vertical legend geometry and
top-mounted keyboard transport layout; both old paths were removed in place.

## Verification

- Geometry tests assert centred Trimesh proportions and non-overlap contracts.
- Presentation tests assert that blocking-editor chrome is suppressed.
- Keyboard tests assert that key proportions and all existing interaction
  targets survive the side-by-side transport layout.
- Capture the ordinary graph and expanded Trimesh editor at the same production
  size as the baseline screenshots.
- Run the focused UI tests, `git diff --check`, the Cycle V2 architecture audit,
  file-size review, and the standalone build.

## Completion Criteria

- The expanded Trimesh editor is centred, wider, shorter, and unobscured.
- Ordinary graph utilities remain available but visually secondary.
- The guide shelf is narrower and its create/collapse actions are legible.
- The signal legend no longer occupies a large outlined card.
- Selected nodes and preview tiles do not borrow domain colours or use a stark
  white focus rectangle.
- The production screenshots show no new clipping or illegible controls.

## Implementation Evidence

- The 2,048-by-1,152 canvas capture at
  `/private/tmp/cycle-v2-hierarchy-canvas.png` shows the 521-by-112 keyboard,
  unboxed horizontal legend, narrower Guide shelf, and lower-emphasis chrome.
- The matching Trimesh capture at
  `/private/tmp/cycle-v2-hierarchy-trimesh.png` shows an unobscured, centred
  editor with the surrounding canvas dimmed and all utility chrome suppressed.
- The requested graph-flow mockup keeps one visible canvas and organizes it
  into faint Voice Control, Voice Signal, and Global FX lanes. It was generated
  as a preview-only design reference and did not mutate stored graph positions.
- Focused geometry and interaction coverage passes 117 assertions in nine test
  cases.
- The standalone application and `CycleV2_tests` target build successfully.
- The full `CycleV2_tests` run passes 787 of 826 cases. Its 39 failures are in
  the already-open preset-fixture and audio/spectral groups; the focused UI
  suite passes, and this change does not touch DSP, graph serialization, or the
  user-modified preset files.
- `git diff --check` and the Cycle V2 architecture audit pass. `clang-tidy`
  could not be run because it is not installed in this environment.
- Follow-up captures at `/private/tmp/cycle-v2-hierarchy-followup-canvas.png`
  and `/private/tmp/cycle-v2-hierarchy-followup-trimesh.png` show the lower-right
  legend, matched minimap/sidebar widths, horizontal keyboard progress, hidden
  empty spy rail, and original Trimesh colour intensity inside the modal.
- The expanded-editor dismissal fixture verifies that the editor closes and
  the keyboard becomes visible after one 100 ms idle interval. The File-menu
  fixture verifies both persisted spy-refresh choices through automation.
- The follow-up focused suite passes 199 assertions in 18 cases, and both the
  test target and standalone application build successfully.
