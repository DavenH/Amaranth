#include "UI/Editors/MorphControlLayout.h"

namespace CycleV2 {

MorphControlLayout::MorphControlLayout(juce::Rectangle<float> bounds, int count) :
        area       (bounds.reduced(8.f))
    ,   dimensions (count) {}

juce::Rectangle<float> MorphControlLayout::vertices() const {
    if (dimensions.axisCount() == 0) {
        return area;
    }
    return area.withTrimmedLeft(area.getWidth() * 0.52f);
}

juce::Rectangle<float> MorphControlLayout::morphColumn() const {
    return area.withWidth(area.getWidth() * 0.48f);
}

juce::Rectangle<float> MorphControlLayout::diagram() const {
    if (dimensions.axisCount() < 2) {
        return {};
    }
    const auto column = morphColumn();
    const float size = juce::jmin(110.f, column.getWidth());
    return { column.getCentreX() - size * 0.5f, column.getY(), size, size };
}

juce::Rectangle<float> MorphControlLayout::row(int axisIndex) const {
    if (!dimensions.active(MorphDimensions::axes[(size_t) axisIndex])) {
        return {};
    }
    auto column = morphColumn();
    const int ordinal = dimensions.axisCount() == 2 ? axisIndex - 1 : axisIndex;
    const float top = dimensions.axisCount() >= 2 ? diagram().getBottom() + 12.f : column.getY();
    return { column.getX(), top + ordinal * 36.f, column.getWidth(), 30.f };
}

juce::Rectangle<float> MorphControlLayout::rail(int axisIndex) const {
    auto bounds = row(axisIndex);
    return bounds.isEmpty() ? bounds : bounds.withTrimmedLeft(42.f).withTrimmedRight(
            dimensions.axisCount() > 1 ? 56.f : 28.f);
}

juce::Rectangle<float> MorphControlLayout::axis(int axisIndex) const {
    const auto bounds = row(axisIndex);
    if (bounds.isEmpty() || dimensions.axisCount() <= 1) {
        return {};
    }
    return { bounds.getRight() - 52.f, bounds.getCentreY() - 10.f, 20.f, 20.f };
}

juce::Rectangle<float> MorphControlLayout::link(int axisIndex) const {
    const auto bounds = row(axisIndex);
    if (bounds.isEmpty()) {
        return {};
    }
    return { bounds.getRight() - 24.f, bounds.getCentreY() - 10.f, 20.f, 20.f };
}

}
