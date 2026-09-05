#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "Nodes/Delay/DelayPreviewPainter.h"

#include "Graph/NodeParameterMap.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

void DelayPreviewPainter::paint(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) const {
    const bool showLabels = area.getWidth() >= 260.f && area.getHeight() >= 74.f;
    const Rectangle<float> background = area.reduced(
            jmin(area.getWidth(), area.getHeight()) * 0.12f);
    Rectangle<float> content = background;
    if (showLabels) {
        content = content.withTrimmedLeft(18.f).reduced(0.f, 8.f);
    }
    const NodeParameterMap parameters(node);
    const float time = parameters.floatValue("time", 0.5f);
    const float feedback = parameters.floatValue("feedback", 0.5f);
    const float spin = parameters.floatValue("spin", 0.5f);
    const float wet = parameters.floatValue("wet", 0.5f);
    const bool enabled = parameters.boolValue("enabled", true);
    const int spinLength = CycleDsp::delaySpinIterations(
            parameters.floatValue("spinIters", 0.f));
    constexpr int visibleBeatCount = 16;
    const float delayBeats = (float) CycleDsp::delayBeats(time, 4);
    float amplitude = 1.f;
    const auto colour = [enabled](Colour value) {
        return EffectPlotPalette::forEnabledState(value, enabled);
    };

    graphics.setColour(colour(EffectPlotPalette::background));
    graphics.fillRoundedRectangle(background, CanvasChromeMetrics::insetCornerRadius);
    for (int beat = 0; beat <= visibleBeatCount; ++beat) {
        const float unit = (float) beat / (float) visibleBeatCount;
        const float x = content.getX() + unit * content.getWidth();
        const bool measure = beat % 4 == 0;
        graphics.setColour(colour(
                EffectPlotPalette::grid.withAlpha(measure ? 0.38f : 0.20f)));
        graphics.drawVerticalLine(roundToInt(x), content.getY(), content.getBottom());
    }
    graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.68f)));
    graphics.fillRect(content.getX(), content.getCentreY() - 0.5f, content.getWidth(), 1.f);
    graphics.drawVerticalLine(roundToInt(content.getX()), content.getY(), content.getBottom());
    if (showLabels) {
        graphics.setColour(colour(EffectPlotPalette::label));
        graphics.setFont(FontOptions(11.f));
        graphics.drawText("L", content.getX() - 18.f, content.getY() - 5.f, 14.f, 12.f,
                Justification::centredRight);
        graphics.drawText("R", content.getX() - 18.f, content.getBottom() - 7.f, 14.f, 12.f,
                Justification::centredRight);
        graphics.drawText("TIME", content.getRight() - 44.f, content.getBottom() - 14.f,
                44.f, 13.f, Justification::centredRight);
    }
    for (int ping = 1; ; ++ping) {
        const float beat = (float) ping * delayBeats;
        if (beat > (float) visibleBeatCount) {
            break;
        }
        const float phase = (float) ((ping - 1) % spinLength)
                / (float) spinLength
                * MathConstants<float>::twoPi;
        const float pan = spin * (float) dsp::FastMathApproximations::sin((double) phase);
        const float x = content.getX() + beat / (float) visibleBeatCount * content.getWidth();
        const float y = content.getCentreY() + pan * content.getHeight() * 0.46f;
        const float radius = wet * amplitude * content.getHeight() * 0.17f;
        if (radius < 0.75f) {
            break;
        }
        graphics.setColour(colour(
                EffectPlotPalette::accent.withAlpha(0.30f + amplitude * 0.62f)));
        graphics.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre({ x, y }));
        amplitude *= feedback;
    }
}

}
