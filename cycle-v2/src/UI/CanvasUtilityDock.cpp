#include "CanvasUtilityDock.h"

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
            98.f
    };

    const float keyboardWidth = juce::jmin(420.f, availableWidth);
    const float keyboardHeight = juce::jmin(
            126.f,
            juce::jmax(0.f, contentBounds.getHeight() - margin * 2.f));
    result.keyboard = {
            right - keyboardWidth,
            contentBounds.getBottom() - margin - keyboardHeight,
            keyboardWidth,
            keyboardHeight
    };

    const float statusWidth = juce::jmin(560.f, availableWidth);
    result.status = {
            contentBounds.getX() + margin,
            contentBounds.getBottom() - margin - 24.f,
            statusWidth,
            24.f
    };
    if (result.status.intersects(result.keyboard.expanded(gap))) {
        result.status.setY(result.keyboard.getY() - gap - result.status.getHeight());
    }
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
    graphics.setColour(juce::Colour(0xdd11171d));
    graphics.fillRoundedRectangle(bounds, cornerRadius);
    graphics.setColour(juce::Colour(0xff3d4a58));
    graphics.drawRoundedRectangle(bounds, cornerRadius, 1.f);
}

}
