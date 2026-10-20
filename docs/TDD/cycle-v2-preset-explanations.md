# Cycle V2 Preset And Node Explanations

Status: Implemented. Implements roadmap PR 1 only.

## Contract And Ownership

This is new authoring behavior. Generic type help remains owned by
`NodeDefinitionRegistry`; optional `Node::rationale` explains a particular
component. `PresetPresentation::description` remains the single preset-level
explanation. Empty notes add no editor section or authored placeholder.
`GraphSerializer` owns the optional node field; `PresetPresentationCodec`
already owns preset description persistence. Notes are plain multiline text,
limited to 16,384 characters, with no markup execution.

`GraphCommandDispatcher` validates semantic explanation edits. `GraphDocument`
records only the target ID and before/after text in its existing ordered undo
history. Node rationale follows ordinary Node value copying, deletion and
restoration. No duplicate preset/node notes store, DSP parameter, typed mesh
model, or editor selection field contains this text.

The canvas uses a quiet information action inside annotated or selected node headers and a preset
information action. Both open one shared multiline text editor with Save and
Cancel; Escape cancels, ordinary Return inserts a newline. Generic help and
specific authored rationale remain distinguishable. The existing canvas help,
JUCE CallOutBox/TextEditor, NodeGroupRenameCallout palette, and document command
routing are authoritative mechanics. No cards or group notes are introduced.

Automation provides scoped read/write explanation commands and exports the
same authored text in snapshots/node inspection without opening an editor.

## Cost And Lifecycle

Canvas help and explanation hover text use the existing console only; no canvas
TooltipClient or annotation tooltip creates unsolicited popup text. Unrelated
control tooltips remain unchanged.

All editing and persistence occur on the message thread. Typing changes only
local editor text. Save captures O(text length) history for one target; target
lookup uses the existing graph index. It neither copies/serializes unrelated
graph or audio resources nor advances NodeGraph's synthesis revision. Notes
publish an annotation-only document change. Undo/redo must skip audio/preview
refresh for such changes. Repaint may traverse visible presentation as usual.

## Size And Deletion Targets

Reuse existing serializer, dispatcher, document history, canvas input and
inspection owners. Keep shared text editing and badge geometry/painting in a
focused UI collaborator (under 300 lines); keep new explanation command/history
logic in focused implementation files. NodeCanvas, GraphSerializer and
NodeCanvasPresentation already exceed size triggers; add only routing/codec
hooks and no second policy for annotation mutation or geometry. Remove any
interim duplicated editor or text store before completion. No existing mature
DSP or interaction algorithm is replaced.

## Acceptance

- Human and agent author, inspect, edit, clear, save/reload and undo node and
  preset explanations; unchanged text creates no undo step.
- Missing node, malformed/non-text input, and oversized notes fail atomically.
- Deletion/undo and Node value copying preserve rationale appropriately.
- Generic help survives specific notes; all node types support notes.
- Multiple local editor updates followed by Save create one undo entry;
  Cancel/Escape leave the document untouched.
- Annotation commit/undo/redo preserve compiled plan, prepared resources and
  audio content; operation counters show no unrelated graph/model copying.
- Focused semantic tests, exact app build, coordinated GUI fixture and actual
  production-size screenshot review, style review, code-map check, architecture
  audit and coherent commit are complete.

## Evidence

The exact standalone-debug app and CycleV2_tests targets build with
BUILD_TESTING enabled. Fourteen focused CTest cases pass, covering authored
notes, atomic validation, save points, document replacement, local drafts,
header geometry, and existing document/transaction behavior. Text-only history
copies no unrelated graph, mesh or sample state with 0 and 128 unrelated nodes;
canvas note undo/redo preserves the compiled/audio/preview revisions and counts.
The 31-command explanation fixture passes, including real routed in-app badge
and Save/Cancel controls. Corrected production-size component screenshots show
inset header information, readable generic help, and preset access outside the
sidebar. The exact-PID native smoke also passes badge click, multiline typing,
local draft inspection, Save, one-step undo, preset information and Escape.
Its earlier click misses coincided with a stale same-bundle root instance
remaining frontmost after System Events reported successful activation; that
instance was stopped through its owning coordinator. A fresh exact-worktree
app and exact-PID activation completed the native route without an app failure.
The coordinator also encountered an independent workspace's foreground lease
and waited without disturbing that instance.

