# Cycle V2 Trimesh spectral morph surface stability

Status: in progress (2026-10-18). A new user recording still shows alternating
stepped and smooth rows at an unchanged Red slider position, including on
release. The follow-up now selects one expanded renderer for spectral magnitude
and compares held and released captures. Native drag acceptance is still
pending because `cliclick` lacks Accessibility permission in this environment.

## Contract

`TrimeshGridRenderService` and the shared Cycle 1 `Panel3D` are the
authoritative grid sampler and expanded surface renderer. The expanded panel
uses its own pitch-aware column geometry for a key-scale primary axis. Its
rendered spectral rows must stay fixed while that primary morph slider moves;
the slider changes the selected slice, not the surface swept along that axis.
The expanded panel has two rendering paths: the scalar material renderer and
the older `Panel3D` column renderer. The scalar path shows discrete harmonic
rows. The older path interpolated vertex colours vertically and blurred one
harmonic into the next. The compact heatmap has its own image renderer.

`TrimeshWidget` previously used one `TrimeshPanelDataSource` for the 96-column
compact preview and the 768-column expanded panel. `NodePreviewResources`
resynchronizes the same widget for compact painting, so the compact request
replaced expanded panel data during drag and on gesture commit. That ownership
repair remains useful but did not address the recorded interpolation flicker.

The recording uses `acid-stab-3`. That preset opens with Yellow as the primary
axis and Red supplied by key scale. Moving Red rebuilds the surface as well
as the slice.
The first fixture changed the primary axis to Red, so it tested a slice-only
edit instead of the reported movement. With the actual preset, forcing the
older rendering path reproduces the blurred appearance in the recording;
forcing the scalar path reproduces the stepped appearance.

Keep the panel data source at expanded resolution once the expanded editor has
requested it. Render the compact image from the same model through
`TrimeshGridRenderService`, with its own 96-column cache. The boundary only
selects which render product each view consumes; it does not duplicate mesh
slicing, pitch mapping, or material evaluation. A primary morph movement may
rebuild its slice, while the unchanged expanded surface must not be rebuilt.

`Panel3D` remains the authoritative expanded renderer. Its magnitude fallback
must use constant colour across each row interval while retaining colour
interpolation between columns. A narrow virtual choice in `TrimeshPanel3D`
selects this behavior for spectral magnitude; no mesh sampling or material
policy is copied into the editor. The old vertically interpolated magnitude
fallback is the deletion target. Other domains retain their existing row
interpolation.

The later recording at `2026-10-18 4.34.58 PM` shows that matching the two
renderers in one settled frame did not keep them visually identical through a
native drag. `TrimeshPanel3D` now declines the scalar surface renderer for
spectral magnitude, so `Panel3D` uses its stepped column path on every redraw.
Time, phase, and other domain materials still use the scalar renderer. This
removes the magnitude renderer choice at the panel boundary rather than trying
to tune two independent interpolation policies to look alike.

The local editor used six decimal places for a dragged morph value while the
graph command used three. At mouse-up, binding the committed node changed a
value such as `0.798996` to `0.799`, causing an otherwise unnecessary surface
rebuild. `formatTrimeshMorphValue` is the shared representation boundary for
both sides. Release at an unchanged pointer position now keeps the surface
rebuild count constant.

The expanded grid costs one render of each of its columns when first opened;
a secondary-axis movement rebuilds the local surface. The compact image costs
one render of 96 columns only when its own revision, pitch, style, or bounds
change. Neither view copies the mesh or prepares durable resources during a
movement. The compact cache may hold an image, but no second long-lived mesh
or grid copy. Closing the expanded editor restores the shared panel's compact
96-column budget on its next sync.

Size review: `TrimeshWidget.cpp` was 855 lines before this repair and is over
the 800-line review trigger. It owns compact and expanded view composition,
hit geometry, and control routing. Its stable collaborators are
`TrimeshPanelBridge` for model and panel lifecycle,
`TrimeshGridRenderService` for grid sampling, and
`TrimeshSurfaceRenderer` for compact material drawing. The widget decides
which view owns a render product; the bridge alone decides panel invalidation
and the grid service alone decides spectral samples. No second lifecycle or
eligibility decision was added in graph or runtime code. The old compact
`bridge.syncFromNode(..., 96)` path is the deletion target; compact painting
must no longer replace the expanded panel's grid.

Current size review: `Panel3D.cpp` grew from 1,069 to 1,117 lines and owns
expanded panel surface rendering, overlay drawing, and GL presentation. Its
stable collaborators remain `PanelRenderer`, `ColorGradient`, and the panel
data retriever. The new row policy is selected by `TrimeshPanel3D`; `Panel3D`
only applies that policy during the existing column drawing pass. No graph,
runtime, or editor layer decides the row interpolation. The source remains
below the 1,200-line extraction-plan trigger. `NodeEditorCommandService.cpp`
is 849 lines; it still routes editor commands and presentation, while the
value representation is owned by the small Trimesh model helper.

## Completion

- [x] A focused test interleaves expanded paint, compact paint, two primary
      morph updates, and release-equivalent repaint, preserving the expanded
      spectral grid and pitch-spanning mode throughout. Its surface rebuild
      counter remains unchanged after the initial expanded render. Closing the
      editor returns the panel to the 96-column compact budget.
- [x] The focused `acid-stab-3` fixture drives Red with Yellow primary, checks
      the surface during drag, and verifies no surface rebuild on release at
      the same value.
- [x] A forced fallback capture shows discrete rows like the scalar path,
      instead of the old blurred rows.
- [x] Architecture audit, size review, style check, and focused build/test pass.
- [x] The expanded magnitude panel selects only the stepped column renderer;
      the focused test protects this boundary, and held versus released OS
      captures have the same sampled spectral column.
- [ ] Confirm the native macOS Red slider drag no longer changes row style at
      an unchanged position. The environment's `cliclick` reports missing
      Accessibility permission, so the in-app fixture cannot establish this.

Previous evidence used `organ.cyclegraph` with Red as the primary axis. It
does not establish that `acid-stab-3` is fixed. A direct comparison in
`.context/acid-repro.mp4` and `.context/acid-legacy.mp4` confirmed the two
rendering appearances. `python3 scripts/test_cycle_v2_trimesh_morph_surface.py`
passed with the actual preset, one drag at each of two Red values, and release
at an unchanged position. It checks that the secondary-axis drag changes the
surface, that release does not rebuild it, and that the forced fallback has no
extra vertical colour transitions relative to the scalar path. The old
fallback had 47 vertical transitions in the sampled strip versus 23 for the
scalar path; the final repaired capture had 19 versus 26. The resulting videos are
in `.context/trimesh-morph-regression/`. Focused `ctest` passed all three
Trimesh cases. `git diff --check` and the architecture audit passed; clang-tidy
reported existing `Panel3D` warnings and no new warnings on changed lines.

The [captured expanded editor](figures/cycle-v2-trimesh-spectral-morph-stable-editor.png)
shows the preserved spectral surface after the morph drag.

The follow-up `python3 scripts/test_cycle_v2_trimesh_morph_surface.py` run
compared the held and released `acid-stab-3` frames at the same Red value: the
sampled spectral column had a mean grayscale difference below 0.1, 16 discrete
transitions, and no surface rebuild on mouse-up. The three focused CTest cases
passed, including the renderer-selection test. These synthetic pointer and OS
capture checks do not replace native pointer delivery.
