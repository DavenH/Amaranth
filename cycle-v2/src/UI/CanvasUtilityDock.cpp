#include "UI/CanvasUtilityDock.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

CanvasUtilityDockLayout CanvasUtilityDock::layout(
        juce::Rectangle<float> contentBounds) {
    CanvasUtilityDockLayout result;
    if (contentBounds.isEmpty()) {
        return result;
    }

    const float availableWidth = juce::jmax(0.f, contentBounds.getWidth() - margin * 2.f);
    const float utilityWidth = juce::jmin(preferredMinimapWidth, availableWidth);
    const float right = contentBounds.getRight() - margin;
    result.keyboard = contentBounds.withHeight(
            juce::jmin(preferredKeyboardHeight, contentBounds.getHeight()));
    const float minimapTop = result.keyboard.getBottom() + margin;
    const float minimapHeight = 92.f;
    result.minimap = {
            right - utilityWidth,
            minimapTop,
            utilityWidth,
            juce::jmin(minimapHeight,
                    juce::jmax(0.f, contentBounds.getBottom() - margin - minimapTop))
    };
    const float legendRight = right - utilityWidth - gap;
    const float legendWidth = juce::jmin(
            preferredLegendWidth,
            juce::jmax(0.f, legendRight - contentBounds.getX() - margin));
    result.legend = {
            legendRight - legendWidth,
            contentBounds.getBottom() - margin - preferredLegendHeight,
            legendWidth,
            juce::jmin(preferredLegendHeight,
                    juce::jmax(0.f, contentBounds.getHeight() - margin * 2.f))
    };

    const float statusLeft = contentBounds.getX() + margin;
    const float statusRight = result.minimap.getX() - gap;
    const float statusWidth = juce::jmin(560.f, juce::jmax(0.f, statusRight - statusLeft));
    const float statusTop = result.keyboard.getBottom() + margin;
    result.status = {
            statusLeft,
            statusTop,
            statusWidth,
            juce::jmin(preferredStatusHeight,
                    juce::jmax(0.f, contentBounds.getBottom() - margin - statusTop))
    };
    return result;
}

void CanvasUtilityDock::paintSurface(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds) {
    if (bounds.isEmpty()) {
        return;
    }
    graphics.setColour(CanvasChromePalette::insetBackground.withAlpha(0.87f));
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::panelCornerRadius);
}

}
