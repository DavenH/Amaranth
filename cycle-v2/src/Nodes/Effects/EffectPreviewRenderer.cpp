#include "EffectPreviewRenderer.h"

#include "EffectPlotPalette.h"

#include "../../UI/NodeParameterValue.h"

#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EqualizerCore.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>

namespace CycleV2 {

namespace {

float parameterValue(const Node& node, const String& id, float fallback) {
    return nodeParameterValue(node, id, String(fallback)).getFloatValue();
}

Colour previewColour(bool enabled, Colour colour) {
    return EffectPlotPalette::forEnabledState(colour, enabled);
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
    const bool enabled = parameterValue(node, "enabled", 1.f) >= 0.5f;
    const int reflectionCount = 5 + roundToInt(size * 6.f);
    float amplitude = 0.88f;
    const float decay = 0.48f + size * 0.36f - damping * 0.12f;

    graphics.setColour(previewColour(enabled, EffectPlotPalette::background));
    graphics.fillRoundedRectangle(content, 4.f);
    for (int index = 0; index < reflectionCount; ++index) {
        const float unit = (float) index / (float) jmax(1, reflectionCount - 1);
        const float x = content.getX() + unit * content.getWidth();
        const float height = content.getHeight() * amplitude * (0.36f + wet * 0.5f);
        const float spread = height * (0.35f + width * 0.65f);
        graphics.setColour(previewColour(
                enabled,
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

void paintDelayAxes(
        Graphics& graphics,
        Rectangle<float> plot,
        bool showLabels,
        bool enabled) {
    const Colour axis = EffectPlotPalette::grid;
    graphics.setColour(previewColour(enabled, axis.withAlpha(0.68f)));
    graphics.fillRect(plot.getX(), plot.getCentreY() - 0.5f, plot.getWidth(), 1.f);
    graphics.drawVerticalLine(roundToInt(plot.getX()), plot.getY(), plot.getBottom());

    if (!showLabels) {
        return;
    }

    graphics.setColour(previewColour(enabled, EffectPlotPalette::label));
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("L", plot.getX() - 18.f, plot.getY() - 5.f, 14.f, 12.f,
            Justification::centredRight);
    graphics.drawText("R", plot.getX() - 18.f, plot.getBottom() - 7.f, 14.f, 12.f,
            Justification::centredRight);
    graphics.drawText("TIME", plot.getRight() - 44.f, plot.getBottom() - 14.f, 44.f, 13.f,
            Justification::centredRight);
}

void paintBeatGrid(
        Graphics& graphics,
        Rectangle<float> plot,
        int beatCount,
        bool enabled) {
    for (int beat = 0; beat <= beatCount; ++beat) {
        const float unit = (float) beat / (float) beatCount;
        const float x = plot.getX() + unit * plot.getWidth();
        const bool measure = beat % 4 == 0;
        graphics.setColour(previewColour(
                enabled,
                EffectPlotPalette::grid.withAlpha(measure ? 0.38f : 0.20f)));
        graphics.drawVerticalLine(roundToInt(x), plot.getY(), plot.getBottom());
    }
}

}

void paintDelayPingPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) {
    const bool showLabels = area.getWidth() >= 260.f && area.getHeight() >= 74.f;
    const Rectangle<float> background = contentArea(area);
    Rectangle<float> content = background;
    if (showLabels) {
        content = content.withTrimmedLeft(18.f).reduced(0.f, 8.f);
    }
    const float time = parameterValue(node, "time", 0.5f);
    const float feedback = parameterValue(node, "feedback", 0.5f);
    const float spin = parameterValue(node, "spin", 0.5f);
    const float wet = parameterValue(node, "wet", 0.5f);
    const bool enabled = parameterValue(node, "enabled", 1.f) >= 0.5f;
    const int spinLength = CycleDsp::delaySpinIterations(
            parameterValue(node, "spinIters", 0.f));
    constexpr int visibleBeatCount = 16;
    const float delayBeats = (float) CycleDsp::delayBeats(time, 4);
    float amplitude = 1.f;

    graphics.setColour(previewColour(enabled, EffectPlotPalette::background));
    graphics.fillRoundedRectangle(background, 4.f);
    paintBeatGrid(graphics, content, visibleBeatCount, enabled);
    paintDelayAxes(graphics, content, showLabels, enabled);
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
        graphics.setColour(previewColour(
                enabled,
                EffectPlotPalette::accent.withAlpha(0.30f + amplitude * 0.62f)));
        graphics.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre({ x, y }));
        amplitude *= feedback;
    }
}

void paintEqualizerResponsePreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        bool showDetails) {
    CycleDsp::EqualizerCore core(1);
    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        const String prefix = "band" + String(band + 1);
        core.configureBand(
                band,
                44100.0,
                CycleDsp::equalizerFrequency(parameterValue(
                        node, prefix + "Frequency", 0.5f)),
                CycleDsp::equalizerGainDecibels(parameterValue(
                        node, prefix + "Gain", 0.5f)));
    }

