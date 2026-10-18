# Cycle V2 Trimesh spectral morph surface stability

Status: implemented (2026-10-18).

## Contract

`TrimeshGridRenderService` and the shared Cycle 1 `Panel3D` are the
authoritative grid sampler and expanded surface renderer. The expanded panel
uses its own pitch-aware column geometry for a key-scale primary axis. Its
rendered spectral rows must stay fixed while that primary morph slider moves;
the slider changes the selected slice, not the surface swept along that axis.
The expanded panel consumes the linear-frequency columns before screen
projection, preserving the sharper harmonic presentation seen in the
recording; the compact heatmap has its own row-resampled image.

`TrimeshWidget` previously used one `TrimeshPanelDataSource` for the 96-column
compact preview and the 768-column expanded panel. `NodePreviewResources`
resynchronizes the same widget for compact painting, so the compact request
replaced expanded panel data during drag and on gesture commit. The two column
counts produce visibly different row presentation. No mesh or audio DSP
interpolation should be changed to hide that ownership problem.

Keep the panel data source at expanded resolution once the expanded editor has
requested it. Render the compact image from the same model through
`TrimeshGridRenderService`, with its own 96-column cache. The boundary only
selects which render product each view consumes; it does not duplicate mesh
slicing, pitch mapping, or material evaluation. A primary morph movement may
rebuild its slice, while the unchanged expanded surface must not be rebuilt.

The expanded grid costs one render of each of its columns when first opened;
an unchanged primary-axis movement costs one slice. The compact image costs
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

## Completion

- [x] A focused test interleaves expanded paint, compact paint, two primary
      morph updates, and release-equivalent repaint, preserving the expanded
      spectral grid and pitch-spanning mode throughout. Its surface rebuild
      counter remains unchanged after the initial expanded render. Closing the
      editor returns the panel to the 96-column compact budget.
- [x] A native focused fixture confirms the expanded surface remains stable
      through drag and mouse-up while the slider and slice update.
- [x] Architecture audit, size review, style check, and focused build/test pass.

Evidence: `ctest --test-dir build/tests -R 'Compact Trimesh repaint|Primary Trimesh morph rebuilds|Compact and expanded Trimesh views' -V`
passed all three cases. The native
`scripts/fixtures/cycle-v2-agent-trimesh-spectral-morph-surface-stability.json`
passed all 25 commands, retaining 768 columns, 1699 rows, and pitch-spanning
mode before, during, and after the gesture. Its captured video is
`.context/trimesh-morph-fixed.mp4`; the cropped grid in settled drag and
release frames was visually stable. `clang-tidy` completed with no warnings
on changed lines, and `git diff --check` passed.

The [captured expanded editor](figures/cycle-v2-trimesh-spectral-morph-stable-editor.png)
shows the preserved spectral surface after the morph drag.
