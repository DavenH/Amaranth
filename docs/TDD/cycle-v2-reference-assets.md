# Wave reference assets workflow

Status: Implemented (2026-10-23).

## Contract

Wave References has a persistent disclosure control below the canvas. Its session
visibility is explicit and independent of which node editor is open. Imports and
explicit reference additions reveal it immediately; browsing and auditioning an
asset do not. An empty tray offers a direct route to Assets and import.

Assets joins the existing sidebar tabs. It reuses LibrarySearchField,
SidebarLibraryToolbar and SidebarMediaRow. Audio files of at most four seconds
show one JUCE waveform thumbnail from the sum of the two channels, normalized
to the signed display range. The background job reuses PitchedSample decoding
and Buffer operations; audition audio remains unchanged. Rows support audition, keyboard selection,
explicit Add, double click and drag to the reference tray. Longer files remain
listed and usable without a waveform. Card titles retain names and direct play /
close controls; duration sublabels are removed.

## Owners and complexity

ReferenceLibrary retains catalog, import, analysis, pitch edits and active cards.
ReferenceAssetBrowser owns filtering, list selection and thumbnail presentation;
A bounded JUCE ThreadPool decodes the short samples; AudioThumbnail owns
waveform reduction and drawing. PitchedSample owns audio
decoding and StandaloneAudioEngine owns prepared audition playback. No DSP,
pitch tracking, graph commands or mesh gestures are duplicated. NodeWorkspace
owns tray disclosure and routes sidebar actions. Browser rows never select a
reference merely to audition it. Drag movement does not decode, analyze or edit
the graph; a successful drop uses the existing reference-selection boundary.

Delete the Trimesh-dependent visibility gate and duration card layout. Paint and
hit targets for each card action derive from the same geometry. Verify actual
clicks, independent disclosure, search, add/drop and audible output, alongside
focused reference tests and production-size captures.


## Icon and geometry contract

The five sidebar tabs use Curves (Bezier handles), Presets (stacked cards),
Patterns (piano roll), Nodes (connected blocks), and Assets (wave folder).
Reference actions use import-to-tray, waveform-plus, tray and plus.
Preset/pattern New/Edit/Rename/Delete retain their text buttons.
LibraryIconRenderer parses the existing embedded SVG resource pipeline
once, with neutral resting/highlighted variants. No cyan is introduced.

Tabs keep their existing 46 px hit height and receive a centered 24 px icon
canvas. Reference actions are 32x28 px with a centered 24 px canvas, in a compact
centered group with 8 px gaps. All SVGs use a 24-unit viewBox, a 1.7-unit primary
stroke and roughly 3-unit optical insets. The tray disclosure is 30 px tall;
its initial content height is 254 px, preserving the previous 284 px total.
Reference cards are 300 px wide; asset rows are 82 px high. Drag images contain
one row at native scale and stay above the pointer, rather than snapshotting
the list. Icons have accessible names and tooltips; the tray title remains a
section heading with a disclosure chevron, without a boxed text action.


## Completion and review

The production C++ diff adds 1002 lines and removes 86. New presentation owners
are ReferenceAssetBrowser (386 lines), ReferenceTrayBar (121),
LibraryIconRenderer (90), and ReferenceWaveformPreview (22). NodeCanvas grows
3613→3630 lines only for sidebar navigation and attachment. NodeWorkspace grows
1000→1021 for composition/routing and native-pointer diagnostics while deleting the deferred requested state
and timer/editor visibility checks. InlinePresetBrowser grows 798→835 for the
fifth tab and attached page; its preset indexing/editing owners remain stable.
ComparisonDock grows 706→755 for shared action geometry, drop routing and
file-specific audition feedback. These existing large coordinators retain
composition responsibilities; algorithms remain in the named collaborators.