    const int pointCount = jmax(2, roundToInt(area.getWidth()));
    std::vector<float> response((size_t) pointCount);
    double frequency = 40.0;
    const double frequencyRatio = std::pow(400.0, 1.0 / (double) (pointCount - 1));
    for (int index = 0; index < pointCount; ++index) {
        response[(size_t) index] = jlimit(
                0.f,
                1.f,
                core.responseDecibels(frequency) / 60.f + 0.5f);
        frequency *= frequencyRatio;
    }
    paintEqualizerResponseData(graphics, area, node, response, showDetails);
}

void paintEqualizerResponseData(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        const std::vector<float>& response,
        bool showDetails) {
    if (response.size() < 2) {
        return;
    }

    const bool enabled = parameterValue(node, "enabled", 1.f) >= 0.5f;
    const auto stateColour = [enabled](Colour colour) {
        return EffectPlotPalette::forEnabledState(colour, enabled);
    };
    const auto frequencyUnit = [](double frequency) {
        return std::log(frequency / 40.0) / std::log(400.0);
    };
    const auto frequencyX = [&area, &frequencyUnit](double frequency) {
        return area.getX() + (float) frequencyUnit(frequency) * area.getWidth();
    };

    graphics.setColour(stateColour(EffectPlotPalette::grid.withAlpha(0.68f)));
    graphics.fillRect(area.getX(), area.getCentreY() - 0.5f, area.getWidth(), 1.f);
    if (showDetails) {
        for (const float landmark : { 60.f, 120.f, 250.f, 500.f, 1000.f,
                2000.f, 4000.f, 8000.f, 16000.f }) {
            graphics.setColour(stateColour(EffectPlotPalette::grid.withAlpha(0.18f)));
            graphics.drawVerticalLine(roundToInt(frequencyX(landmark)), area.getY(), area.getBottom());
        }
        for (const float unit : { 0.1f, 0.3f, 0.7f, 0.9f }) {
            graphics.setColour(stateColour(EffectPlotPalette::grid.withAlpha(0.20f)));
            graphics.fillRect(
                    area.getX(),
                    area.getY() + unit * area.getHeight() - 0.5f,
                    area.getWidth(),
                    1.f);
        }
    }

    Path path;
    const float denominator = (float) (response.size() - 1);
    for (size_t index = 0; index < response.size(); ++index) {
        const Point<float> point {
                area.getX() + (float) index / denominator * area.getWidth(),
                area.getBottom() - response[index] * area.getHeight()
        };
        if (index == 0) {
            path.startNewSubPath(point);
        } else {
            path.lineTo(point);
        }
    }
    graphics.setColour(stateColour(EffectPlotPalette::accent));
    graphics.strokePath(path, PathStrokeType(showDetails ? 2.f : 1.6f, PathStrokeType::curved));

    if (!showDetails) {
        return;
    }
    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        const Rectangle<float> marker = Rectangle<float>(12.f, 12.f).withCentre(
                equalizerBandControlPoint(area, node, band));
        graphics.setColour(stateColour(EffectPlotPalette::background.withAlpha(0.92f)));
        graphics.fillEllipse(marker);
        graphics.setColour(stateColour(EffectPlotPalette::accent.withAlpha(0.92f)));
        graphics.drawEllipse(marker, 1.f);
    }
}

Point<float> equalizerBandControlPoint(
        Rectangle<float> area,
        const Node& node,
        int band) {
    const String prefix = "band" + String(band + 1);
    const float frequency = CycleDsp::equalizerFrequency(parameterValue(
            node, prefix + "Frequency", 0.5f));
    const float frequencyUnit = (float) (
            std::log((double) frequency / 40.0) / std::log(400.0));
    const float gainUnit = parameterValue(node, prefix + "Gain", 0.5f);
    return {
            area.getX() + frequencyUnit * area.getWidth(),
            area.getBottom() - gainUnit * area.getHeight()
    };
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
