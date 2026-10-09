# Cycle V2 Canvas Spies

## Status

Implemented. This supersedes the rail presentation and interaction sections of
`cycle-v2-spy-rail.md`; its passive signal capture contract remains in force.

## Design

`SignalProbe` remains graph metadata and the existing graph compiler, executor,
preview renderer, cable marker, and detail view remain authoritative. A Spy card
renders the same preview at a world position on the node canvas. An ordinary
Spy card can be dragged like a node; dragging changes only its position and
produces one undoable graph delta on release. The cable marker still reattaches the
probe to another cable. The tether joins the card to that cable marker. The
right-click cable command remains `Spy on Signal` / `Stop Spying`.

The implicit output Spy uses `DefaultOutputProbeResolver` to identify the
signal before the wet global effects. Its marker and tether use the cable
selected by that resolver. Its card position belongs to preset presentation,
since the implicit Spy is not a graph node or probe record. Its position is
saved with preset metadata, but presentation-only edits do not currently enter
graph undo history. Existing presets without positions place cards near their
cables until moved.

The bottom Spy rail, its tray sizing, scrolling, minimization, and keyboard
navigation are deletion targets. The Guide shelf remains a left overlay on the
full-height canvas. No Spy movement runs DSP or copies the graph during pointer
movement. Placement commits once on pointer-up.

## Verification

- A focused UI gesture covers card dragging, save/reload, and detail view.
- The existing right-click cable command and cable marker reattachment remain
  the creation and attachment paths.
- Graph tests cover position serialization and undo/redo without a graph-wide
  snapshot for a move.
- Resolver tests cover the pre-wet cable, including Waveshaper and IR.
- A production-size screenshot confirms the full-height canvas and cards.

## Architecture review

The existing compiler, executor, preview renderer, detail view, and cable
marker are reused without copied DSP or interaction logic. `SignalProbeCanvas`
owns card geometry, painting, and cable tethers; `NodeCanvas` routes pointer
gestures; `GraphCommandDispatcher` owns both Spy position commands. The
pre-wet boundary has one decision site in `DefaultOutputProbeResolver`, and
full-height Guide shelf layout has one decision site in `WorkspaceDock`.

Before/after line counts: `NodeCanvas.cpp` 2962/2964,
`NodeCanvasPresentation.cpp` 1458/1432, `GraphSerializer.cpp` 1252/1265.
`NodeCanvas` remains the large editor coordinator, but this change adds only
gesture routing there; card geometry and rendering are below it. The next
interaction extraction should move its gesture dispatch into a dedicated
canvas interaction coordinator, reducing `NodeCanvas` rather than introducing
another switchboard. `GraphSerializer` only gains the two position fields for
each Spy. No second preparation, eligibility, or output-boundary policy was
added. The obsolete tray painter, tray interaction, keyboard focus, sizing,
scrolling, and minimization paths were removed. Legacy `railOrder` JSON and
global setting keys remain for preset/settings compatibility and are inert.

Pointer movement updates a card coordinate and repaint state only. Commit
stores a two-coordinate delta for an ordinary Spy; output card movement updates
one preset presentation field without copying preview JPEG data. The native
fixture verifies both positions survive save/reload and detail view still opens.
