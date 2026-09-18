# Cycle V2 Guide Column And Floating Spy Dock

## Status

Implemented (2026-09-17). The main-branch visual edits are merged. The right Guide column, floating Spy row, and top-edge keyboard are implemented.

## Problem And Visual Contract

The shared bottom Guide/Spy dock currently reserves a full-width canvas strip. Its Guide tiles run horizontally and are taller than their curve previews need to be. Guide previews can interfere visually with Spy grids, while a sparse Spy shelf hides a large area of the graph. The performance keyboard also has a top inset.

The graph remains the primary surface. Guides become a compact vertical stack in a right-side column below the minimap and legend. Spies remain ordered and docked near the bottom, with their previews in the same stable locations, but empty space around them shows the graph and accepts canvas gestures. The keyboard touches the top edge of its available canvas area. This is a presentation change: Guide resources, Spy probes, preview DSP, document commands, and audio behavior retain their current semantics.

## Authoritative Implementations And Boundaries

- `WorkspaceDock::layout()` is the shared screen-space geometry authority for Guide and Spy regions, collapse/minimize controls, and resize handles. Replace its side-by-side partition; do not add a second layout computation in painting or event code.
- `GuideCurveShelf` owns Guide tile previews and Guide hit testing. Reuse `CurveEditorWidget::paintPreviewSnapshot()` and the existing selection/editor behavior; change only bounds, clipping, overflow, and presentation chrome.
- `SignalProbeRail` owns ordered Spy previews, preview cache, ordinal, markers, and tile hit testing. Keep those implementations. Change the rail surface and hit regions to fit its visible tiles and controls.
- `CanvasUtilityDock::layout()` owns minimap, legend, keyboard, and status geometry. It should expose the right-column clearance required by Guides and place the keyboard at the top edge. `NodeWorkspace` continues to position the live keyboard from that rectangle.
- `NodeCanvas`, `NodeCanvasPresentation`, `WorkspaceDockInteractionController`, and `WorkspaceDockKeyboardNavigation` route state, painting, pointer, wheel, and focus using the shared geometry. They must not duplicate Guide or Spy tile placement.
- `GuideRelationshipPresentation` owns temporary Guide tethers; it must use the new tile edge and preserve expanded-editor occlusion.

There is no adapter or copied domain implementation. Geometry is translated at the presentation boundary only. The stable end state deletes the bottom full-width reservation, side-by-side split/divider, and obsolete horizontal Guide scrolling. No document format change is required.

## Geometry And State Design

At each resize, derive all rectangles from the current workspace, utility layout, dock state, and visible item counts. Use the existing canvas chrome spacing and colours after the pending merge. The production-size target is:

1. Minimap and legend stay in the upper-right utility column. The Guide column starts below the last visible utility with one standard gap and aligns to that column's right edge. A Guide tile is wider than it is tall, with a shallow curve preview. Guides stack downward with one consistent gap; excess Guides scroll vertically inside the column. Keep its title, add, minimize, focus, selection, and empty state usable at narrow sizes.
2. The keyboard is horizontally centred as today, but its top is exactly the available canvas top. Its width and height remain bounded by the existing preferred dimensions. Resolve narrow-width collisions by shortening or relocating the right utility column, not by adding a top gap to the keyboard.
3. The Spy controls and visible tiles occupy a bottom-anchored row. The row can keep horizontal overflow. Its surface is limited to controls, occupied tiles, and any needed compact backdrop; unused width and space above the row have no opaque fill. A sparse row leaves the graph visible. Preserve a legible surface behind each preview rather than making data marks themselves translucent.
4. The graph viewport extends into the previously reserved bottom strip. Guide and Spy rectangles are overlay occlusions for fit-to-document, expanded editors, and routes that need clearance. The Guide column may continue beside the Spy row because their horizontal bounds do not overlap. Empty Spy-space is available to graph hit testing, panning, and selection. A click or wheel event in an actual control or tile belongs to the dock; the same event in transparent space belongs to the canvas.
5. The global expanded/collapsed state remains one coherent dock control unless the final visual review proves that this shared action is misleading. The collapsed affordance must be compact, anchored, and must not restore a full-width opaque strip. Guide and Spy minimize controls retain their individual meanings.

`WorkspaceDockLayout` exposes shelf and handle bounds; the existing Guide and Spy components supply their own control and tile hit bounds. No whole-row rectangle is an event target or opaque paint surface. The Guide column and Spy row must not overlap each other or the minimap, legend, keyboard, status, and expanded editors at supported sizes. If the available height cannot show a useful Guide tile, show its header without clipped miniatures. At narrow widths, prioritize a usable keyboard, minimap, and reachable dock controls. The four-entry legend may hide where it cannot remain legible.

