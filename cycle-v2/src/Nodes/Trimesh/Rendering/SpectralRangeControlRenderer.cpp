#include "Nodes/Trimesh/Rendering/SpectralRangeControlRenderer.h"

#include <Audio/CycleDsp/SpectralLayerCore.h>

#include "UI/Editors/PropertyControlLookAndFeel.h"

#include <utility>
#include <vector>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kLabelWidth = 62.f;

Rectangle<float> labelColumnBounds(Rectangle<float> row) {
    return row.removeFromLeft(kLabelWidth).reduced(8.f, 0.f);
}

std::vector<std::pair<float, String>> tickValues(PortDomain domain) {
    using CycleDsp::SpectralLayerCore;

    if (domain == PortDomain::SpectralPhaseSignal) {
        return {
                { SpectralLayerCore::rangeForPhaseOffsetScale(1.f), "1x" },
                { SpectralLayerCore::rangeForPhaseOffsetScale(10.f), "10x" },
                { SpectralLayerCore::rangeForPhaseOffsetScale(100.f), "100x" }
        };
    }

    return {
            { SpectralLayerCore::rangeForMagnitudeScale(0.1f), "0.1" },
            { SpectralLayerCore::rangeForMagnitudeScale(1.f), "1" },
            { SpectralLayerCore::rangeForMagnitudeScale(10.f), "10" },
            { SpectralLayerCore::rangeForMagnitudeScale(100.f), "100" }
    };
}

}

Rectangle<float> SpectralRangeControlRenderer::labelBounds(
        Rectangle<float> row,
        Rectangle<float> rail) {
    Rectangle<float> label = labelColumnBounds(row).withHeight(18.f);
    return label.withCentre({ label.getCentreX(), rail.getCentreY() });
}

void SpectralRangeControlRenderer::draw(
        Graphics& g,
        Rectangle<float> row,
        Rectangle<float> rail,
        PortDomain domain,
        float value) {
    paintMorphSlider(g, rail, jlimit(0.f, 1.f, value), Colour(0xffa7b0bd));

    g.setColour(kText);
    g.setFont(FontOptions(12.f));
    g.drawText(
            domain == PortDomain::SpectralPhaseSignal ? "Width" : "Range",
            labelBounds(row, rail),
            Justification::centredLeft);

    g.setColour(kMutedText);
    g.setFont(FontOptions(9.f));
    for (const auto& tick : tickValues(domain)) {
        const float x = rail.getX() + jlimit(0.f, 1.f, tick.first) * rail.getWidth();
        g.drawVerticalLine(
                roundToInt(x),
                rail.getBottom() + 2.f,
                rail.getBottom() + 6.f);
        g.drawText(
                tick.second,
                Rectangle<float>(x - 18.f, rail.getBottom() + 6.f, 36.f, 12.f),
                Justification::centred);
    }
}

}
