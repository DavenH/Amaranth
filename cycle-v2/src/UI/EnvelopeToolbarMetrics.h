#pragma once

namespace CycleV2::EnvelopeToolbarMetrics {

inline constexpr float controlWidth = 34.f;
inline constexpr float controlHeight = 30.f;
inline constexpr float purposeIconCanvasSize = 24.f;
inline constexpr float purposeIconHorizontalMargin
        = (controlWidth - purposeIconCanvasSize) * 0.5f;
inline constexpr float purposeIconVerticalMargin
        = (controlHeight - purposeIconCanvasSize) * 0.5f;

inline constexpr float actionIconCanvasSize = 21.f;
inline constexpr float actionIconHorizontalMargin
        = (controlWidth - actionIconCanvasSize) * 0.5f;
inline constexpr float actionIconVerticalMargin
        = (controlHeight - actionIconCanvasSize) * 0.5f;

inline constexpr float svgViewBoxSize = 48.f;
inline constexpr float svgLiveAreaInset = 6.f;
inline constexpr float svgLiveAreaSize = svgViewBoxSize - 2.f * svgLiveAreaInset;

inline constexpr float diagramControlWidth = 42.f;
inline constexpr float diagramCanvasWidth = 24.f;
inline constexpr float diagramCanvasHeight = 18.f;
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
