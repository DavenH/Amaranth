# Cycle V2 Trimesh spectral morph surface stability

Status: implemented (2026-10-18). A later recording exposed alternating
stepped and smooth rows at an unchanged Red slider position. The panel now
selects one stepped renderer for spectral magnitude; the user reports that this
stopped the expanded-editor flicker. A follow-up visual check covers the lowest
harmonic width. The later compact-preview move regression is covered by a
separate two-update node drag and release fixture.

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

The stepped path inherited the legacy bottom fill of 15 pixels. A midpoint
cell made the lowest harmonic visible, but the user's visual comparison showed
that it remained shorter than the second band. `TrimeshPanel2D` is authoritative
for harmonic bin placement: bin `i` extends from log position `i` to position
`i + 1`, and its bar uses a small gap inside that interval. `Panel3D` should
use those same intervals without gaps for a connected surface. The final log
position closes the last interval; it does not create an extra band. This
changes only GL column geometry and colour lookup count. It keeps mesh samples,
harmonic positions, row colours, and gesture cost intact.

The node canvas painted cached node tiles at rest but directly repainted every
node during a move. That exposed a differently sampled compact heatmap and did
unnecessary preview work. `NodeCanvasNodeLayerCache` now treats position alone
as a translation of the existing tile. During a move, it checks constant-size
facts (model identity, tile size, selection, display scale, and runtime-preview
revision) and returns the same image. It does not traverse node parameters or
editor state, sample the mesh, evaluate the material, or recalculate guide
configuration for a cache hit. A missing or changed tile falls back to the
normal complete key, and release also uses that full key. The cache owns this
eligibility rule; `NodeCanvasPresentation` only chooses move access while a
move gesture is active. Both held and released compact previews therefore use
the same discrete row pixels from one tile.

The old cache searched a vector of all entries for each node. An ID-keyed map
keeps the move lookup independent of unrelated node count. The canvas still
iterates visible nodes to composite a frame and draws cables separately, but a
position delta does not create node render products. The two-update focused
fixture observed 19 tile hits and zero misses during movement, then zero
misses on release. Undo restored the original node position.

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

Current size review: `Panel3D.cpp` grew from 1,069 to 1,118 lines and owns
expanded panel surface rendering, overlay drawing, and GL presentation. Its
stable collaborators remain `PanelRenderer`, `ColorGradient`, and the panel
data retriever. The new row policy is selected by `TrimeshPanel3D`; `Panel3D`
only applies that policy during the existing column drawing pass. No graph,
runtime, or editor layer decides the row interpolation. The source remains
below the 1,200-line extraction-plan trigger. `NodeEditorCommandService.cpp`
is 849 lines; it still routes editor commands and presentation, while the
value representation is owned by the small Trimesh model helper.

`NodeCanvasPresentation.cpp` is 1,511 lines after this change, down eight lines
from its baseline. It owns canvas layer order, node chrome composition, and
preview routing; its stable collaborators remain `NodeCanvasScene`,
`NodePreviewRenderer`, and the two layer caches. Cache matching and lifetime
stay in `NodeCanvasNodeLayerCache`, so no second cache policy was added to the
presentation file. The existing extraction direction in
`docs/TDD/refactors.md` remains appropriate for its separate chrome painters.

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
- [x] The user reports that the native macOS Red slider drag no longer changes
      row style at an unchanged position. The local `cliclick` process still
      lacks Accessibility permission, so the agent fixture provides separate
      synthetic gesture evidence.
- [x] The lowest authored harmonic occupies its full first log interval and is
      visibly taller than the second band, matching 2D bin-width ordering.
      The rows remain discrete through drag and release.
- [x] A compact spectral Trimesh preview retains the same row appearance while
      its node is moved and after release at the same pointer position.

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

The midpoint-cell visual regression measured 66 orange pixels for the lowest
row in the 469-pixel panel; the same colour classifier measured 72 pixels for
the second band. That contradicted the monotonic bin spacing visible in 2D.
With the 2D interval mapping, the focused OS capture measured 88 and 63 pixels
for the first and second bands. The visual fixture now asserts that the first
band is at least 1.2 times the second, and held and released frames match with
no surface rebuild on mouse-up. Three focused CTest cases passed, and the
architecture audit and `git diff --check` passed. The measured height ratio
of 1.40 closely follows the 2D log spacing shown in the updated figure. The
updated assertion rejects the saved midpoint-cell baseline at 66 versus 72.

`python3 scripts/test_cycle_v2_trimesh_preview_node_move.py` drives two node
positions in `acid-stab-3`, checks tile-cache hit/miss telemetry, compares OS
video crops at held and released positions, verifies a strong row boundary in
each crop, commits the move, and undoes it. The held/released crop differed by
0.264 mean RGB levels (out of 255); the discrete boundary measured 64 and 63
levels respectively. The earlier direct-paint version changed by more than
0.5 mean levels and showed no boundary above 11 levels at that column. A
focused cache unit test covers two translations, commit-equivalent lookup,
undo-equivalent lookup, and a real context-key invalidation.

The [captured compact preview after the node move](figures/cycle-v2-trimesh-spectral-preview-node-move.png)
shows the same stepped bands as the held frame.
