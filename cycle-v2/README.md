# Cycle 2.0

Cycle 2.0 is a sister project to Cycle, built around a node-based workflow for explicit audio, spectral, mesh, envelope, pitch, voice, and scratch-attachment routing.

The current implementation is an early standalone prototype. It has a spacious OpenGL-backed node canvas, typed graph validation, graph compilation, a runtime trace shell, node dragging, pan/zoom, minimap, execution badges, channel labels, and expandable node panels.

## Build and Run

From the repository root:

```bash
cmake --preset standalone-debug
cmake --build build/standalone-debug --target CycleV2 --parallel 10
open build/standalone-debug/cycle-v2/CycleV2.app
```

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
- explicit scratch attachment ports so hidden Cycle 1.x scratch assignments become visible and traceable
- rich per-node previews: trilinear mesh surfaces, 2D mesh slices, waveform/cyclogram views, spectrograms, spectral magnitude/phase graphs, envelopes, and meters
- double-click expansion into a larger editor for the selected node

The generated UI mockups from the design discussion should be treated as visual references, not implementation snapshots. If they are added to the repository, store them under `cycle-v2/resources/mockups/` and reference them from this README.

## Current Scope

Implemented:

- `NodeGraph` model with typed ports, domains, channel layouts, node kinds, and scratch attachments
- graph validation for missing nodes/ports, domain mismatches, channel layout mismatches, scratch attachment misuse, pitch routing, and cycles
- graph compiler that separates signal edges from attachment edges and produces deterministic execution order
- runtime trace shell for inspecting compiled execution state
- standalone canvas UI with placeholder previews

Not implemented yet:

- real DSP execution
- real mesh/spectrum/cyclogram preview renderers
- node creation/deletion and cable editing
- preset migration/loading
- final all-OpenGL path/sprite widget renderer
