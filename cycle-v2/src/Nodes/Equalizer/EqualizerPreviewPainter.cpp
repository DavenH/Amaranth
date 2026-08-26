#include <Audio/CycleDsp/EqualizerCore.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>

#include "Nodes/Equalizer/EqualizerPreviewPainter.h"

#include "Graph/NodeParameterMap.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

void EqualizerPreviewPainter::paint(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        bool showDetails) const {
    CycleDsp::EqualizerCore core(1);
    const NodeParameterMap parameters(node);
    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        const String prefix = "band" + String(band + 1);
        core.configureBand(
                band,
                44100.0,
                CycleDsp::equalizerFrequency(parameters.floatValue(
                        prefix + "Frequency", 0.5f)),
                CycleDsp::equalizerGainDecibels(parameters.floatValue(
                        prefix + "Gain", 0.5f)));
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
    paintResponse(graphics, area, node, response, showDetails);
}

void EqualizerPreviewPainter::paintResponse(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        const std::vector<float>& response,
        bool showDetails) const {
    if (response.size() < 2) {
        return;
    }

    const bool enabled = NodeParameterMap(node).boolValue("enabled", true);
    const auto colour = [enabled](Colour value) {
        return EffectPlotPalette::forEnabledState(value, enabled);
    };
    const auto frequencyUnit = [](double frequency) {
        return std::log(frequency / 40.0) / std::log(400.0);
    };
    const auto frequencyX = [&area, &frequencyUnit](double frequency) {
        return area.getX() + (float) frequencyUnit(frequency) * area.getWidth();
    };

    graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.68f)));
    graphics.fillRect(area.getX(), area.getCentreY() - 0.5f, area.getWidth(), 1.f);
    if (showDetails) {
        for (const float landmark : { 60.f, 120.f, 250.f, 500.f, 1000.f,
                2000.f, 4000.f, 8000.f, 16000.f }) {
            graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.18f)));
            graphics.drawVerticalLine(roundToInt(frequencyX(landmark)), area.getY(), area.getBottom());
        }
        for (const float unit : { 0.1f, 0.3f, 0.7f, 0.9f }) {
            graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.20f)));
            graphics.fillRect(
                    area.getX(),
                    area.getY() + unit * area.getHeight() - 0.5f,
                    area.getWidth(),
                    1.f);
        }
    }

    Path path;
    Path fill;
    const float denominator = (float) (response.size() - 1);
    fill.startNewSubPath(area.getX(), area.getCentreY());
    for (size_t index = 0; index < response.size(); ++index) {
        const Point<float> point {
                area.getX() + (float) index / denominator * area.getWidth(),
                area.getBottom() - response[index] * area.getHeight()
        };
        fill.lineTo(point);
        if (index == 0) {
            path.startNewSubPath(point);
        } else {
            path.lineTo(point);
        }
    }
    fill.lineTo(area.getRight(), area.getCentreY());
    fill.closeSubPath();
    graphics.setColour(colour(EffectPlotPalette::label.withAlpha(0.20f)));
    graphics.fillPath(fill);
    graphics.setColour(colour(EffectPlotPalette::accent));
    graphics.strokePath(path, PathStrokeType(showDetails ? 2.f : 1.6f, PathStrokeType::curved));

    if (!showDetails) {
        return;
    }
    const bool expandedMarkers = area.getWidth() >= 300.f;
    const float markerSize = expandedMarkers ? 12.f : 5.f;
    if (expandedMarkers) {
        graphics.setFont(FontOptions(8.f, Font::bold));
    }
    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        const Rectangle<float> marker = Rectangle<float>(markerSize, markerSize).withCentre(
                bandControlPoint(area, node, band));
        graphics.setColour(colour(EffectPlotPalette::background.withAlpha(0.92f)));
        graphics.fillEllipse(marker);
        graphics.setColour(colour(EffectPlotPalette::accent.withAlpha(0.92f)));
        graphics.drawEllipse(marker, 1.f);
        if (expandedMarkers) {
            graphics.drawText(String(band + 1), marker, Justification::centred);
        }
    }
}

Point<float> EqualizerPreviewPainter::bandControlPoint(
        Rectangle<float> area,
        const Node& node,
        int band) const {
    const NodeParameterMap parameters(node);
    const String prefix = "band" + String(band + 1);
    const float frequency = CycleDsp::equalizerFrequency(
            parameters.floatValue(prefix + "Frequency", 0.5f));
    const float frequencyUnit = (float) (
            std::log((double) frequency / 40.0) / std::log(400.0));
    const float gainUnit = parameters.floatValue(prefix + "Gain", 0.5f);
    return {
            area.getX() + frequencyUnit * area.getWidth(),
            area.getBottom() - gainUnit * area.getHeight()
    };
}

}