New production implementation files are 59 lines for graph commands, under
180 for shared editing/geometry and under 100 for canvas note orchestration.
Clang-tidy on the three new implementation files reports no user-code
diagnostics. The semantic map and diff whitespace checks pass. The architecture
size review below records existing large owners and the final canvas-header
size. The final closed-editor OS window capture is
`.context/annotation-header.png`, showing console explanation text with no
canvas popup; native proof is
`.context/native-explanation-report.json` and `.context/native-explanation-final.log`.
The GUI fixture report is `.context/explanations-report.json`, and focused test
proof is `.context/explanations-ctest.log`. Screenshots were inspected at the
actual production size.

The console refinement builds both exact targets and passes ten focused cases
with 225 assertions and no JUCE assertion or leak output. It adds CRLF/hard-break,
normal/narrow wrapping, fixed font-size, utility clearance and accessible
information-action checks. The extended 31-command fixture passes. The final
real OS screenshot `.context/annotation-header-refined.png` shows both an
authored hard newline and a wrapped paragraph, with the quiet preset circle.
The native edit/Save/undo/preset/Escape smoke also passes on the fresh exact app.
The local native harness waits up to one second for asynchronous activation and
avoids resetting AX main-window focus when its app is already frontmost; this
resolved the bounded follow-up run without a product focus workaround. Evidence
is `.context/console-refinement-build.log`, `.context/console-refinement-tests.log`
and `.context/native-console-refinement.log`. The wrapping/cache/continuation
policy remains in NodeCanvasChromePresentation (289 lines); dock geometry stays
in CanvasUtilityDock, and information-action painting stays in ExplanationEditor
(167 lines). No graph, persistence or audio ownership changes in this refinement.


## Implementation Shape Review

Information hit bounds follow the existing 27-unit header control size and zoom,
centred on the shared header baseline. The quiet circle is inset inside those
bounds. Its slot clears the existing port-layout action, Envelope-purpose icon
and global-processing indicator; title reservation comes from the same geometry.
No part protrudes outside the header. Only selected empty nodes and nodes with
authored rationale show the action.
The shared editor is 430 pixels wide, with a 14-pixel outer inset, a 26-pixel
heading, an optional 38-pixel generic-help area, and 72 by 28 Save/Cancel
buttons separated by 8 pixels. Return inserts a line break; Escape cancels;
Command+Return saves. The console preserves authored line breaks and wraps at the existing 14-pixel
font, without popup tooltips or horizontal shrinking. Its utility region is
94 pixels high when space permits, sharing one layout with repaint bounds and
remaining clear of the keyboard and minimap. Long notes show complete visible
lines and a continuation ellipsis; the information action opens the shared
scrollable editor for the entire note. Text layout is cached by text and width.
The console clears its complete region on long-to-short transitions.

The preset action reuses the node information-circle painter: a 28 by 28 hit
target contains an approximately 18-pixel glyph, without a boxed label. Its
accessibility title is Preset explanation, its description states the read/edit
action, and keyboard focus has a visible ring. The action retains its existing
component identity and editor route. Hovering it publishes the preset rationale
through the same console, including when the pointer is over the child button.

Baseline/touched large sources: NodeCanvas 3468 -> 3506 lines, presentation
1511 -> 1525, serializer 1322 -> 1340, authoring 1036 -> 1043, automation
inspector 1139 -> 1162, NodeGraph header 464 -> 468, canvas header 442 -> 450.
Canvas remains the widget-lifecycle/input coordinator; `NodeCanvasExplanations`
owns note hit resolution, editor orchestration and inspection. `ExplanationEditor`
owns shared editing and badge eligibility/geometry, and
`GraphExplanationCommands` owns explanation validation/history integration.
The canvas header reaches the 450-line trigger: the additional public note
actions and owned widget lifetime remain cohesive with its existing surface.
The stable extraction target is its existing sidebar/overlay composition, including preset information, rather than a note-specific facade.
Serializer continues to own the one graph codec; no schema or persistence
policy is duplicated. There are no new NodeKind/DSP branches or compatibility
paths. The semantic, native and screenshot acceptance evidence is complete.
The editor draft test owns a scoped JUCE GUI lifetime. Its final explanation
group passes seven cases and 159 assertions with no JUCE assertions or leak
warnings (`.context/explanation-gui-lifetime-tests.log`).

Document replacement extends the existing graph snapshot history with optional
matching `PresetPresentation`; only replacement snapshots capture it. Ordinary
structural edits retain their existing graph history and explanation edits
still retain only target ID and two strings. Opening A then B and traversing
undo/redo must restore each description with the corresponding graph and save
that same pairing. This closes the former replacement-history metadata gap.
