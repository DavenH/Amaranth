# Cycle V2 Piano Roll CC Lanes

Status: Complete

## Design

The committed graph's attached Modulation Source and Modulation Triple nodes
identify the controllers available for live audition. The piano roll also
retains lanes found in saved phrase points, so an edited phrase never hides its
own data after a graph change. CC1 is the default lane for an empty phrase.
Discovery happens when the editor opens, outside pointer gestures. The editor
owns only transient slider values and the selected lane; neither selection nor
slider movement changes the saved phrase. Slider movement emits ordinary MIDI
CC through the existing keyboard sink. Point gestures retain the existing
one-publication-on-release contract and touch only their selected lane.

The CC selector occupies the left label gutter of the modulation lane. Live
sliders occupy a compact horizontal row above the minimap, leaving the main
note grid as the largest region. The minimap follows the selected lane.

The graph helper scans outgoing edge sources once and returns distinct
controller numbers. Discovery is O(nodes + edges) when the editor opens. Each
point movement remains O(1) outside the existing local repaint. There is no
graph clone, serialization, or audio preparation during these gestures.

`NodeCanvas` is 3,305 lines after the delegating query, versus 3,300
before. It still owns canvas orchestration and delegates controller discovery
to the Graph helper. `PerformanceKeyboard.cpp` is 921 lines after the MIDI
callback and editor construction wiring, versus 903 before. It remains the
sole owner of performance MIDI routing and note transport; no graph policy was
added there. The editor grew from 484 to 566 lines, below its review threshold.
The old implicit lane selection was deleted from both the editor and minimap.
The new query does not duplicate the runtime's modulation evaluation; it only
reads attached source identifiers and controller parameters for presentation.

## Verification

- Alto-sax-2 exposes CC1 and CC73 through attached triples. The graph load and
  discovery test passes.
- The multi-lane interaction test selects CC73, adds a point without changing
  CC1, and sends CC73 value 91 from its live slider without publishing a phrase
  edit. The focused editor suite passes 55 assertions in nine cases.
- The standalone Cycle V2 build and focused four-command automation fixture
  pass. The 840-by-640 capture at
  `/private/tmp/cycle-v2-piano-roll-mapped-cc.png` shows both live sliders and
  a legible CC selector. The filtered launch log contains no errors.
- `git diff --check` and the architecture audit pass. The modified UI code
  adds no scalar `std::<math>` calls in hot loops. Clang-tidy reports existing
  narrowing warnings in the editor's navigation code.

Deletion target: the implicit CC1-or-first-point lane decision in the editor
and minimap. Completion requires an attached-source discovery test, multi-lane
edit test, MIDI slider test, production-size capture, build, style and
architecture checks, and commit.
