# Cycle V2 preset search fields

Status: Complete (2026-10-04).

## Contract and ownership

The sidebar and full preset browser both use `LibrarySearchField`, a `TextEditor`
subclass that owns their common placeholder styling, search icon identity, and
Space handling. When its text is empty, an unmodified Space is consumed and
calls the supplied playback toggle. When text is present, JUCE's editor handles
Space as text input. Search queries, selection, and list scrolling remain owned
by each browser. The existing `NodeWorkspace` playback action owns transport;
the field only invokes a callback on the UI thread.

The field adds O(1) work per key. The old duplicated field setup is deleted
from both browser constructors. The icon is painted by the field itself, so
it does not depend on a browser-specific look and feel. No graph or audio
state is copied.

## Completion criteria

- Both preset searches use one styled field and draw the same search icon.
- Empty-field Space toggles playback without inserting text; nonempty-field
  Space inserts text without toggling playback.
- The sidebar and full browser route playback to the existing workspace
  transport action, and a focused regression test and UI fixture pass.
- Production-size inspection, style review, architecture audit, and commit are
  complete.

## Verification and architecture review

The focused browser tests pass: 123 assertions in nine cases. They cover both
browser hosts, the shared field type, empty-field playback routing, and normal
Space insertion after search text and page-level Space outside search. The
visual agent fixture passes all five
commands and captures the focused sidebar field at
`/private/tmp/cycle-v2-preset-search.png`. The agent's synthetic pointer API
does not establish keyboard focus, so keyboard behavior is asserted in the
component tests instead of an agent key command.

`LibrarySearchField` is 61 lines after the shared icon and whitespace handling.
The browser constructors lose their duplicated styling; the look and feel
removes its component-ID special case.
`NodeWorkspace` remains the playback owner, with its existing automation
entrypoint delegating to the UI action. `NodeCanvas.cpp` is already above the
size-plan trigger, so this change adds only five lines of sidebar callback
wiring. The sidebar-host extraction plan in
`docs/TDD/cycle-v2-pattern-library.md` remains the deletion target for that
wiring and other sidebar orchestration. The architecture audit finds no new
size trigger.

## Native macOS Space route (2026-10-04)

JUCE's macOS peer sends printable text to the focused `TextInputTarget` through
the input context before `keyPressed`. This bypassed the earlier key override
and let Space enter an empty search. `LibrarySearchField::insertTextAtCaret`
now consumes a single Space when the field contains only whitespace and invokes
the existing transport callback. Text with a nonempty query still uses JUCE's
normal insertion. A focused test covers both routes. An isolated running Cycle
instance received native macOS text and key events: Space started preview
playback from the empty focused field, and the next Space stopped it.
