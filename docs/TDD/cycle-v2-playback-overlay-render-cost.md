# Playback overlay rendering cost

Status: Complete

Voice cursor/active-note animation must reuse graph plans, sampled surface and
slice data, prepared palettes and GPU resources. Fixed UI work is bounded by
the eight published voices, independently of unrelated graph size. No graph
copy, compile, preview preparation, mesh sampling or palette conversion may be
triggered by a playback frame.

Measure the warmed Envelope, waveform Trimesh and harmonic Trimesh panels with
an isolated, fixed preset and two held notes, then note-off/retirement. Use the
existing canvas timing distributions and surface/slice/revision counters. Add
only observation of actual surface render calls and time at the shared drawing
boundary; instrumentation must not duplicate the rendering algorithm. Preserve
snapshot readback/upload counters so data caching cannot hide redraw work.

Authoritative implementations: Panel3D owns surface geometry/colour rendering;
GLSurfaceCache and OpenGLPanel3D own Cycle 1's baked-surface reuse. Adapt that
cache to the shared canvas without copying colour or resampling algorithms.
TrimeshPanelHosts owns GL context resources, teardown and panel bounds. Panel
invalidation continues to own bake requests. Dynamic overlays remain after the
cached surface. EnvelopeSurfacePanel already owns a prepared colour texture;
CurvePanelHost must support direct GL presentation for that panel so playback
does not read back and convert a full unchanged surface image each frame.

Delete the shared-canvas bypass of Trimesh's surface cache. Use the established
viewport/scissor coordinate mapping for shared framebuffer capture; preserve
Cycle 1's local-origin case. Keep the snapshot path for editable curve panels.
Validate actual display, zoom, resizing, material/domain changes, mesh changes,
context recreation and voice retirement. Verify fixed graph/surface/slice/upload
counts and compare same-build before/after frame distributions. Run focused
semantic tests, app checks, architecture/code-map and style checks and commit.


## Findings and implementation

Graph plans, sampled surface/slice data and prepared envelope textures were
already stable during MIDI. The warmed baseline still performed 29 Envelope
3D framebuffer readbacks/conversions, 47 waveform surface draws and 48 harmonic
surface draws during the observation windows. Trimesh's shared-canvas mode
explicitly bypassed Cycle 1's surface cache. More significantly, every render
profile assignment regenerated Guide tables and requested a full bake, even
when the domain was unchanged. A temporary stack trace identified
`setRenderProfile -> updateGuideCurveSeeds -> setLineGuideTables` as the owner;
the temporary trace was removed.

The bridge now regenerates visualization seeds/tables only for a changed
domain or newly prepared Guides. TrimeshPanelHosts supplies the mature
GLSurfaceCache to its existing renderer and sizes/releases it in that context's
lifecycle. The shared-canvas cache bypass is deleted. Cached frames also skip
spectral background/harmonic preparation, which the baked image already holds.
The existing full-residency framebuffer-read mapping moved from the curve host
to GLSurfaceCache; both consumers reuse it. Tests cover local and offset/Retina
viewports. No surface colour/resampling algorithm was copied.

Envelope 3D uses the existing prepared colour texture through CurvePanelHost's
direct GL mode; editable 2D curve panels keep their snapshot path. This removes
3D readback/conversion while drawing the actual voice overlay. The first direct
trial exposed the editor background covering the surface. The shared editor
paint hook now excludes each owner's GL panel regions, and an actual editor
paint regression checks transparent surface pixels plus opaque controls. Native
captures confirm visibility at normal/compact sizes and after context reload.

## Comparable measurements

Same macOS machine, standalone Debug, warmed `brass-section`, 1728x962, two held
notes 48/55; 12 paced 80 ms intervals per panel, followed by release retirement.
These are descriptive frame timings, not machine-independent thresholds.

| Panel | Before GL mean / p95 | After GL mean / p95 |
| --- | --- | --- |
| Envelope 3D | 4.88 / 16 ms | 3.44 / 8 ms |
| Waveform Trimesh | 1.36 / 2 ms | 0.42 / 0.5 ms |
| Harmonic Trimesh | 3.70 / 4 ms | 0.27 / 0.5 ms |

In the final 111-command ordinary workload and its 84-node variant (64 extra
unrelated offscreen nodes), all sampled data, palette/upload, Guide preparation,
background preparation, surface drawing, graph/undo, preview and audio-plan
copy counters stay fixed through playback and voice retirement. Overlay
coordinates continue changing. Envelope 3D performs zero snapshot readbacks.
Native captures show independent diamonds and no ghost cursors. Envelope's
remaining frame cost includes the existing editable 2D snapshot path and canvas
composition; this slice does not change that separate presentation contract.

The 28-command invalidation fixture proves zoom, resize, secondary morph edits,
context recreation and waveform/spectral domain changes refresh the baked image;
unchanged requested frames reuse it. Context recreation and zoom do not resample
the mesh. The existing 50-command Envelope verifier passes, including pitch's
bipolar palette, missing-tail coverage and per-voice clocks. Focused C++ checks:
2,004 assertions / 26 cases, plus 62 / six Guide cases and 101 / 16 library cases.
A broader static-node spectral case's 1-vs-3-column expectation reproduces in the
original baseline binary; evidence is recorded in ui-bugs.md. Debug/release and
test targets build; code-map, architecture and whitespace/style checks pass.
Artifacts: `.context/playback-render-cost-{before,after-verification,scaled-verification}*`,
`.context/playback-render-cost/`, `.context/playback-render-*-native.png` and
`.context/playback-render-cache-tests.log`.

## Size and responsibility review

CurvePanelInfrastructure shrinks by extracting read bounds and adds one stable
presentation-mode choice. Its 613 lines retain component/input, resource,
cache/snapshot and invalidation composition; shared GL pixel mapping lives in
GLSurfaceCache (199 lines). TrimeshPanelHosts is 432 lines and owns its context
resources/bounds; the bridge owns seed invalidation; TrimeshPanel3D (313 lines)
only delegates surface rendering and exposes atomic work counters.

TrimeshWidget grows 981 to 982 lines only to forward the existing slice counter.
Its model/interaction/rendering collaborators remain unchanged. The Envelope
editor grows 1,199 to 1,206 lines for a seven-line paint-region override: this
continues its layout/presentation responsibility and adds no rendering,
preparation, graph or lifecycle policy. All new cache/readback behavior is in
its existing focused collaborators. No new responsibility is hidden in these
large orchestration files. Rendering instrumentation is UI-only; no audio path,
clock, allocation, lock or graph publication was introduced.
