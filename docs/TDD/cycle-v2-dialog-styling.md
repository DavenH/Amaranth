# Dialog styling

Status: Complete.

## Contract

Unify the pattern-copy prompt and movie exporter with the Cycle 2 canvas/preset
controls. Reuse PresetBrowserLookAndFeel's button and text-field painting through
a dialog-specific subclass, rather than copying widget drawing or altering
Cycle 1's shared AmaranthLookAndFeel. Use CanvasChromePalette and metrics.

Dialogs use an 18px heading, 13px message/field text, 32px action buttons and
4px control radii. Remove decorative stock question icons from routine save,
rename and tag prompts. Use one blue primary action, readable disabled controls,
boxed text editors and a flat progress track. Preserve existing JUCE alert
layout, modal callbacks, keyboard shortcuts, validation and export worker state.
Alert windows own/detach their theme so asynchronous dismissal cannot outlive it.

The exporter retains its 24px content insets and 12px footer action gap; controls
and progress share the same left/right content edges. The form's content,
progress and action bands retain their semantic scope. No global button redesign
or graph/audio changes. Preserve all incoming preset modifications.

## Verification

Build exact Debug app/tests and Release app. Exercise pattern-copy Save, Cancel,
Discard and blank-name validation using existing UI routes with isolated library
storage; review actual-size prompt and exporter screenshots, including disabled
export state. Run existing relevant pattern/export checks. Review size and
responsibility, architecture audit, code-map and whitespace; commit the slice.


## Outcome and evidence

Pattern copy, preset replacement, rename/tags and movie export share the new
DialogLookAndFeel. StyledAlertWindow owns/detaches its theme; existing JUCE
modal state, text layout and callbacks remain the behavior owners. The exporter
loses its private look-and-feel and reuses its existing flat progress painter
through the shared theme. Cycle 1 look-and-feel is unchanged.

Native screenshots: .context/dialog-style/pattern-copy.png, pattern-blank.png,
movie-settings.png and movie-running.png. The pattern prompt is 373x186 logical
pixels, with 18px title/13px message, boxed field and 32px actions. Movie settings
retain 620x530 window size. Native assertions verify the footer's 32px common
button height/baseline and balanced 24px left/right insets.

The isolated native workflow verifies blank names cannot save with Return,
Escape retains the draft, Save creates a user copy, Discard restores the saved
phrase, export disables its settings and Cancel returns to editable settings.
Evidence: .context/dialog-style/final-review.log and review.py. An optional AX
button lookup returned no result; blank-name behavior was then verified through
native Return plus unchanged modal/pattern state, not the missing AX query.

Existing movie settings, library validation and MIDI draft-save checks pass.
Debug app/tests and Release app build. Code-map, whitespace and architecture
audit complete. No new unit tests for the visual-only change; actual rendering
and existing behavioral checks provide the relevant verification.

Responsibility review: shared dialog presentation occupies a 108-line source
and 36-line header, reusing browser control painting. SpyMovieExportWindow
shrinks 235->208 lines and loses its private theme. Main remains 670 lines and
only constructs the themed alert; NodeWorkspacePatterns and PresetMetadataEditor
retain their existing persistence/callback policies. No domain behavior or
realtime work moves into the presentation layer. Incoming presets remain unstaged.
