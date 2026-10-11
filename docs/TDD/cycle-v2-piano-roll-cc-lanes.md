# Cycle V2 MIDI CC controls and piano roll lanes

Status: Complete (2026-10-11)

## Live graph preview and first-note automation

The user reports that mapped CC wheels update sound but not graph previews,
and that the leading automation value does not affect the first note. The
existing mod-wheel preview path is authoritative: it records a controller
gesture, updates bound morph targets through `GraphCommandDispatcher`, and
invalidates only affected preview roots. Extend that path to a controller
number and current controller values. `PreviewMorphBinding` must bind MIDI CC
sources on Envelope and TrilinearMesh axes; the presentation snapshot, preview
renderer, fingerprint, and expanded probe capture must carry the same CC state.
Audio graph state remains owned by the existing MIDI sink. A live movement may
touch only the mapped axis and downstream local preview products; no graph
clone, unrelated resource preparation, or serialization may occur.

The existing `PatternControlScheduler` owns interpolation and timed CC output.
Create one scheduler per automation controller, and enqueue each lane's first
value before note events with the same timestamp. Keep the real-time queue's
timestamp and sequence ordering unchanged. Test CC1 and CC73 together with a
first note at time zero, including a first automation point after zero, and
prove the initial value arrives before NoteOn.

Completion requires two-update CC73 graph preview gesture, durable morph
commit and undo, observable modulation and probe changes without unrelated
preparation, multi-lane playback ordering, focused alto-sax-2 UI fixture,
build, audit, style check, and commit. The verification below closes these
criteria.

### Implementation and review

`ModulationSource::controllerFor` owns the MIDI controller identity shared by
compiled preview root discovery and bound morph axes. The presentation model
stores controller values and local roots per controller; the preview renderer
uses those values for ordinary and expanded probe previews. `NodeCanvas` only
routes wheel gestures and requests a preview of the affected roots. The graph
dispatcher owns envelope and mesh morph edits. Its envelope replacement now
uses the durable model revision captured by the gesture and reuses the transient
model revision on later movements, so two live updates commit as one delta.
The existing MOD wheel follows the same path.

`PatternControlScheduler` still owns interpolation. `PerformanceKeyboardPanel`
creates one scheduler per controller and queues each lane's initial event before
NoteOn. The sink's existing timestamp and insertion sequence ordering preserves
that order at the audio boundary. A point after time zero supplies its value
from time zero, matching the scheduler's established leading-value behavior.

The focused C++ tests cover CC73 graph source, bound envelope and mesh axes,
alto-sax-2 downstream spy and expanded capture, two transient envelope updates
with undo, and first-note CC1/CC73 order. UI fixtures cover release and Live
CC73 drags with parameter changes, preview rendering, single commit, and undo.
The Live fixture waits four seconds for the large alto-sax-2 preview to settle.
The focused suite passes with 343 assertions in 14 cases. Both UI fixtures pass,
their filtered logs contain no errors, the architecture audit and diff check
pass, and the standalone and test targets build with `--parallel 10`.

`NodeCanvas.cpp` grows from 3,324 to 3,360 lines. It retains orchestration of
the same preview gesture lifecycle and delegates controller discovery, morph
editing, and rendering to their existing owners. The file remains large; a
future extraction of the preview gesture orchestration could reduce it, but
this change adds no second policy site. `PerformanceKeyboard.cpp` grows from
968 to 1,010 lines and retains keyboard MIDI routing and transport; controller
lanes reuse the existing scheduler. No domain interaction or raster behavior
was copied, and no unrelated graph scale work is added to movement updates.

## Design

The committed graph's attached Modulation Source and Modulation Triple nodes
identify MIDI controllers available for live audition. `mappedPresetMidiControllers`
is the authoritative discovery query. `NodeCanvas` caches its result by durable
document revision and invalidates it on graph load. Discovery costs O(nodes +
edges) after a graph change; pointer movement and painting do not rescan the graph.

The performance keyboard's existing `ModWheel` owns pointer, keyboard, paint,
and MIDI value behavior. Additional mapped controllers use the same component
beside MOD. `CanvasUtilityDock` widens the keyboard by one wheel column per
additional controller. The keyboard emits ordinary performance MIDI CC events
and sends current wheel values at playback start.

The piano roll contains saved automation only. It shows a vertically stacked
lane for CC1, each mapped controller, and any controller already in the saved
phrase. Point gestures edit one controller and publish once on release through
the existing phrase save path. Movement touches one point outside local repaint;
there is no graph clone, serialization, or audio preparation. Playback uses the
existing `PatternControlScheduler` for every controller.

The keyboard and editor share only the controller list. The editor owns lane
layout and point gestures; the keyboard owns live values and MIDI routing.
Deletion targets are the piano roll's live slider row, controller selector, and
single-lane rendering assumption.

## Verification

- Alto-sax-2 discovers CC1 and CC73; the agent fixture shows both wheels and
  asserts a CC73 pointer gesture changes its live value.
- The focused C++ tests assert CC73 MIDI, playback initialization, separate
  CC1 and CC73 point edits, one publication per gesture, lane retention after
  a mapping change, and normal and compact keyboard geometry. Seven relevant
  cases pass with 67 assertions.
- Production-size captures at `/private/tmp/cycle-v2-mapped-cc-keyboard.png`
  and `/private/tmp/cycle-v2-piano-roll-mapped-cc.png` show the keyboard controls
  and stacked piano roll lanes without clipping. The seven-command agent
  fixture passes; its filtered launch log has no errors.
- Standalone and test targets build with `--parallel 10`. Architecture audit
  and `git diff --check` pass. The changed hot paths add no scalar math calls.
  Clang-tidy found existing warnings plus a new recursive screenshot lookup;
  the new lookup was changed to iterative traversal.

## Architecture review

The keyboard component remains the owner of performance MIDI routing and
transport. `NodeCanvas` coordinates graph discovery and dock geometry;
`PresetMidiEditor` owns only phrase editing. The automation capture extension
locates the existing editor component by name and contains no editor behavior.
`NodeCanvas.cpp` grows from 3,305 to 3,324 lines; its 19-line net increase
only caches the delegated graph query and passes the controller count to dock
layout. The existing orchestration remains cohesive enough for this slice;
graph discovery stays in the Graph helper and keyboard layout stays in
`CanvasUtilityDock`. `PerformanceKeyboard.cpp` is 968 lines, and the two
editor implementation files total 893 lines. No new graph mutation path or
duplicate eligibility policy was added. Production diff inspection found no
copied interaction behavior: additional CC wheels reuse `ModWheel` directly.
