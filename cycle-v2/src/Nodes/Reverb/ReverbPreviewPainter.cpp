#include "Nodes/Reverb/ReverbPreviewPainter.h"

#include "Graph/NodeParameterMap.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

void ReverbPreviewPainter::paint(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) const {
    const Rectangle<float> content = area.reduced(
            jmin(area.getWidth(), area.getHeight()) * 0.12f);
    const NodeParameterMap parameters(node);
    const float size = parameters.floatValue("size", 0.5f);
    const float damping = parameters.floatValue("damp", 0.2f);
    const float width = parameters.floatValue("width", 1.f);
    const float wet = parameters.floatValue("wet", 0.4f);
    const bool enabled = parameters.boolValue("enabled", true);
    const int reflectionCount = 5 + roundToInt(size * 6.f);
    float amplitude = 0.88f;
    const float decay = 0.48f + size * 0.36f - damping * 0.12f;
    const auto colour = [enabled](Colour value) {
        return EffectPlotPalette::forEnabledState(value, enabled);
    };

    graphics.setColour(colour(EffectPlotPalette::background));
    graphics.fillRoundedRectangle(content, 4.f);
    for (int index = 0; index < reflectionCount; ++index) {
        const float unit = (float) index / (float) jmax(1, reflectionCount - 1);
        const float x = content.getX() + unit * content.getWidth();
        const float height = content.getHeight() * amplitude * (0.36f + wet * 0.5f);
        const float spread = height * (0.35f + width * 0.65f);
        graphics.setColour(colour(
                EffectPlotPalette::accent.withAlpha(0.28f + amplitude * 0.62f)));
        graphics.drawLine(
                x,
                content.getCentreY() - spread,
                x,
                content.getCentreY() + spread,
                jmax(1.f, 1.5f * zoom));
        amplitude *= decay;
    }
}

}
