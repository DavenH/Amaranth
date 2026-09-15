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
    result.minimap = {
            right - utilityWidth,
            contentBounds.getY() + margin,
            utilityWidth,
            juce::jmin(92.f, juce::jmax(0.f, contentBounds.getHeight() - margin * 2.f))
    };
    result.legend = {
            result.minimap.getX(),
            result.minimap.getBottom() + gap,
            utilityWidth,
            preferredLegendHeight
    };

    const float keyboardWidth = juce::jmin(preferredKeyboardWidth, availableWidth);
    const float keyboardBottom = contentBounds.getBottom() - margin;
    const float compactKeyboardTop =
            result.legend.getY() + minimumCompactLegendHeight + gap;
    const float centredKeyboardX = contentBounds.getCentreX() - keyboardWidth * 0.5f;
    const juce::Rectangle<float> preferredKeyboard {
            centredKeyboardX,
            contentBounds.getY() + margin,
            keyboardWidth,
            preferredKeyboardHeight
    };
    const juce::Rectangle<float> spacedKeyboard = preferredKeyboard.expanded(gap);
    const bool overlapsRightUtilities = spacedKeyboard.intersects(result.minimap)
            || spacedKeyboard.intersects(result.legend);
    const float keyboardTop = overlapsRightUtilities
            ? compactKeyboardTop
            : preferredKeyboard.getY();
    const float availableKeyboardHeight = juce::jmax(0.f, keyboardBottom - keyboardTop);
    result.keyboard = {
            centredKeyboardX,
            keyboardTop,
            keyboardWidth,
            juce::jmin(preferredKeyboardHeight, availableKeyboardHeight)
    };

    const float statusLeft = contentBounds.getX() + margin;
    const float statusRight = overlapsRightUtilities
            ? result.minimap.getX() - gap
            : juce::jmin(result.minimap.getX(), result.keyboard.getX()) - gap;
    const float statusWidth = juce::jmin(
            560.f,
            juce::jmax(0.f, statusRight - statusLeft));
    result.status = {
            statusLeft,
            contentBounds.getY() + margin,
            statusWidth,
            30.f
    };
    if (result.legend.intersects(result.keyboard.expanded(gap))) {
        result.legend.setHeight(juce::jmax(0.f, result.keyboard.getY() - gap - result.legend.getY()));
    }
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
