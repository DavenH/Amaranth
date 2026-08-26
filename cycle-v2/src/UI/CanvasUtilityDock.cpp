#include "UI/CanvasUtilityDock.h"

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

    const float statusLeft = contentBounds.getX() + margin;
    const float statusWidth = juce::jmin(
            560.f,
            juce::jmax(0.f, result.minimap.getX() - gap - statusLeft));
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
    graphics.setColour(juce::Colour(0xdd11171d));
    graphics.fillRoundedRectangle(bounds, cornerRadius);
    graphics.setColour(juce::Colour(0xff3d4a58));
    graphics.drawRoundedRectangle(bounds, cornerRadius, 1.f);
}

}
