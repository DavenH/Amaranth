#include "UI/Editors/MorphControlLayout.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

namespace {

constexpr float kInset = 8.f;
constexpr float kMorphLabelWidth = 42.f;
constexpr float kMorphControlsWidth = 56.f + PropertyControlMetrics::morphKeyboardFocusSlotWidth;

}

MorphControlLayout::MorphControlLayout(juce::Rectangle<float> bounds, int count, MorphDimensions::Plane plane) :
        area       (bounds.reduced(kInset))
    ,   dimensions (count, plane) {}

juce::Rectangle<float> MorphControlLayout::vertices() const {
    const float width = juce::jlimit(VertexPropertyMetrics::minimumColumnWidth,
            VertexPropertyMetrics::maximumColumnWidth, area.getWidth() * 0.48f);
    return area.withTrimmedLeft(area.getWidth() - width);
}

juce::Rectangle<float> MorphControlLayout::morphColumn() const {
    return area.withRight(vertices().getX() - PropertyControlMetrics::sectionGap);
}

float MorphControlLayout::minimumWidth() {
    return 2.f * kInset + kMorphLabelWidth + PropertyControlMetrics::minimumUsableTrackWidth + kMorphControlsWidth
            + PropertyControlMetrics::sectionGap + VertexPropertyMetrics::minimumColumnWidth;
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
    return bounds.isEmpty() ? bounds : bounds.withTrimmedLeft(kMorphLabelWidth).withTrimmedRight(kMorphControlsWidth);
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
