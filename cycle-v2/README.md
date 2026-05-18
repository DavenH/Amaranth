# Cycle 2.0

Cycle 2.0 is a sister project to Cycle, built around a node-based workflow for explicit audio, spectral, mesh, envelope, pitch, voice, and scratch-attachment routing.

## Build and Run

From the repository root:

```bash
cmake --preset standalone-debug
cmake --build build/standalone-debug --target CycleV2 --parallel 10
open build/standalone-debug/cycle-v2/CycleV2.app
```

## Current Editor Controls

- drag empty canvas: pan
- mouse wheel: zoom around cursor
- drag a node: move it
- drag from one port to another: connect compatible ports
- click palette item: add a node at the viewport center
- click cable: select edge
- double-click node: open or close the expanded node panel
- click empty canvas or press `Esc`: dismiss expanded panel and selection
- `Delete` / `Backspace`: delete selected node or edge
- `Cmd/Ctrl+Z`: undo
- `Cmd/Ctrl+Shift+Z` or `Cmd/Ctrl+Y`: redo
- `Cmd/Ctrl+S`: save graph snapshot
- `Cmd/Ctrl+O`: load graph snapshot

## Tests

```bash
cmake --preset tests
cmake --build build/tests --target CycleV2_tests --parallel 10
ctest --test-dir build/tests -R "Demo graph|Runtime|Audio signal|Channel layouts|Compiler" -V
```

## Design Direction

The editor should feel like a studio canvas rather than a legacy panel UI:

- large dark grid canvas with minimap navigation
- soft, routed cables that avoid node bodies where possible
- domain-colored ports and cables for time, spectral magnitude, spectral phase, envelope/control, mesh, pitch, and voice routing
- explicit stereo split/join nodes for visible left/right channel routing
- explicit scratch attachment ports so hidden Cycle 1.x scratch assignments become visible and traceable
- rich per-node previews: trilinear mesh surfaces, 2D mesh slices, waveform/cyclogram views, spectrograms, spectral magnitude/phase graphs, envelopes, and meters
- double-click expansion into a larger editor for the selected node

## Visual References

These generated UI mockups are visual references, not implementation snapshots. They capture the intended density, routing clarity, node-preview richness, and studio-canvas atmosphere.

![Spectral morph node editor mockup](../docs/cycle-v2/mockups/spectral-morph-node-editor.png)

![Voice waveform node editor mockup](../docs/cycle-v2/mockups/voice-waveform-node-editor.png)
