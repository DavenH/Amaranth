#pragma once

#include "UI/CanvasChromeMetrics.h"

namespace CycleV2::EnvelopeToolbarMetrics {

inline constexpr float controlWidth = CanvasChromeMetrics::panelOverlayButtonSize;
inline constexpr float controlHeight = CanvasChromeMetrics::panelOverlayButtonSize;
inline constexpr float actionIconCanvasSize
        = controlWidth - 2.f * CanvasChromeMetrics::panelOverlayIconInset;
inline constexpr float actionIconHorizontalMargin
        = (controlWidth - actionIconCanvasSize) * 0.5f;
inline constexpr float actionIconVerticalMargin
        = (controlHeight - actionIconCanvasSize) * 0.5f;

inline constexpr float svgViewBoxSize = 48.f;
inline constexpr float svgLiveAreaInset = 6.f;
inline constexpr float svgLiveAreaSize = svgViewBoxSize - 2.f * svgLiveAreaInset;

inline constexpr float diagramControlWidth = controlWidth;
inline constexpr float diagramCanvasWidth = actionIconCanvasSize * svgLiveAreaSize / svgViewBoxSize;
inline constexpr float diagramCanvasHeight = diagramCanvasWidth;
inline constexpr float diagramHorizontalMargin
        = (diagramControlWidth - diagramCanvasWidth) * 0.5f;
inline constexpr float diagramVerticalMargin
        = (controlHeight - diagramCanvasHeight) * 0.5f;

inline constexpr float pairedActionGap = CanvasChromeMetrics::panelOverlayButtonGap;
inline constexpr float pairedActionWidth
        = 2.f * controlWidth + pairedActionGap;
inline constexpr float purposeSelectorHeight = 30.f;
inline constexpr float purposeSelectorWidth = 4.f * 70.f;
inline constexpr float scalingSelectorWidth = pairedActionWidth;

}
