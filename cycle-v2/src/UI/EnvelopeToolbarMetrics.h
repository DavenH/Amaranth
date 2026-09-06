#pragma once

namespace CycleV2::EnvelopeToolbarMetrics {

inline constexpr float controlWidth = 34.f;
inline constexpr float controlHeight = 30.f;
inline constexpr float iconCanvasSize = 24.f;
inline constexpr float iconHorizontalMargin = (controlWidth - iconCanvasSize) * 0.5f;
inline constexpr float iconVerticalMargin = (controlHeight - iconCanvasSize) * 0.5f;

inline constexpr float svgViewBoxSize = 48.f;
inline constexpr float svgLiveAreaInset = 6.f;
inline constexpr float svgLiveAreaSize = svgViewBoxSize - 2.f * svgLiveAreaInset;

inline constexpr float diagramControlWidth = 42.f;
inline constexpr float diagramCanvasWidth = 30.f;
inline constexpr float diagramCanvasHeight = 20.f;
inline constexpr float diagramHorizontalMargin
        = (diagramControlWidth - diagramCanvasWidth) * 0.5f;
inline constexpr float diagramVerticalMargin
        = (controlHeight - diagramCanvasHeight) * 0.5f;

inline constexpr float pairedActionOuterInset = 1.f;
inline constexpr float pairedActionGap = 2.f;
inline constexpr float pairedActionWidth
        = 2.f * controlWidth + 2.f * pairedActionOuterInset + pairedActionGap;
inline constexpr float purposeSelectorWidth = 4.f * controlWidth;
inline constexpr float scalingSelectorWidth = 2.f * diagramControlWidth;

}
