# Cycle V2 Canvas Utility Dock And Status

## Status

Implemented (2026-08-25).

## Problem

The canvas currently presents utilities as unrelated overlays. The minimap has
its own status line, hover information appears in a second lower-left line, and
the performance keyboard is persisted as draggable world content even though
it is an application control. The cable legend also describes more visual
categories than the product needs. Guide relationship tethers begin inside the
Guide shelf and are subsequently masked by dock chrome, obscuring their origin.

## Authoritative Implementations

- `NodeCanvasChromePresentation.cpp` owns minimap, legend, and status painting.
- `NodeWorkspace` owns the live `PerformanceKeyboardPanel` and audio/MIDI
  lifecycle.
- `NodeCableRenderer` owns cable stroke rendering; `colourForDomain()` owns the
  shared domain palette.
- `WorkspaceDock::layout()` owns the Guide/Spy dock boundary and
  `GuideRelationshipPresentation` owns hover-only Guide routes.

The change reuses those implementations. `CanvasUtilityDock` adds only shared
screen-space geometry and chrome constants; it does not copy minimap,
keyboard, cable, or Guide behavior.

## Product Contract

- There is one canvas status surface, in the lower left. Current hover help
  temporarily replaces the last edit result and every message is a concise,
  informative phrase rather than a slash-delimited diagnostic breadcrumb.
- The minimap, legend, and performance keyboard use one screen-space utility
  dock layout with common right alignment, margins, gaps, surface colour,
  border, and corner radius.
- The minimap and four-entry legend form the upper-right utility column. The
  legend contains only Time, Magnitude, Phase, and Control.
- Envelope, pitch, voice-control, context, mesh-field, and generic control
  domains share the Control colour. Attachment semantics remain available from
  ports, endpoints, and hover text; cables do not use a separate dashed stroke
  or legend entry.
- The performance keyboard is a compact, fixed lower-right application
  control. It does not pan with the graph, participate in document undo, or
  serialize position into `.cyclegraph` content.
- A Guide hover tether begins at a visible guide-coloured terminal on the
  Guide/Spy dock boundary. The terminal is layered above dock chrome so the
  route origin cannot be masked.
- Expanded editors remain above canvas utilities. The keyboard releases held
  notes while hidden by an expanded editor and returns to its dock afterward.

## Ownership And Lifecycle

`CanvasUtilityDock::layout()` consumes the current unobscured canvas content
bounds and returns minimap, legend, keyboard, and status rectangles. JUCE canvas
painting uses the minimap, legend, and status rectangles. `NodeWorkspace` uses
the same keyboard rectangle for its child component. Keyboard note state and
audio-device lifecycle remain owned by `NodeWorkspace`.

The fixed keyboard location is application presentation, not document content.
The old graph field, serializer property, dispatcher command, drag gesture, and
movement automation state are deletion targets rather than a compatibility
layer; Cycle V2 is undeployed.

## Semantic Verification

- Layout tests prove right alignment, non-overlap, shared spacing, lower-left
  status placement, and usable small-window bounds.
- Status tests prove hover precedence, edit-status fallback, and sentence-like
  node, port, edge, palette, and action descriptions without breadcrumb
  separators.
- Cable tests prove attachment and signal styles share the same solid stroke,
  and domain tests prove every control-class domain maps to the Control colour.
- Keyboard tests prove the panel has no drag handle and always occupies the
  utility dock rectangle independently of graph pan, zoom, serialization, and
  undo.
- Guide relationship rendering proves the route reaches a visible terminal at
  the dock boundary while preserving fan-out and editor occlusion.
- A native fixture captures all utilities, the sole lower-left status surface,
  reduced legend, docked keyboard, and Guide tether terminals.

## Deletion Targets

- minimap-adjacent status painting;
- slash-delimited canvas hover breadcrumbs;
- Envelope and Attachment legend entries and the Universal label;
- dashed attachment cable rendering;
- draggable keyboard header behavior and movement callbacks;
- `NodeGraph::performanceKeyboardBounds` and its serializer property;
- `GraphCommandDispatcher::setPerformanceKeyboardBounds`;
- canvas world/screen keyboard translation and movement commands;
- tests and automation fields that describe keyboard world position or
  document persistence.

## Completion Criteria

- One lower-left status surface presents natural-language hover and edit
  feedback.
- Minimap, four-entry legend, and keyboard share the utility dock layout.
- The keyboard is fixed screen UI and no keyboard position remains in graph
  content, commands, undo, or serialization.
- Cable colour and stroke presentation has exactly four legend classes and no
  attachment-specific dashed style.
- Hovered Guide tethers visibly terminate at the Guide/Spy dock edge.
- Focused tests, the full Cycle V2 suite, standalone build, native automation,
  OS-rendered capture, style checks, and production-diff review pass.

## Implementation Evidence

- `CycleV2_tests` passes all 498 test cases and 8,817 assertions, including the
  focused utility-dock, status, relationship, query, hit-router, cable, legend,
  keyboard, and serialization contracts.
- The standalone `CycleV2` app and test executable build from the
  `standalone-debug` and `tests` presets with `--parallel 10`.
- `cycle-v2-agent-guide-dock-tether.json` completed all five commands through
  the LaunchServices-aware runner. It verified Guide hover state and reported a
  visible, docked 420-by-126 keyboard at screen position 1290,628.
- The docked-keyboard fixture completed 23 commands, including MIDI press,
  drag, release, audio output, and final-idle checks. The canvas-composition
  fixture completed 13 commands and proved graph panning leaves the keyboard
  docked, opening the Trimesh editor synchronously hides it, releases its held
  note, and reports editor occlusion.
- The native OS canvas capture at
  `/private/tmp/cycle-v2-canvas-utility-dock-native.png` verifies the unified
  minimap/legend/keyboard surfaces, sole lower-left status, four-entry legend,
  compact keyboard, Guide/Spy dock, and hover relationship presentation.
- `git diff --check`, all changed preset/fixture JSON parses, the preset-port
  script syntax check, the hot-loop scalar-math scan, deletion-target search,
  and production diff review pass. No new node-kind branch or compatibility
  adapter was introduced. `clang-tidy` is unavailable in this environment.
- The launch log contains only the previously tracked JUCE settings assertions
  at `Settings.cpp:228` and `Settings.cpp:229`; the fixture and capture
  completed normally.
