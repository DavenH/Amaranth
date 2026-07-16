#include "EnvelopeMorphControls.h"

using namespace juce;

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

}

Rectangle<float> EnvelopeMorphControls::squareColumn(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    return controls.removeFromLeft(178.f);
}

Rectangle<float> EnvelopeMorphControls::planeBounds(Rectangle<float> controls) const {
    auto bounds = squareColumn(controls);
    bounds.removeFromTop(23.f);
    const float size = jmin(bounds.getWidth() - 20.f, bounds.getHeight() - 8.f);
    return Rectangle<float>(size, size).withCentre(bounds.getCentre());
}

Rectangle<float> EnvelopeMorphControls::railColumn(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(200.f);
    return controls.removeFromLeft(328.f);
}

Rectangle<float> EnvelopeMorphControls::morphRow(Rectangle<float> controls, int axis) const {
    auto row = railColumn(controls);
    row.removeFromTop(41.f + 35.f * static_cast<float>(axis));
    return row.removeFromTop(32.f);
}

Rectangle<float> EnvelopeMorphControls::axisBounds(Rectangle<float> controls, int axis) const {
    const auto row = morphRow(controls, axis);
    return { row.getRight() - 52.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> EnvelopeMorphControls::linkBounds(Rectangle<float> controls, int axis) const {
    const auto row = morphRow(controls, axis);
    return { row.getRight() - 26.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> EnvelopeMorphControls::vertexBounds(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(546.f);
    return controls;
}

Colour EnvelopeMorphControls::axisColour(int axis) const {
    static const Colour colours[] {
        Colour(0xffd7bf5f),
        Colour(0xffd65a5a),
        Colour(0xff5f91e8)
    };
    return colours[jlimit(0, 2, axis)];
}

void EnvelopeMorphControls::drawPlane(
        Graphics& graphics,
        Rectangle<float> controls,
        float red,
        float blue) const {
    auto column = squareColumn(controls);
    auto header = column.removeFromTop(23.f);
    const auto square = planeBounds(controls);
    const Point<float> cursor {
        square.getX() + square.getWidth() * red,
        square.getBottom() - square.getHeight() * blue
    };
    const auto cursorBounds = Rectangle<float>(8.f, 8.f).withCentre(cursor);

    graphics.setColour(kMutedText);
    graphics.setFont(FontOptions(10.5f, Font::bold));
    graphics.drawText("morph plane", header, Justification::centred);
    graphics.setColour(Colour(0xff5f91e8).withAlpha(0.20f));
    graphics.fillRect(square);
    graphics.setGradientFill(ColourGradient(
            Colour(0x00d65a5a),
            square.getCentreX(),
            square.getY(),
            Colour(0x66d65a5a),
            square.getCentreX(),
            square.getBottom(),
            false));
    graphics.fillRect(square);
    graphics.setColour(kText.withAlpha(0.3f));
    graphics.drawRect(square, 1.f);
    graphics.setColour(Colours::black.withAlpha(0.75f));
    graphics.fillEllipse(cursorBounds);
    graphics.setColour(kText);
    graphics.drawEllipse(cursorBounds, 1.2f);
}

void EnvelopeMorphControls::draw(
        Graphics& graphics,
        Rectangle<float> controls,
        float red,
        float blue,
        int viewAxis,
        bool redLinked,
        bool blueLinked) const {
    const bool linked[] { true, redLinked, blueLinked };
    drawPlane(graphics, controls, red, blue);

    for (int axis = 0; axis < 3; ++axis) {
        const auto axisArea = axisBounds(controls, axis);
        const auto linkArea = linkBounds(controls, axis);
        const auto colour = axisColour(axis);
        graphics.setColour(colour.withAlpha(axis == viewAxis ? 0.45f : 0.06f));
        graphics.fillRoundedRectangle(axisArea, 4.f);
        graphics.setColour(colour.withAlpha(axis == viewAxis ? 1.f : 0.32f));
        graphics.drawRoundedRectangle(axisArea, 4.f, 1.4f);
        graphics.setColour(colour.withAlpha(linked[axis] ? 0.9f : 0.25f));
        graphics.drawRoundedRectangle(linkArea, 4.f, 1.4f);
    }
}

}
