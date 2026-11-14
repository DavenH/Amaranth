# Dialog styling

Status: Complete.

## Contract

The movie exporter and existing preset-save controls are visual references.
Preserve the movie exporter exactly as it was before 21215885. Match its
square-edged AmaranthLookAndFeel controls in routine pattern-copy and preset
save/rename/tag prompts. Do not replace the references with a new style.

DialogLookAndFeel derives directly from AmaranthLookAndFeel, with default
installation disabled. It inherits existing button/text-editor drawing, colours,
keyboard focus and dimensions. Only alert typography/background and icon-free
text painting are specialized: 18px heading, 13px body, exporter grey background.
No browser-derived rounded controls or custom blue primary treatment.

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