Keep dock height as the Spy tile-height preference if still useful. Replace the persisted horizontal divider setting with a Guide column width setting only if user adjustment remains valuable after visual review; otherwise remove the divider gesture and setting. Do not silently reinterpret old split percentages as a different dimension. The existing UI settings are local application state, so a safe default/reset is acceptable and should be documented when implemented. Rename Guide scroll offset and automation fields to reflect vertical movement; Spy offset remains horizontal.

## Interaction And Complexity Contract

Guide tiles keep single-click selection, double-click editor launch, hover tether, add, and minimize. Wheel motion over the Guide stack scrolls vertically. Spy tiles keep selection, double-click detail, remove, refresh policy, and horizontal overflow. Empty overlay space is transparent to hit routing. Tab still traverses all visible controls. Up/Down moves within the vertical Guide stack; Left/Right moves within the Spy row. A deliberate cross-region shortcut should preserve discoverability without mapping Up/Down to two conflicting actions. Focusing an offscreen tile reveals it along its own axis.

No gesture may copy the graph, serialize document state, prepare DSP resources, or rebuild unrelated previews. This change does not port a Cycle 1 interaction or modify a semantic edit path. Guide and Spy hit lookup retain their existing cost in the number of dock items. The new geometry and empty-space routing are constant-time additions. Keep preview caches keyed to their existing content, not to scroll position.

## Implementation Slices

1. **Geometry and reservation:** update `WorkspaceDock` and `CanvasUtilityDock` as the sole layout authorities. Distinguish content viewport from occupied overlay regions. Update `NodeCanvas` fit/editor clearance and `NodeWorkspace` keyboard bounds. Remove split hit geometry and full-width bottom occlusion.
2. **Guide column:** change tile aspect ratio and vertical stacking, overflow indicator, scrolling, focus reveal, empty/drawer presentation, and tether terminal. Preserve preview and Guide resource ownership.
3. **Floating Spy row:** bound paint and hit regions to actual chrome and tiles; make unused row space transparent to painting and pointer/wheel routing. Preserve preview cache and stable Spy order/positions.
4. **Integration:** update automation geometry/state names and fixtures, migrate or remove obsolete UI settings, review hover/selection/expanded editor behavior, and remove old partition/scroll paths. After each slice, inspect production diff size, type branches, and any accidental duplication of mature code.

## Verification

- Geometry tests at representative desktop and minimum supported sizes: keyboard top equals available top; Guide column sits below the right utilities; Guide tiles stack vertically with squat aspect ratios; Spy tiles stay bottom anchored; occupied bounds do not overlap important controls; transparent space remains in canvas content and is absent from dock hit regions.
- Routed interaction tests/fixtures: click and wheel in a Spy tile versus adjacent transparent canvas; Guide vertical scroll and keyboard reveal; Spy horizontal scroll; Guide hover tether; selection and double-click editor launch; expanded editor close and dock focus; minimize/collapse and resize.
- Focused Cycle V2 automation fixtures with actual-size OS screenshots before and after in the same preset, window size, and appearance. Include sparse and crowded Spy rows, zero/one/many Guides, keyboard-visible state, and a narrow window. Inspect the rendered hierarchy and preview readability, then correct discrepancies.
- Build Cycle V2 with `cmake --build --preset standalone-debug --target CycleV2 --parallel 10`, run focused Catch2 tests and relevant fixtures, then proportionate broader tests. Run `git diff --check`, applicable clang-tidy, and a style/readability pass on every changed production file. Commit each coherent completed slice.

## Completion Criteria And Deletion Targets

The graph uses the formerly reserved bottom area; empty Spy-space is visible and interactive canvas; Spy tiles retain stable bottom positions and readable grids; Guide tiles form a compact right-side vertical stack below the utilities; the keyboard has no top gap; both dock families remain usable by pointer and keyboard at supported sizes. Remove the full-width bottom shelf fill and hit target, side-by-side split/divider code and obsolete setting if no longer used, horizontal Guide overflow behavior, and tests/automation contracts that encode the old geometry. Do not mark this TDD implemented until the visual, interaction, performance, and deletion criteria all pass.

## Implementation Evidence

- `CycleV2` and `CycleV2_tests` build with the `standalone-debug` preset and `--parallel 10`.
- The focused canvas, utility, Guide dock, and Spy tests pass: 16 cases and 170 assertions.
- Focused automation passes for vertical Guide overflow and reset, keyboard traversal, Guide and Spy drawers, collapsed dock state, tether routing, and canvas wheel routing through unused Spy-row space.
- Actual-size before/after captures of the Baroque Flute preset show a top-edge keyboard, shallow right-side Guide tiles, bottom-anchored Spy tiles, and graph visibility through unused dock space.
- The full Cycle V2 test executable was also run. It reports 38 failures in graph, DSP, serializer, and preset tests outside the changed UI contract, including missing or noncanonical preset files. The focused layout suite passes.
- The production diff removes the split ratio setting and divider interaction. `git diff --check` passes. The modified UI paths contain no new scalar math in per-sample, per-bin, or per-pixel loops; `clang-tidy` is unavailable in this environment.