The only new categories are the Assets UI tab and twelve icon identifiers.
There are no new DSP/node kinds, graph mutation paths or copied audio/pitch
algorithms. Imports keep the existing library boundary. Audio decoding remains
PitchedSample-owned; vector channel summation and presentation normalization
are isolated in ReferenceWaveformPreview. Drag motion uses JUCE and changes no
graph, analysis or audio preparation; one row snapshot is made at drag start.
The floating drag image stays above the canvas's native OpenGL view. Cached
waveforms repaint on decode completion and audition state changes, not idle
polls. No temporary compatibility path remains; the automation selection entry
point forwards to the same semantic reference action used by the UI.

Verification artifacts under `.context/` cover the standalone build, 32 focused
reference/browser/audio cases, all twelve SVGs, icon contact-sheet review,
explicit disclosure without an editor, unchanged visibility on editor open,
asset-only audition without tray activation, add-to-tray, real card clicks,
nonzero captured device audio, native search and drops onto both open/collapsed
trays, and unchanged preset revisions. The reference surface-selection fixture
and original empty-comparison fixture pass. Native checks also cover 1000x700
sidebar/playback and revealing Assets from an expanded editor. Icon passes used
the large/exact-size contact sheet, full-window screenshots and the narrow
native layout. Architecture audit, semantic map and whitespace checks pass.

The expanded Trimesh editor's crowded top pane at 1000x700 is recorded separately
in `ui-bugs.md`; reference browsing, disclosure and playback remain operable at
that size. The UI keeps neutral accents, including inherited scrollbars/hover
outlines. Source/synth material colours retain their domain visualization.

## Follow-up: stable cards and expanded pitch view

Status: Implemented (2026-10-24).

Restore the preset/pattern New/Edit/Rename/Delete row to its original text
controls. Asset actions and sidebar tabs keep their semantic icons. Preserve
library insertion order in the tray: selecting or playing a reference changes
selection, never placement.

Double-clicking a reference surface or title opens a canvas editor with that
reference's cyclogram above its pitch envelope. The rail remains visible and
keeps its order. ReferencePitchEditor composes the existing ComparisonSurfaceView,
CurvePanelHost, PropertySliderRow and ExpandedEditorChrome; no second envelope
interaction, pitch codec or surface renderer is permitted. NodeCanvas owns only
attachment, standard editor bounds, input dismissal and occlusion. ReferenceLibrary
retains pitch edits and undo. Delete the dock-wide envelope replacement and the
selected-card rotation. Live pitch movement remains local to the curve panel;
release updates the selected reference and its cached cyclogram once. The
cyclogram and envelope share horizontal bounds, separated by a compact pitch
heading/fundamental row. Escape or the standard close control dismisses the
expanded view. Verify real surface double-click routing, stable positions,
layout geometry, pitch commit/undo and tray preservation.

The empty pitch view also sampled time 0.5 despite EnvelopeMeshState placing
reference cubes in the static time-zero slice. The adapter now supplies time
zero, preserving the existing envelope rasterizer and gestures. Cmd-Z accepts
the native uppercase key code as well as the lowercase automation form.
Remove the four superseded New/Edit/Rename/Delete SVG resources and identifiers.

Review: production changes add 185 lines and remove 87. ReferencePitchEditor
grows 290→342 lines by composing the existing surface view and chrome.
ComparisonDock stays 755→756 while deleting rotation and dock replacement.
NodeWorkspace shrinks 1021→1018 and only translates automation coordinates.
NodeCanvas grows 3630→3674 (header 484→487): attachment, standard bounds,
dismissal and keyboard/occlusion routing remain its cohesive responsibilities.
NodeCanvasEditorCoordinator still owns editor geometry, ReferencePitchEditor
owns the reference layout, and ReferenceLibrary owns edits/history. The canvas
holds a non-owning SafePointer; the dock retains lifetime and GL teardown.
No graph/DSP kinds, mesh gesture implementations or analysis paths are added.
The cyclogram cache is keyed by sample identity and analysis revision; live
curve motion does not copy its grid, publish a document, or rebuild the graph.

