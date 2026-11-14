# Dialog styling

Status: Complete.

## Contract

The movie exporter and existing preset-save controls are visual references.
Preserve the movie exporter exactly as it was before 21215885. Match its
square-edged AmaranthLookAndFeel controls in routine pattern-copy and preset
save/rename/tag prompts. Do not replace the references with a new style.

DialogLookAndFeel derives directly from AmaranthLookAndFeel, with default
installation disabled. It inherits existing button/text-editor drawing, colours,
dimensions and focus borders. Alerts use an 18px heading, 13px body and the
app’s dark teal dock surface. The enabled button holding keyboard focus receives
a blue interior fill; unfocused and disabled buttons retain the inherited style.
No browser-derived rounded controls or fixed blue primary treatment.

StyledAlertWindow retains owned/detached theme lifetime. Existing JUCE layout,
modal callbacks, save/discard/cancel shortcuts and name validation remain owners
of behavior. Preserve all user preset edits and the preset-browser theme.

## Verification

Compare exporter source/header byte for byte against 21215885's parent. Build
Debug app/tests and Release app. Capture corrected pattern and restored exporter
at production size; exercise native blank-name validation, cancel/save/discard
and export cancellation. Run existing draft/movie checks, architecture audit,
code map and whitespace review. Commit the correction.


## Outcome

Exporter source/header and preset-browser theme declaration exactly match the
parent of 21215885. DialogLookAndFeel now only specializes alert typography and
icon-free text presentation; all field/button painting is inherited from the
reference AmaranthLookAndFeel. No references or library controls are restyled.

Corrected native captures: .context/dialog-style/pattern-copy-corrected.png,
pattern-blank-corrected.png, movie-restored.png and movie-restored-running.png.
The native workflow verifies blank-name Return cannot save, Escape cancellation,
copy save, Discard restoring the saved phrase, original 28px exporter action
alignment with 24px opposing insets, and export cancellation. Evidence:
.context/dialog-style/correction.log. Existing movie/draft checks pass; Debug
app/tests and Release app build. Code-map, whitespace and architecture audit pass.

Responsibility review: DialogLookAndFeel shrinks 108->43 source lines and
36->27 header lines. Removed browser-control inheritance, custom primary-button
styling, focus painting and duplicated dialog progress/window painting. The
exporter regains its original private theme. No callback, graph, persistence,
audio or user preset behavior changes.


## Focus-colour polish

Restore the requested dark teal alert background and blue fill on the currently
focused, enabled button. Keep inherited square borders/fields and movie exporter
unchanged. The dialog override delegates existing button painting then fills its
interior for focus; it does not copy the reference renderer or alter callbacks.
Verify the focused Save rendering in native screenshots; build Debug/Release
and repeat whitespace, code-map and architecture checks.


Outcome: Debug/Release builds and code-map/whitespace/architecture checks pass.
The native pattern prompt shows the dark teal background and square focused
Save button filled blue (.context/dialog-style/pattern-teal-save-focus.png).
The movie exporter still matches its pre-restyling files exactly. The theme grows
43->53 source lines and 27->29 header lines, retaining inherited control painting;
only the focus interior is specialized. No new tests for this cosmetic change.
