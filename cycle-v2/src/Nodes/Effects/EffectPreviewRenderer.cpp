#include "EffectPreviewRenderer.h"

#include "../../UI/NodeParameterValue.h"

#include <Audio/CycleDsp/CycleDelay.h>

namespace CycleV2 {

namespace {

float parameterValue(const Node& node, const String& id, float fallback) {
    return nodeParameterValue(node, id, String(fallback)).getFloatValue();
}

Rectangle<float> contentArea(Rectangle<float> area) {
    return area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.12f);
}

void paintReverb(Graphics& graphics, Rectangle<float> area, const Node& node, float zoom) {
    const Rectangle<float> content = contentArea(area);
    const float size = parameterValue(node, "size", 0.5f);
    const float damping = parameterValue(node, "damp", 0.2f);
    const float width = parameterValue(node, "width", 1.f);
    const float wet = parameterValue(node, "wet", 0.4f);
    const int reflectionCount = 5 + roundToInt(size * 6.f);
    float amplitude = 0.88f;
    const float decay = 0.48f + size * 0.36f - damping * 0.12f;

    graphics.setColour(Colour(0xff11262a));
    graphics.fillRoundedRectangle(content, 4.f);
    for (int index = 0; index < reflectionCount; ++index) {
        const float unit = (float) index / (float) jmax(1, reflectionCount - 1);
        const float x = content.getX() + unit * content.getWidth();
        const float height = content.getHeight() * amplitude * (0.36f + wet * 0.5f);
        const float spread = height * (0.35f + width * 0.65f);
        graphics.setColour(Colour(0xff43c7d0).withAlpha(0.28f + amplitude * 0.62f));
        graphics.drawLine(
                x,
                content.getCentreY() - spread,
                x,
                content.getCentreY() + spread,
                jmax(1.f, 1.5f * zoom));
        amplitude *= decay;
    }
}

void paintDelayAxes(Graphics& graphics, Rectangle<float> plot, bool showLabels) {
    const Colour axis { 0xff53616d };
    graphics.setColour(axis.withAlpha(0.68f));
    graphics.drawHorizontalLine(roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());
    graphics.drawVerticalLine(roundToInt(plot.getX()), plot.getY(), plot.getBottom());

    if (!showLabels) {
        return;
    }

    graphics.setColour(Colour(0xff8793a1));
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("L", plot.getX() - 18.f, plot.getY() - 5.f, 14.f, 12.f,
            Justification::centredRight);
    graphics.drawText("R", plot.getX() - 18.f, plot.getBottom() - 7.f, 14.f, 12.f,
            Justification::centredRight);
    graphics.drawText("TIME", plot.getRight() - 44.f, plot.getBottom() + 3.f, 44.f, 13.f,
            Justification::centredRight);
}

}

void paintDelayPingPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) {
    const bool showLabels = area.getWidth() >= 260.f && area.getHeight() >= 74.f;
    Rectangle<float> content = contentArea(area);
    if (showLabels) {
        content = content.withTrimmedLeft(18.f).withTrimmedBottom(15.f);
    }
    const float time = parameterValue(node, "time", 0.5f);
    const float feedback = parameterValue(node, "feedback", 0.5f);
    const float spin = parameterValue(node, "spin", 0.5f);
    const int tapCount = CycleDsp::delaySpinIterations(
            parameterValue(node, "spinIters", 0.2f));
    const int visibleTaps = jlimit(2, 9, tapCount + 2);
    float amplitude = 0.9f;
    const float spacing = content.getWidth() * (0.09f + time * 0.075f);

    graphics.setColour(Colour(0xff11262a));
    graphics.fillRoundedRectangle(contentArea(area), 4.f);
    paintDelayAxes(graphics, content, showLabels);
    for (int index = 0; index < visibleTaps; ++index) {
        const float x = jmin(
                content.getRight() - 3.f,
                content.getX() + content.getWidth() * 0.08f + (float) index * spacing);
        const float pan = ((index & 1) == 0 ? -1.f : 1.f) * spin;
        const float y = content.getCentreY() + pan * content.getHeight() * 0.38f;
        const float radius = jmax(1.8f, amplitude * content.getHeight() * 0.11f);
        graphics.setColour(Colour(0xff43c7d0).withAlpha(0.30f + amplitude * 0.62f));
        graphics.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre({ x, y }));
        amplitude *= 0.28f + feedback * 0.66f;
    }
}

bool paintEffectCompactPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) {
    if (node.kind == NodeKind::Reverb) {
        paintReverb(graphics, area, node, zoom);
        return true;
    }

    if (node.kind == NodeKind::Delay) {
        paintDelayPingPreview(graphics, area, node, zoom);
        return true;
    }

    return false;
}

}
