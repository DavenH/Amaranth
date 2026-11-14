# Curve summaries and transport polish

Status: Complete

## Contract

- Investigate the reported MIDI end/restart behavior before changing transport.
  Verify declared duration after the last note, silent gaps, CC-only timelines,
  and note release. Preserve intentional keyboard-audition duration behavior.
- Compact curve/2D previews show shape without editing intercepts, selected/free
  vertices or depth handles. Expanded editors retain editing feedback. Reuse
  Panel's existing rendering passes; preview hosts supply presentation intent.
- The Curves tab displays a small superscript count of preset-owned Guide curves,
  including zero. Update on load, add/delete, undo/redo and document replacement;
  count vector size at existing notification boundaries, with no graph scans.
- Octave controls retain their hit areas and behavior; use muted arrows at rest,
  a quiet hover/press surface and a keyboard-only focus outline. Reuse the
  existing WorkspaceDock chevron geometry.

## Owners and verification

PanelHostContext/Panel own editing-overlay eligibility; CurvePanelHost selects
preview mode for all shared flat/2D snapshot clients. InlinePresetBrowser owns
count presentation; NodeCanvas forwards document facts. LibraryIconRenderer
paints the small count beside its existing SVG. OctaveButton owns its states.
No new interaction algorithm, DSP, graph mutation route or persisted format.
Preserve the five incoming modified preset files.

Use existing transport tests and a native playback observation; add only missing
regression coverage. Verify count updates and retained editing handles through
commands, native screenshots and GL/editor state. Build exact Debug/Release app
and focused tests with parallelism 10. Review production diff, source sizes,
architecture audit, code-map and whitespace. Report new regression coverage
separately from the existing checks run.


## Outcome and evidence

No early-loop failure reproduced. Native playback of a 1.6s phrase with a 0.2s
note and a later CC point remains playing after note-off, reaches progress 1,
and stays stopped. Transport production code is unchanged. The existing roll-
length regression now also verifies the later CC reaches its endpoint, the
pattern source is released at the declared end, and subsequent updates do not
restart or enqueue another pass. Existing CC-only audition and explicit-seek
pattern-duration cases pass; these remain intentionally separate workflows.

Native accessibility labels confirm counts 2->3->2 through add/undo, redo restores
3, deletion restores 2, loading an empty preset gives 0, and returning to Sitar
Pad gives 2. Screenshots confirm compact curves without markers and expanded
Guide editing handles still present. Native octave clicks change the range by
12 semitones and restore it. The buttons retain keyboard focus/activation support
and render a focus-only outline; clicking does not grab keyboard focus.

Exact Debug app/test and Release app targets build with parallelism 10. Ran 27
existing browser/transport cases (418 assertions); extended one existing
regression rather than introducing a new test suite. Native evidence and
screenshots are under .context/curve-transport/. No requested saved preset was
modified. Architecture audit, code map and whitespace checks pass.

Responsibility/size review: NodeCanvas 3753->3757 only forwards the O(1) count at
existing document/configuration boundaries. CurvePanelInfrastructure 613->614
selects preview presentation in the existing snapshot host. Panel 1325->1328
adds shared editing-pass eligibility, preserving its default Cycle 1 behavior;
it remains the authoritative pass owner and avoids copied render algorithms.
InlinePresetBrowser 679->689 owns count/title updates; LibraryIconRenderer draws
the superscript beside the existing icon. WorkspaceDock exposes its existing
chevron painter; OctaveButton owns its hover/press/focus colors. No large-file
responsibility or policy is moved into an orchestrator.

The macOS AX focus query returned missing values during an optional keyboard-focus
capture attempt (.context/octave-tab.log), so that capture is not claimed as
verification. Native octave click/range behavior and expanded editing-handle
visibility were verified separately. Button keyboard event handling is unchanged.
