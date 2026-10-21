#include "UI/Editors/PropertyControlLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kTrack { 0xff384351 };
const Colour kTrackHover { 0xff465363 };
const Colour kRail { 0xff080b10 };
const Colour kFill { 0xffdce3ec };

struct SliderPaintGeometry {
    Rectangle<float> bounds;
    Rectangle<float> track;
    Rectangle<float> indicator;
    float indicatorX {};
};

SliderPaintGeometry sliderPaintGeometry(
        Rectangle<float> bounds,
        Rectangle<float> track,
        float sliderPosition) {
    SliderPaintGeometry result;
    result.bounds = bounds;
    result.track = track;
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
        Colour fill,
        bool hovered,
        bool enabled) {
    graphics.setColour(kRail.withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.fillRoundedRectangle(geometry.track.expanded(
            (PropertyControlMetrics::visibleRailHeight - geometry.track.getHeight()) * 0.5f), 2.5f);
    graphics.setColour((hovered ? kTrackHover : kTrack).withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.fillRoundedRectangle(geometry.track, 1.f);
    graphics.setColour(fill.withMultipliedAlpha(enabled ? 0.78f : 0.35f));
    graphics.fillRoundedRectangle(geometry.track.withRight(geometry.indicatorX), 1.f);
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
    Slider::SliderLayout getSliderLayout(Slider& slider) override {
        Slider::SliderLayout layout = LookAndFeel_V4::getSliderLayout(slider);
        const auto* precision = dynamic_cast<const PrecisionSlider*>(&slider);
        if (slider.isHorizontal() && precision != nullptr) {
            layout.sliderBounds = slider.getLocalBounds().reduced(
                    roundToInt(precision->getTrackEndInset()),
                    0);
        }
        return layout;
    }

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
        const Rectangle<float> bounds {
                (float) x,
                (float) y,
                (float) width,
                (float) height
        };
        const SliderPaintGeometry geometry = sliderPaintGeometry(
                bounds,
                bounds.withSizeKeepingCentre(
                        bounds.getWidth(),
                        PropertyControlMetrics::visibleTrackHeight),
                sliderPosition);
        const bool enabled = slider.isEnabled();
        const bool hovered = slider.isMouseOverOrDragging();
        const bool focused = slider.hasKeyboardFocus(false);

        const Colour fill = slider.isColourSpecified(Slider::trackColourId)
                ? slider.findColour(Slider::trackColourId) : kFill;
        paintSliderTrack(graphics, geometry, fill, hovered, enabled);
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

float propertySliderValuePosition(Slider& slider, double value) {
    const Rectangle<float> sliderBounds = slider.getLookAndFeel()
            .getSliderLayout(slider)
            .sliderBounds
            .toFloat();
    const double clampedValue = jlimit(
            slider.getMinimum(),
            slider.getMaximum(),
            value);
    return jmap(
            (float) slider.valueToProportionOfLength(clampedValue),
            sliderBounds.getX(),
            sliderBounds.getRight());
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

Rectangle<float> morphKeyboardFocusOutlineBounds(
        Rectangle<float> rowBounds,
        Rectangle<float> trailingControlBounds) {
    return Rectangle<float>::leftTopRightBottom(
            rowBounds.getX() + 1.f,
            rowBounds.getY() + 1.f,
            trailingControlBounds.getRight() + 5.f,
            rowBounds.getBottom() - 1.f);
}

Rectangle<float> keyboardFocusIndicatorBounds(Rectangle<float> focusOutlineBounds) {
    return Rectangle<float>(18.f, 13.f).withCentre({
            focusOutlineBounds.getRight() + 13.f,
            focusOutlineBounds.getCentreY()
    });
}

Rectangle<float> keyboardFocusIndicatorHitBounds(Rectangle<float> focusOutlineBounds) {
    return keyboardFocusIndicatorBounds(focusOutlineBounds)
            .withSizeKeepingCentre(24.f, 24.f);
}

void paintKeyboardFocusIndicator(
        Graphics& graphics,
        Rectangle<float> focusOutlineBounds) {
    const Rectangle<float> icon = keyboardFocusIndicatorBounds(focusOutlineBounds);
    graphics.setColour(Colour(0xff111820).withAlpha(0.96f));
    graphics.fillRoundedRectangle(icon, 3.f);

    const Colour glyph = propertyControlFocusColour().withAlpha(0.96f);
    graphics.setColour(glyph);
    graphics.drawRoundedRectangle(icon.reduced(0.6f), 3.f, 1.2f);

    constexpr float keyDiameter = 1.5f;
    constexpr float horizontalInset = 3.1f;
    const float keySpacing = (icon.getWidth() - 2.f * horizontalInset - keyDiameter) / 4.f;
    for (int row = 0; row < 2; ++row) {
        const float y = icon.getY() + 3.25f + (float) row * 3.f;
        for (int column = 0; column < 5; ++column) {
            const float x = icon.getX() + horizontalInset + (float) column * keySpacing;
            graphics.fillEllipse(x, y, keyDiameter, keyDiameter);
        }
    }

    graphics.fillRoundedRectangle(
            icon.withSizeKeepingCentre(8.f, 1.5f)
                    .withY(icon.getBottom() - 3.1f),
            0.75f);
}

void paintMorphKeyboardFocus(
        Graphics& graphics,
        Rectangle<float> focusOutlineBounds) {
    graphics.setColour(Colour(0xff8793a1).withAlpha(0.42f));
    graphics.drawRoundedRectangle(
            focusOutlineBounds,
            CanvasChromeMetrics::controlCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
    paintKeyboardFocusIndicator(graphics, focusOutlineBounds);
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
            bounds,
            track,
            position);
    paintSliderTrack(graphics, geometry, fill, hovered, enabled);
    paintSliderIndicator(graphics, geometry, focused, enabled);
}

void paintMorphSlider(
        Graphics& graphics,
        Rectangle<float> bounds,
        double normalizedValue,
        Colour accent,
        bool hovered,
        bool enabled) {
    const Rectangle<float> track = bounds.withSizeKeepingCentre(
            bounds.getWidth(),
            PropertyControlMetrics::visibleTrackHeight);
    const Rectangle<float> marker = morphSliderIndicatorBounds(bounds, normalizedValue);
    const float alpha = enabled ? 1.f : 0.5f;

    paintSliderTrack(graphics, sliderPaintGeometry(bounds, track, marker.getCentreX()),
            accent, hovered, enabled);
    graphics.setColour(accent.withAlpha(0.98f * alpha));
    graphics.fillRect(marker);
}

LookAndFeel& propertyControlLookAndFeel() {
    static PropertyControlLookAndFeel result;
    return result;
}

void paintPanelOverlayButton(Graphics& graphics, Rectangle<float> bounds,
        bool hovered, bool down, bool focused, bool selected, bool enabled) {
    bounds = bounds.reduced(0.75f);
    const Colour accent { 0xff65b8ff };
    graphics.setColour(selected ? Colour(0xff243d52) : Colour(0xff0e1318).brighter(
            down ? 0.12f : hovered ? 0.06f : 0.f));
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour((focused || selected ? accent : Colour(0xff536171))
            .withMultipliedAlpha(enabled ? 1.f : 0.55f));
    graphics.drawRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius,
            focused ? CanvasChromeMetrics::focusRingWidth : CanvasChromeMetrics::restingBorderWidth);
}

void paintMorphPreviewVertex(Graphics& graphics, Point<float> point, bool selected) {
    graphics.setColour(Colours::black);
    graphics.fillEllipse(Rectangle<float>(5.f, 5.f).withCentre(point));
    graphics.setColour(selected ? Colour(0xffdf9292) : Colour(0xffc3cbd5));
    graphics.fillEllipse(Rectangle<float>(selected ? 3.f : 2.f, selected ? 3.f : 2.f).withCentre(point));
}

void paintMorphPreviewCursor(Graphics& graphics, Point<float> point) {
    Path diamond;
    diamond.startNewSubPath(point.x, point.y - 4.f);
    diamond.lineTo(point.x + 4.f, point.y);
    diamond.lineTo(point.x, point.y + 4.f);
    diamond.lineTo(point.x - 4.f, point.y);
    diamond.closeSubPath();
    graphics.setColour(Colours::black);
    graphics.fillPath(diamond);
    graphics.setColour(Colours::white);
    graphics.strokePath(diamond, PathStrokeType(1.f));
}


}
