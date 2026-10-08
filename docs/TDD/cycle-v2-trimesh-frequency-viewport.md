# Cycle V2 Trimesh frequency viewport parity

Status: implemented.

Cycle 1 `SpectrumInter2D/3D` owns frequency vertex limits `[-FreqMargin, 1 + FreqMargin]`, with `FreqMargin = 0.5`. Its spectral viewport limits are narrower (`[-0.05, 1.05]`). Cycle V2 currently inherits `[0, 1]` vertex limits and creates a `ZoomPanel` but mounts only its child input host in the expanded editor. Cycle V2 needs viewport limits matching the editable bounds so every allowed vertex can be reached.

The Cycle V2 Trimesh panel bridge owns domain selection. It will apply frequency bounds to both interactors for spectral magnitude and phase and restore ordinary bounds for time. The panel owns its zoom rectangle. The expanded editor will mount the existing `ZoomPanel`, while the shared OpenGL host will render only into the child viewport. The component tree remains on the message thread; render and input both use the same viewport dimensions. A scrollbar move changes the zoom rectangle and invalidates the panel through the existing `ZoomListener` path. No mesh copy or DSP preparation is needed on scroll.

Completion criteria:

- Spectral frequency vertices can be edited through `[-0.5, 1.5]` in both 2D and 3D; time returns to `[0, 1]`.
- The 3D viewport exposes horizontal and vertical scrollbars, with the surface, pointer targeting, and overlays using the child viewport size.
- A focused regression checks bounds, scrollbar visibility, viewport geometry, and pan behavior; a production-size screenshot is saved at `/private/tmp/trimesh-frequency-viewport.png` using `scripts/fixtures/cycle-v2-agent-trimesh-frequency-viewport.json`.
- The four relevant panel viewport, host, cursor, and box-selection tests pass. The broader Trimesh selection has unrelated model/guide failures recorded in `docs/TDD/ui-bugs.md`.

Review: `TrimeshPanelBridge` owns the spectral eligibility decision and supplies the same range to both interactors and panel viewports. `TrimeshPanelHosts` owns wrapper placement and child render bounds. `ZoomPanel` remains the sole scrollbar implementation. No graph/model copy or raster preparation is added to scrolling. The touched Cycle V2 files remain below the architecture size triggers; the audit reports 21 existing unrelated triggers. The initial 2D and 3D panel host composition is replaced by the existing zoom wrapper, with the input host retained as its child. No duplicated scrolling algorithm or temporary adapter remains.
