#include "UI/Editors/PropertyControlLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kTrack { 0xff384351 };
const Colour kTrackHover { 0xff465363 };
const Colour kFill { 0xffdce3ec };
const Colour kFocus { 0xff65b8ff };

struct SliderPaintGeometry {
    Rectangle<float> bounds;
    Rectangle<float> track;
    Rectangle<float> thumb;
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
    result.thumb = {
            result.indicatorX - PropertyControlMetrics::thumbWidth * 0.5f,
            result.track.getCentreY() - PropertyControlMetrics::thumbHeight * 0.5f,
            PropertyControlMetrics::thumbWidth,
            PropertyControlMetrics::thumbHeight
    };
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

void paintSliderThumb(
        Graphics& graphics,
        const SliderPaintGeometry& geometry,
        bool focused,
        bool enabled) {
    graphics.setColour(Colour(0xff0d1116).withMultipliedAlpha(enabled ? 1.f : 0.75f));
    graphics.fillRoundedRectangle(geometry.thumb, PropertyControlMetrics::thumbWidth * 0.5f);
    graphics.setColour(kFill.withMultipliedAlpha(enabled ? 1.f : 0.5f));
    graphics.drawRoundedRectangle(geometry.thumb, PropertyControlMetrics::thumbWidth * 0.5f, 1.25f);
    graphics.setColour((focused ? kFocus : kFill).withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.fillRect(propertySliderIndicatorBounds(geometry.thumb));
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
        paintSliderThumb(graphics, geometry, focused, enabled);

        if (focused) {
            graphics.setColour(kFocus.withAlpha(0.72f));
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

Rectangle<float> propertySliderIndicatorBounds(Rectangle<float> thumbBounds) {
    return Rectangle<float>(
            1.f,
            thumbBounds.getHeight() - 4.f)
            .withCentre(thumbBounds.getCentre());
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
    paintSliderThumb(graphics, geometry, focused, enabled);
}

LookAndFeel& propertyControlLookAndFeel() {
    static PropertyControlLookAndFeel result;
    return result;
}

}
