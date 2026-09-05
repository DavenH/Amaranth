#include "UI/Editors/PropertyControlLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kTrack { 0xff384351 };
const Colour kTrackHover { 0xff465363 };
const Colour kFill { 0xffdce3ec };

struct SliderPaintGeometry {
    Rectangle<float> bounds;
    Rectangle<float> track;
    Rectangle<float> indicator;
    float indicatorX {};
};

SliderPaintGeometry sliderPaintGeometry(
        int x,
        int y,
        int width,
        int height,
        float sliderPosition) {
    SliderPaintGeometry result;
    result.bounds = {
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(width),
            static_cast<float>(height)
    };
    result.track = propertySliderTrackBounds(result.bounds);
    result.indicatorX = jlimit(result.track.getX(), result.track.getRight(), sliderPosition);
    const Rectangle<float> indicatorAnchor {
            result.indicatorX - PropertyControlMetrics::thumbWidth * 0.5f,
            result.track.getCentreY() - PropertyControlMetrics::indicatorHeight * 0.5f,
            PropertyControlMetrics::thumbWidth,
            PropertyControlMetrics::indicatorHeight
    };
    result.indicator = propertySliderIndicatorBounds(indicatorAnchor);
    return result;
}

void paintSliderTrack(
        Graphics& graphics,
        const SliderPaintGeometry& geometry,
        bool hovered,
        bool enabled) {
    graphics.setColour((hovered ? kTrackHover : kTrack).withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.fillRoundedRectangle(geometry.track, 2.f);
    graphics.setColour(kFill.withMultipliedAlpha(enabled ? 0.78f : 0.35f));
    graphics.fillRoundedRectangle(geometry.track.withRight(geometry.indicatorX), 2.f);
}

void paintSliderIndicator(
        Graphics& graphics,
        const SliderPaintGeometry& geometry,
        bool focused,
        bool enabled) {
    graphics.setColour((focused ? propertyControlFocusColour() : kFill)
            .withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.fillRect(geometry.indicator);
}

class PropertyControlLookAndFeel final : public LookAndFeel_V4 {
public:
    void drawLinearSlider(
            Graphics& graphics,
            int x,
            int y,
            int width,
            int height,
            float sliderPosition,
            float,
            float,
            Slider::SliderStyle,
            Slider& slider) override {
        const SliderPaintGeometry geometry = sliderPaintGeometry(
                x, y, width, height, sliderPosition);
        const bool enabled = slider.isEnabled();
        const bool hovered = slider.isMouseOverOrDragging();
        const bool focused = slider.hasKeyboardFocus(false);

        paintSliderTrack(graphics, geometry, hovered, enabled);
        paintSliderIndicator(graphics, geometry, focused, enabled);

        if (focused) {
            graphics.setColour(propertyControlFocusColour().withAlpha(0.72f));
            graphics.drawRoundedRectangle(
                    geometry.bounds.reduced(0.75f),
                    CanvasChromeMetrics::controlCornerRadius,
                    CanvasChromeMetrics::focusRingWidth);
        }
    }
};

}

Rectangle<float> propertySliderTrackBounds(Rectangle<float> bounds) {
    return bounds.reduced(PropertyControlMetrics::thumbWidth * 0.5f, 0.f)
            .withSizeKeepingCentre(
                    bounds.getWidth() - PropertyControlMetrics::thumbWidth,
                    PropertyControlMetrics::visibleTrackHeight);
}

Colour propertyControlFocusColour() {
    return Colour(0xff65b8ff);
}

Rectangle<float> propertySliderIndicatorBounds(Rectangle<float> thumbBounds) {
    return Rectangle<float>(
            PropertyControlMetrics::indicatorWidth,
            PropertyControlMetrics::indicatorHeight)
            .withCentre(thumbBounds.getCentre());
}

Rectangle<float> morphSliderIndicatorBounds(
        Rectangle<float> bounds,
        double normalizedValue) {
    const float position = jmap(
            (float) jlimit(0.0, 1.0, normalizedValue),
            bounds.getX(),
            bounds.getRight());
    return Rectangle<float>(
            PropertyControlMetrics::indicatorWidth,
            PropertyControlMetrics::indicatorHeight)
            .withCentre({ position, bounds.getCentreY() });
}

void paintPropertySlider(
        Graphics& graphics,
        Rectangle<float> bounds,
        double normalizedValue,
        Colour fill,
        bool hovered,
        bool focused,
        bool enabled) {
    const Rectangle<float> track = propertySliderTrackBounds(bounds);
    const float position = jmap(
            (float) jlimit(0.0, 1.0, normalizedValue),
            track.getX(),
            track.getRight());
    const SliderPaintGeometry geometry = sliderPaintGeometry(
            roundToInt(bounds.getX()),
            roundToInt(bounds.getY()),
            roundToInt(bounds.getWidth()),
            roundToInt(bounds.getHeight()),
            position);
    paintSliderTrack(graphics, geometry, hovered, enabled);
    if (fill != kFill) {
        graphics.setColour(fill.withMultipliedAlpha(enabled ? 0.78f : 0.35f));
        graphics.fillRoundedRectangle(geometry.track.withRight(geometry.indicatorX), 2.f);
    }
    paintSliderIndicator(graphics, geometry, focused, enabled);
}

void paintMorphSlider(
        Graphics& graphics,
        Rectangle<float> bounds,
        double normalizedValue,
        Colour accent,
        bool hovered,
        bool focused,
        bool enabled) {
    const Rectangle<float> track = bounds.withSizeKeepingCentre(
            bounds.getWidth(),
            PropertyControlMetrics::visibleTrackHeight);
    const Rectangle<float> marker = morphSliderIndicatorBounds(bounds, normalizedValue);
    const float alpha = enabled ? 1.f : 0.5f;

    graphics.setColour((hovered ? kTrackHover : kTrack).withMultipliedAlpha(alpha));
    graphics.fillRoundedRectangle(track, 2.f);
    graphics.setColour(accent.withAlpha(0.76f * alpha));
    graphics.fillRoundedRectangle(track.withRight(marker.getCentreX()), 2.f);
    graphics.setColour(accent.withAlpha(0.98f * alpha));
    graphics.fillRect(marker);

    if (focused) {
        graphics.setColour(propertyControlFocusColour().withAlpha(0.72f * alpha));
        graphics.drawRoundedRectangle(
                bounds.reduced(0.75f),
                CanvasChromeMetrics::controlCornerRadius,
                CanvasChromeMetrics::focusRingWidth);
    }
}

LookAndFeel& propertyControlLookAndFeel() {
    static PropertyControlLookAndFeel result;
    return result;
}

}
