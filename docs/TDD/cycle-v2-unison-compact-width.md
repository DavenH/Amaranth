# Cycle V2 Unison compact width

Status: Complete (2026-10-11)

## Contract

`NodeDefinition` and `naturalSizeForNode` own compact node sizing. Envelope's
269.2-unit preview plus 26 units of node padding gives a 295.2-unit body.
Unison currently uses 230 plus the same padding, giving 256 units. Its new
compact body should match Envelope at 295.2 units, without changing height or
preview semantics.

`GraphSerializer` owns the position-only saved graph format. Existing Unison
positions were authored with a 256-unit body. On loading an old graph, move
each Unison x position left by 39.2 units, preserving its right edge. A saved
layout marker identifies graphs already using the new width so load/save/load
is stable. Newly created nodes use the new size immediately. The conversion
scans only the nodes already being deserialized and does no work during drag.

The parser continues to own graph restoration; the canvas and command service
receive ordinary node bounds and need no compatibility branch. No old
rendering or graph interaction path is copied or left to delete.

## Architecture review

`NodeDefinition.cpp` (823 lines before this change) owns the catalog of node
defaults and presentation sizes; `NodeGraph::naturalSizeForNode` applies its
shared padding. `GraphSerializer.cpp` (1,322 lines before this change) owns
graph format writing, parsing, model codecs, and validation. Its stable
collaborators are the node definition registry, graph factory, model codecs,
and graph validator. This change adds one version fact at the existing JSON
root writer and one position conversion at the existing node reader. Moving
these lines to a separate class would retain the same parser responsibility
and add an unnecessary dependency. No UI, graph editor, or runtime code makes
the same position decision. The larger serializer decomposition remains a
separate responsibility review; this slice does not add model parsing or a
new client branch to its public interface.

## Completion criteria

- New Envelope and Unison compact nodes have the same width.
- An old serialized Unison keeps its right edge when loaded.
- Saving and reloading does not move it again.
- Existing preset screenshot shows the width alignment and preserved routing.
- Focused tests, standalone fixture, style check, architecture audit, and
  coherent commit pass.

## Verification

The factory and serializer regression checks passed with 10/10 focused CTest
cases. The Standalone Debug agent opened `astral-3.cyclegraph` with zero graph
issues and captured its canvas at
`/private/tmp/cycle-v2-unison-width-after.png`; the Unison and neighboring
Envelope bodies share the same left and right edges in the rendered view.
`git diff --check` and the architecture audit passed. `clang-tidy` completed;
its diagnostics in the touched serializer are on existing lines outside this
change. The touched graph files contain no new scalar math in hot loops.
