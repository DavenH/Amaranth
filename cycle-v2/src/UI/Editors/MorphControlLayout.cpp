#include "UI/Editors/MorphControlLayout.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

MorphControlLayout::MorphControlLayout(juce::Rectangle<float> bounds, int count, MorphDimensions::Plane plane) :
        area       (bounds.reduced(8.f))
    ,   dimensions (count, plane) {}

juce::Rectangle<float> MorphControlLayout::vertices() const {
    return area.withTrimmedLeft(area.getWidth() * 0.52f);
}

juce::Rectangle<float> MorphControlLayout::morphColumn() const {
    return area.withWidth(area.getWidth() * 0.48f);
}

float MorphControlLayout::diagramSize() const {
    return juce::jmin(140.f, morphColumn().getWidth(), juce::jmax(70.f, area.getHeight() - 132.f));
}

juce::Rectangle<float> MorphControlLayout::diagram() const {
    if (dimensions.axisCount() == 0) {
        return {};
    }
    const auto column = morphColumn();
    const float size = diagramSize();
    return { column.getCentreX() - size * 0.5f, column.getY(), size, size };
}

juce::Rectangle<float> MorphControlLayout::row(int axisIndex) const {
    if (!dimensions.active(MorphDimensions::axes[(size_t) axisIndex])) {
        return {};
    }
    auto column = morphColumn();
    const float top = column.getY() + diagramSize() + 12.f;
    return { column.getX(),
            top + axisIndex * (PropertyControlMetrics::rowHeight + PropertyControlMetrics::rowGap),
            column.getWidth(), (float) PropertyControlMetrics::rowHeight };
}

juce::Rectangle<float> MorphControlLayout::rail(int axisIndex) const {
    auto bounds = row(axisIndex);
    return bounds.isEmpty() ? bounds : bounds.withTrimmedLeft(42.f).withTrimmedRight(
            56.f + PropertyControlMetrics::morphKeyboardFocusSlotWidth);
}

juce::Rectangle<float> MorphControlLayout::axis(int axisIndex) const {
    const auto bounds = row(axisIndex);
    if (bounds.isEmpty() || dimensions.axisCount() <= 1) {
        return {};
    }
    return { bounds.getRight() - 52.f - PropertyControlMetrics::morphKeyboardFocusSlotWidth,
            bounds.getCentreY() - 10.f, 20.f, 20.f };
}

juce::Rectangle<float> MorphControlLayout::link(int axisIndex) const {
    const auto bounds = row(axisIndex);
    if (bounds.isEmpty()) {
        return {};
    }
    return { bounds.getRight() - 24.f - PropertyControlMetrics::morphKeyboardFocusSlotWidth,
            bounds.getCentreY() - 10.f, 20.f, 20.f };
}

}
