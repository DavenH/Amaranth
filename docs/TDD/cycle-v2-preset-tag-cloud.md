# Preset tag cloud selection and editing

Status: implemented.

SidebarTagCloud owns independent filter and selected-record tag states. Magenta
means filter, blue means selected-record membership, purple means both. Left click
retains AND filtering; right click emits a tag edit intent, never a filter edit.
Favorites remains a filter, not an editable tag. InlinePresetBrowser owns selected
row synchronization and delegates atomic file writes to PresetMetadataStore and
loaded-document updates to its existing metadata callback. List selection changes
must notify on pointer, keyboard, index refresh and filtering. Keep discovered tags
available during the browser session so removal of the last occurrence is reversible.

An explicitly empty tag array differs from missing tags (legacy pattern fallback).
PresetPresentation codec preserves this distinction through save/reload, and the
existing dispatcher metadata command sets it. This is metadata-only, not a new graph
mutation path. No audio or gesture algorithms change. UI callbacks run on the message
thread; metadata writes use the established temporary-file store. Right click does
one existing metadata write/index refresh, no preset load or favorite/filter mutation.

Prove rendered states and complete selection/filter/right-click/save/reload sequence,
including removing the final tag and removing a currently filtered tag. Use temporary
preset copies only. Build, focused tests, style/diff and architecture review required.

## Verification and review

- Standalone Debug and CycleV2_tests builds pass with `--parallel 10`.
- Focused inline browser, preset tag and serialization coverage: 28 cases,
  1,307 assertions, including rendered chip colours and temporary-file edits.
  Follow-up loaded-document empty-tag save coverage: 5 cases, 998 assertions.
- Native sidebar capture `/private/tmp/tag-cloud-native.png` confirms the combined
  purple state; `/private/tmp/tag-cloud-states.png` covers all three accents.
- Refactor pass removed a redundant local metadata update: the existing index
  refresh alone republishes records and reconciles selection. No duplicate save or
  graph mutation implementation was introduced.
- Architecture audit reviewed; touched production sizes are InlinePresetBrowser
  760/140, SidebarTagCloud 224/54, GraphCommandDispatcher 623,
  PresetMetadataStore 93, PresetPresentation 308/83 (cpp/header).
  InlinePresetBrowser coordinates selection, filtering and metadata callbacks;
  CompactList owns row selection, SidebarTagCloud owns chip presentation/filter
  state, PresetMetadataStore owns atomic persistence, the index owns refreshed
  records, and GraphCommandDispatcher owns loaded-document metadata changes.
  Existing metadata dialogs use the same persistence boundary. No new kind
  branches, adapters, hot loops, or deletion targets remain.
- Production diff reviewed against the style guide; `git diff --check` passes.
