#include "NodePortSocketRenderer.h"

#include "NodePortGeometry.h"

namespace CycleV2 {

namespace {

const juce::Colour kCanvasBackground { 0xff101318 };

}

void NodePortSocketRenderer::paint(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        juce::Colour colour,
        bool input) {
    const float scale = bounds.getWidth() / NodePortGeometry::socketDiameter;
    graphics.setColour(colour.withAlpha(0.22f));
    graphics.fillEllipse(bounds.expanded(1.4f * scale));
    if (input) {
        graphics.setColour(colour);
        graphics.fillEllipse(bounds);
        return;
    }

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(bounds);
    graphics.setColour(colour);
    graphics.drawEllipse(bounds, 1.2f * scale);
}

}
