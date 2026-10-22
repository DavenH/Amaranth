#include "UI/Editors/MeshEditorLayout.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/MorphControlLayout.h"

namespace CycleV2 {

namespace {

constexpr float kPanelGap = 8.f;
constexpr float kTopRowRatio = 0.54f;

}

MeshEditorLayout::MeshEditorLayout(juce::Rectangle<float> area, int count) :
        content        (area)
    ,   dimensionCount (count) {}

juce::Rectangle<float> MeshEditorLayout::controls() const {
    const auto top = content.withHeight(content.getHeight() * (dimensionCount == 0 ? 1.f : kTopRowRatio));
    const float width = juce::jmax(top.getWidth() * 0.5f - kPanelGap, MorphControlLayout::minimumWidth());
    return top.withLeft(top.getRight() - width);
}

juce::Rectangle<float> MeshEditorLayout::surface() const {
    return dimensionCount == 0 ? juce::Rectangle<float>()
            : content.withHeight(content.getHeight() * kTopRowRatio).withRight(controls().getX() - kPanelGap);
}

juce::Rectangle<float> MeshEditorLayout::curve() const {
    if (dimensionCount == 0) {
        const auto properties = MorphControlLayout(controls().withTrimmedBottom(controlFooterHeight), 0).vertices();
        return content.withRight(properties.getX() - kPanelGap);
    }
    return content.withTrimmedTop(content.getHeight() * kTopRowRatio + kPanelGap);
}

juce::Rectangle<float> MeshEditorLayout::staticEditorBounds(juce::Rectangle<float> fullBounds) {
    const float chromeHeight = CanvasChromeMetrics::embeddedEditorHeaderHeight + 16.f;
    return fullBounds.withHeight(chromeHeight + (fullBounds.getHeight() - chromeHeight) * kTopRowRatio);
}

}
