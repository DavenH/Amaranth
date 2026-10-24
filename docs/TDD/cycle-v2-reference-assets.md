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
Actions use import-to-tray, waveform-plus, tray, plus, pencil, rename caret and
trash. LibraryIconRenderer parses the existing embedded SVG resource pipeline
once, with neutral resting/highlighted variants. No cyan is introduced.

Tabs keep their existing 46 px hit height and receive a centered 24 px icon
canvas. Action controls are 32x28 px with a centered 24 px canvas, in a compact
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
