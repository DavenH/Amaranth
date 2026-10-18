# Cycle V2 Voice Context input order

Status: Complete (2026-10-11)

## Design

`NodeDefinitionRegistry` owns the semantic order of Voice Context inputs.
`NodeCanvasScene::portWorldCentre` projects that order into side-port y
coordinates. The requested order is Modulation, Unison, Pitch, Scratch; port
IDs, domains, connection kinds, and graph edges stay unchanged. The canvas
renderer and hit router continue using the shared node definition.

Saved preset positions are authored data. Of the 94 presets containing both
Unison and Scratch attachments, 64 have no attached Pitch source and already
show Modulation, Unison, Scratch top to bottom. The other 30 place Pitch above
Unison. For those, keep the Modulation and Scratch anchors where possible,
place Unison before Pitch, and distribute the available vertical space evenly
with at least 24 canvas units between adjacent node bodies. Only the three
affected source y positions may change; move Scratch down only if needed to
maintain that minimum gap. This is a one-time preset content edit, not a
runtime layout rule or a graph migration. It adds no work to gestures or audio
processing and leaves connector semantics intact.

Two of the 30 affected presets already have unrelated uncommitted user edits.
Apply the same layout operation to their current working copies, while staging
only the layout operation against their committed versions. Their other edits
must remain uncommitted and intact.

`NodeDefinition.cpp` is 824 lines and owns the central catalog of node ports,
parameters, and presentation defaults. Its stable collaborators are
`GraphNodeFactory`, graph validation, and `NodeCanvasScene`. Reordering two
entries at the existing Voice Context definition adds no second port policy or
new dependency. Searches found no Voice Context behavior that addresses these
ports by numeric index. The canvas remains the sole owner of socket geometry.

## Completion criteria

- Voice Context side ports render in Modulation, Unison, Pitch, Scratch order.
- The 30 presets with all four attachments show their source nodes in that
  order with no overlap in the source stack.
- The other 64 presets keep their authored source positions.
- Dirty preset edits remain intact and unstaged except for the requested
  position changes; graph content other than those positions is unchanged.
- A focused semantic test, standalone screenshot, build, style check,
  architecture audit, and coherent commit pass.

## Verification

The 29 staged four-input preset changes alter only the y coordinates of their
Unison, Pitch, and occasionally Scratch source nodes. A content comparison
against each committed graph confirmed all other JSON values remain equal.
The thirtieth preset, Astral 3, gained its Scratch connection in the user's
uncommitted working copy. Its working layout now has all four sources in order;
the staged committed version reorders the existing Unison and Pitch sources
without staging the user's connection or other edits. African Horn's current
working copy likewise retains its unrelated edits. The 64 presets with no
attached Pitch source were not repositioned.

The three focused CTest cases passed; 34 related Voice Context and Unison cases
passed when excluding two tests blocked by missing audio fixture files recorded
in `docs/TDD/audio-bugs.md`. The Standalone Debug agent opened the current
Astral 3 graph with zero validation issues and captured
`/private/tmp/cycle-v2-voice-port-after.png`; the before capture is
`/private/tmp/cycle-v2-voice-port-before.png`. The tighter legacy Ooh Aah preset
also opened with zero validation issues and showed separated source nodes at
`/private/tmp/cycle-v2-voice-port-old-preset.png`.
`git diff --check` and the Cycle
V2 architecture audit passed. `clang-tidy` completed without diagnostics in
the changed production file. No scalar math was introduced in a hot loop.
