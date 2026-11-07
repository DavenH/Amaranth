# Console footer and sidebar grouping

Status: Complete (2026-11-07).

The console uses one 28 px line in a shared 30 px grey canvas footer. Its
information button and hover precedence remain; overflow uses an ellipsis.
Legend glyph groups end 18 px from the canvas right edge on the same row.
Graph fitting now clears the keyboard above and footer below, reclaiming the
old top-left status block. The node minimap has no layout, renderer, scene
target or telemetry stage; MIDI pattern minimaps remain.

Sidebar tabs group WAV references, Presets and MIDI patterns before a 16 px
separation and vertical rule, followed by Nodes and Curves. A horizontal rule
at 46 px separates tab chrome from content. GuideCurveShelf paints the common
sidebar backing beneath its thumbnails; InlinePresetBrowser stays transparent
where canvas-owned nodes/guides must remain visible and interactive.

Ownership and review

CanvasUtilityDock owns geometry; NodeCanvasChromePresentation owns footer
painting. InlinePresetBrowser owns tab layout; GuideCurveShelf owns the guide
backing. Multiline TextLayout/cache and node minimap mechanics are deleted.
The status-only paint path stays O(1) with respect to graph size: clear only
its footer text region and draw one line. No graph edits or audio changes.

NodeCanvas (3,751 lines) changes only fit clearance. NodeCanvasPresentation
(1,532) coordinates footer painting and removes minimap routing. Existing
orchestrator decomposition remains in refactors.md; no new responsibilities
are added. InlinePresetBrowser (839) retains cohesive sidebar composition and
tab geometry; its small change adds group spacing and separators. Chrome
presentation shrinks to 192 lines; the guide shelf remains below 450.

Verification

Exact Debug/Release apps and tests build. Focused status, utility, sidebar,
inline browser, explanation and canvas-performance checks pass 1,133 assertions
in 51 cases. Geometry tests cover normal/compact footer containment, equal
baselines, tight status/legend separation, tab ordering and the group gap.
Status tests flatten line breaks/tabs while preserving hover precedence.

scripts/test_cycle_v2_console_sidebar.py passes in an isolated native instance:
all group positions, lower-left information target, real MOD hover console
content, single-line text, tab switching and resize. Captures of Nodes, Curves,
Presets and compact windows were reviewed, including visible guide previews,
matching sidebar backing, removed minimap, content divider and visible legend.
Native evidence is in .context/console-sidebar/; focused logs are
.context/console-sidebar-tests.log. The footer sits above the reference tray.

Production diff/style, code-map, architecture audit and whitespace checks pass.
No new incidental application assertions or crashes were observed. User preset
edits remain outside this change.

Follow-up: PatternBrowser's Edit button now supplies console help for editing
user notes/automation or copying a factory pattern. The shared help lookup is
covered by the pattern-browser test, and the native sidebar check verifies real
pointer hover over the enabled factory Edit action. Debug/Release builds and
845 assertions in 10 pattern/status cases pass. Evidence:
`.context/midi-edit-help-tests.log`, `.context/midi-edit-help-native.log`, and
`.context/console-sidebar/pattern-edit-help.png`.