The standalone build, 31 reference/browser cases, three fixtures
(`reference-expanded`, `reference-surface-click`, `reference-assets`), architecture
audit, semantic map and whitespace checks pass. Native verification with
`python3 scripts/test_cycle_v2_reference_editor.py --no-build` passes card
selection/placement, double-click, aligned cyclogram/envelope bounds, two live
drag updates without analysis publication, one release revision, changed derived
pitch frames, exact Cmd-Z restoration, close and Escape. Document revisions
and dirty state remain unchanged. The script uses a scoped instance and deletes
only its own UUID import, leaving live user presets/reference metadata alone.
Native replay uses the existing native-edit smoke's two-click sequence and
80 ms keyboard settling; earlier fast click/shortcut attempts missed delivery.
No application assertions or crashes occurred. Evidence is in
`.context/reference-editor-native/`, with full-window captures at 1280x900 and
1000x760, and `.context/final-reference-*-report.json`.

Expanded reference pitch envelopes fit vertically when opened, retargeted or
restored from history. ReferencePitchEditor prepares the static curve slice
before calling the existing fitEnvelopeVerticalRange; fitting never runs during
live pointer movement. The mature zoom panel retains its minimum range/padding
for nearly flat curves. The expanded-reference fixture asserts the resulting
vertical range is smaller than the full domain. Standalone build, fixture,
architecture audit and whitespace checks pass; the sax-growl capture at
`.context/reference-pitch-fit.png` confirms the fitted view without document edits.
The editor remains 343 lines with unchanged ownership and algorithms.

The expanded pitch envelope mounts its existing CurvePanelHost zoom wrapper,
exposing only the vertical bar. ZoomPanel retains range, pan, wheel and clamping
behavior; AmaranthLookAndFeel supplies the neutral track/thumb. ReferencePitchEditor
listens to scrollbar range changes only to request a fresh GL snapshot, including
when the shared wheel handler changes scale. The drawing/input host remains the
wrapper's child, with automation bounds translated from that child. The cyclogram
shares its content width, leaving the bar outside the aligned drawing areas.
The existing automatic fit still runs on open/history restoration, never during
pan or a live edit. There is no new zoom algorithm, document edit or analysis work.

Tray card input forwards wheel events to the existing JUCE Viewport, including
smooth horizontal trackpad events and the reverse direction. Headers and gaps
retain their normal viewport bubbling. The expanded reference surface has no
tray wheel callback and retains its own navigation. PanelInputHostComponent's
wheel override is protected so the read-only host can delegate its fallback;
the shared axis-lock/zoom implementation is unchanged. No scrolling mechanics
are copied. Scrollbar redraw requests are queued after ZoomPanel applies the
range, so GL reads the updated viewport.

Review: these two additions total 54 production lines added and 9 removed.
ReferencePitchEditor grows 343→370 lines and ComparisonDock 756→761, retaining
their composition roles and shared owners. The standalone build, two shared
trackpad tests, expanded-reference fixture, new reference-tray-scroll fixture,
architecture audit, semantic map and whitespace checks pass. The tray fixture
checks scroll positions 0→134→0→134 across card surfaces and headers while
preserving selection/order. The focused zoom check moves the vertical range
from 0.4771 to 0.5297 with unchanged span, analysis revision and document state.
Evidence: `.context/reference-tray-scroll-report.json`,
`.context/reference-pitch-zoom-check/` (including the full-window capture), and
`.context/reference-zoom-scroll-tests.log`. Broader native replay still has the
previously recorded input-delivery limitation; focused semantic gestures pass.

Space in the expanded reference toggles that reference's audition. The editor
emits an audition action and ComparisonDock calls its existing toggleAudition;
decoding, playback, stop behavior and card feedback keep their existing owners.
The canvas's expanded-editor key route takes precedence over preset transport.
The six production lines add no new state, audio path or policy owner. The
standalone build, space-audition fixture, architecture audit, semantic map and
whitespace checks pass. Native Space checks pass with both panel and pitch-curve
focus, including nonzero captured audio, stop, unchanged preset transport and
clean document state. Evidence: `.context/reference-space-report.json` and
`.context/reference-space-native/`.
