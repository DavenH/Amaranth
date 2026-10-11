# Cycle V2 MIDI CC controls and piano roll lanes

Status: Complete (placement correction, 2026-10-11)

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
