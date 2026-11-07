#include "UI/CanvasUtilityDock.h"

namespace CycleV2 {

CanvasUtilityDockLayout CanvasUtilityDock::layout(
        juce::Rectangle<float> contentBounds) {
    CanvasUtilityDockLayout result;
    if (contentBounds.isEmpty()) {
        return result;
    }

    result.keyboard = contentBounds.withHeight(
            juce::jmin(preferredKeyboardHeight, contentBounds.getHeight()));
    result.footer = contentBounds.withTop(juce::jmax(result.keyboard.getBottom(),
            contentBounds.getBottom() - footerHeight));
    auto line = result.footer.reduced(margin, 1.f);
    result.legend = line.removeFromRight(juce::jmin(preferredLegendWidth, line.getWidth() * 0.6f));
    line.removeFromRight(gap);
    result.status = line;
    return result;
}

}
