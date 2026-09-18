#include "UI/CanvasUtilityDock.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

CanvasUtilityDockLayout CanvasUtilityDock::layout(juce::Rectangle<float> contentBounds) {
    CanvasUtilityDockLayout result;
    if (contentBounds.isEmpty()) {
        return result;
    }

    const float availableWidth = juce::jmax(0.f, contentBounds.getWidth() - margin * 2.f);
    const float utilityWidth = juce::jmin(154.f, availableWidth);
    const float right = contentBounds.getRight() - margin;
    const float keyboardWidth = juce::jmin(preferredKeyboardWidth, availableWidth);
    const float centredKeyboardX = contentBounds.getCentreX() - keyboardWidth * 0.5f;
    result.keyboard = {
            centredKeyboardX,
            contentBounds.getY(),
            keyboardWidth,
            juce::jmin(preferredKeyboardHeight, contentBounds.getHeight())
    };
    const bool sharedTopRow = result.keyboard.getRight() + gap
            > right - utilityWidth;
    const float minimapTop = sharedTopRow
            ? result.keyboard.getBottom() + gap
            : contentBounds.getY() + margin;
    const float minimapHeight = sharedTopRow
            ? juce::jlimit(50.f, 92.f, contentBounds.getHeight() * 0.15f)
            : 92.f;
    result.minimap = {
            right - utilityWidth,
            minimapTop,
            utilityWidth,
            juce::jmin(minimapHeight,
                    juce::jmax(0.f, contentBounds.getBottom() - margin - minimapTop))
    };
    result.legend = {
            result.minimap.getX(),
            result.minimap.getBottom() + gap,
            utilityWidth,
            juce::jmin(sharedTopRow ? 0.f : preferredLegendHeight,
                    juce::jmax(0.f, contentBounds.getBottom() - margin
                            - result.minimap.getBottom() - gap))
    };

    const float statusLeft = contentBounds.getX() + margin;
    const float statusRight = sharedTopRow
            ? result.minimap.getX() - gap
            : juce::jmin(result.minimap.getX(), result.keyboard.getX()) - gap;
    const float statusWidth = juce::jmin(
            560.f,
            juce::jmax(0.f, statusRight - statusLeft));
    result.status = {
            statusLeft,
            sharedTopRow ? result.keyboard.getBottom() + gap
                    : contentBounds.getY() + margin,
            statusWidth,
            30.f
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
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::panelCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
}

}
