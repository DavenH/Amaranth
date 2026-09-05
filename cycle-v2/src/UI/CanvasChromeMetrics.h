#pragma once

namespace CycleV2::CanvasChromeMetrics {

inline constexpr float microCornerRadius = 2.f;
inline constexpr float insetCornerRadius = 3.f;
inline constexpr float controlCornerRadius = 4.f;
inline constexpr float tileCornerRadius = 5.f;
inline constexpr float panelCornerRadius = 6.f;

inline constexpr float restingBorderWidth = 1.f;
inline constexpr float activeBorderWidth = 1.5f;
inline constexpr float focusRingWidth = 2.f;

inline constexpr float microFontSize = 9.f;
inline constexpr float captionFontSize = 10.5f;
inline constexpr float labelFontSize = 12.f;
inline constexpr float sectionTitleFontSize = 14.f;
inline constexpr float editorTitleFontSize = 18.f;

inline constexpr float legendScale = 1.3f;
inline constexpr float legendFontSize = microFontSize * legendScale;
inline constexpr float legendHorizontalInset = 12.f * legendScale;
inline constexpr float legendTopInset = 17.f * legendScale;
inline constexpr float legendLineLength = 17.f * legendScale;
inline constexpr float legendLineWidth = 2.f * legendScale;
inline constexpr float legendTextGap = 7.f * legendScale;
inline constexpr float legendTextWidth = 76.f * legendScale;
inline constexpr float legendTextHeight = 20.f * legendScale;
inline constexpr float legendRowStride = 20.f * legendScale;

inline constexpr int fullEditorHeaderHeight = 44;
inline constexpr int fullEditorHorizontalInset = 18;
inline constexpr int fullEditorTitleVerticalInset = 8;
inline constexpr int fullEditorActionGap = 8;
inline constexpr int fullEditorCloseButtonSize = 28;
inline constexpr int fullEditorCloseButtonRightInset = 14;
inline constexpr int fullEditorEnabledWidth = 28;
inline constexpr int fullEditorEnabledHeight = 28;

inline constexpr float embeddedEditorHeaderHeight = 34.f;
inline constexpr float embeddedEditorHorizontalInset = 13.f;
inline constexpr float embeddedEditorTitleVerticalInset = 4.f;
inline constexpr float embeddedEditorActionGap = 8.f;
inline constexpr float embeddedEditorCloseButtonSize = 22.f;
inline constexpr float embeddedEditorCloseButtonRightInset = 11.f;

}
