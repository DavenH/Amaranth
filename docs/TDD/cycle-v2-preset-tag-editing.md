# Cycle V2 preset tag editing

Status: Complete (2026-10-04).

## Design

`PresetPresentationCodec` owns tag encoding, and `PresetLibraryIndex` reads
metadata for search. A narrow `PresetTagStore` updates the `tags` property
in the existing preset JSON and writes it atomically. It preserves the graph,
preview, pattern reference, and unknown JSON properties. The expanded browser
offers Edit Tags for its selected card, normalizes comma-separated input,
saves it, and refreshes that record in both the expanded and sidebar indexes.
When the file is the loaded preset, `GraphCommandDispatcher` updates the current
document tags so a later preset save does not restore the old tags. The edit
does not change the graph revision or audio plan.

The curated taxonomy puts every shipped filename containing `sax` in Brass;
Kicker and Stomper are Bass. The tagging script and saved files must agree.

## Completion criteria

- The nine specified preset files and the tagging script agree on family tags.
- The expanded browser can edit and persist a selected preset's tags.
- Save preserves unrelated preset JSON and refreshes visible metadata/search.
- Focused semantic test, app build, UI check, architecture/style audit, commit.

## Verification and review

All seven current sax presets have Brass as their primary tag; Kicker and
Stomper have Bass. The five legacy files under `presets/old` do not have Cycle V2
presentation metadata and are not indexed by the browser. The tagging script
reports nine updated current presets.

The standalone-debug app and tests build. Focused browser and tag tests pass
198 assertions in 14 test cases. They cover tag persistence, preservation of
unrelated JSON fields, the nine saved taxonomy corrections, per-record search
refresh, and the loaded-document metadata command. A real Cycle V2 window
screenshot at `/tmp/cycle-v2-expanded-tag-edit.png` confirms the Edit Tags
button is visible and aligned beside Load Preset. The agent fixture opened
Kicker and the expanded browser successfully. `git diff --check` passes.

`PresetTagStore` owns on-disk normalization and atomic writes;
`PresetLibraryIndex` alone re-reads one changed record and rebuilds search
results. Both browsers consume index callbacks. `MainWindow` only routes the
changed file and tags to the workspace; the dispatcher owns the in-memory
metadata change. `NodeCanvas.cpp` remains a large existing orchestration file
(2,841 lines), but this change adds only a narrow command call and record
refresh forwarder. The architecture audit reports the same 21 existing size
triggers, with no new trigger.
