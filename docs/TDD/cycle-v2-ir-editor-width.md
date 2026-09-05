# Cycle V2 IR Editor Width

Status: Implemented (2026-09-05)

## Objective

Increase the IR modeller's preferred width by 20 percent, from 900 to 1080
pixels, without enlarging its property controls. The full 180-pixel increase
belongs to the editable OpenGL curve region.

## Authoritative Layout

- `NodeViewModuleRegistry` owns the preferred expanded-editor size and the
  existing viewport clamping behavior.
- `ImpulseResponseEditorComponent` owns the content split. Its 348-pixel
  right-hand control rail is already a fixed layout invariant.
- `CurveExpandedEditorComponent` owns the embedded header and OpenGL panel
  hosting. No new painting or alternate panel implementation is required.

The implementation changes only the registered preferred width. The existing
fixed-rail split then transfers the complete width increase to the editor
panel without changing slider geometry.

## Test-First Contract

1. The IR expanded-editor preference is 1080 by 430 pixels when the viewport
   can accommodate it.
2. Resizing an IR editor from 900 to 1080 pixels leaves its control bounds at
   348 pixels wide.
3. The panel bounds gain exactly 180 pixels in width while retaining their
   height and inset rules.
4. The existing expanded-editor placement logic still clamps the wider editor
   to smaller viewports.

## Negative Boundaries

- Do not widen the property rail, slider tracks, value fields, or buttons.
- Do not scale the editor height or its internal vertical spacing.
- Do not add JUCE painting or duplicate OpenGL panel behavior.
- Do not special-case placement outside `NodeViewModuleRegistry`.

## Completion Criteria

- Registry and component geometry regressions pass.
- The focused IR fixture confirms the wider panel at production size.
- Standalone Debug and relevant tests build; `git diff --check`, style review,
  and production-diff review pass before commit.

## Implementation Evidence

- The registered preferred size is now 1080 by 430 pixels.
- Component automation reports a 684-pixel OpenGL panel and the unchanged
  348-pixel control rail at preferred size; the former 900-pixel layout had a
  504-pixel panel, proving the entire 180-pixel increase went to the editor.
- The focused IR fixture passed and its production-size screenshot is
  `/private/tmp/cycle-v2-ir-wide.png`.
