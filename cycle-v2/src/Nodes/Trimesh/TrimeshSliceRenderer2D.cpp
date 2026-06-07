#include "TrimeshSliceRenderer2D.h"

namespace CycleV2 {

void TrimeshSliceRenderer2D::drawTrace(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<float>& values,
        Colour colour) {
    if (values.size() < 2) {
        return;
    }

    Path trace;
    const float denominator = (float) (values.size() - 1);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / denominator;
        const float y = area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, values[(size_t) i]);

        if (i == 0) {
            trace.startNewSubPath(x, y);
        } else {
            trace.lineTo(x, y);
        }
    }

    g.setColour(colour);
    g.strokePath(trace, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void TrimeshSliceRenderer2D::drawGrid(
        Graphics& g,
        Rectangle<float> area,
        const TrimeshRenderProfile& profile) {
    const auto& sliceStyle = profile.getSliceStyle();

    g.setColour(sliceStyle.fillColour);
    g.fillRoundedRectangle(area, 4.f);

    g.setColour(sliceStyle.minorGridColour);
    for (int i = 1; i < 32; ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / 32.f;
        g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 16; ++i) {
        const float y = area.getY() + area.getHeight() * (float) i / 16.f;
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }

    g.setColour(sliceStyle.majorGridColour);
    for (int i = 1; i < 8; ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / 8.f;
        g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 4; ++i) {
        const float y = area.getY() + area.getHeight() * (float) i / 4.f;
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }
}

void TrimeshSliceRenderer2D::drawTraceFill(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<float>& values,
        const TrimeshRenderProfile& profile) {
    if (values.size() < 2) {
        return;
    }

    Path fillPath;
    const float denominator = (float) (values.size() - 1);
    const auto& curveStyle = profile.getCurveStyle();
    const float baseY = curveStyle.bipolar ? area.getCentreY() : area.getBottom();

    fillPath.startNewSubPath(area.getX(), baseY);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / denominator;
        const float y = area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, values[(size_t) i]);
        fillPath.lineTo(x, y);
    }

    fillPath.lineTo(area.getRight(), baseY);
    fillPath.closeSubPath();

    g.setColour(curveStyle.positiveColour.toColour().withAlpha(0.22f));
    g.fillPath(fillPath);
    g.setColour(curveStyle.positiveColour.toColour().withAlpha(0.20f));
    g.strokePath(fillPath, PathStrokeType(5.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void TrimeshSliceRenderer2D::drawVertexMarkers(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexMarker>& markers) {
    for (const auto& marker : markers) {
        if (!marker.selected) {
            continue;
        }

        const Point<float> centre(
                area.getX() + area.getWidth() * jlimit(0.f, 1.f, marker.phase),
                area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, marker.amp));
        constexpr float radius = 5.5f;

        g.setColour(Colour(0xffd84a4a).withAlpha(0.92f));
        g.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre(centre));
        g.setColour(Colour(0xff05070a).withAlpha(0.86f));
        g.drawEllipse(Rectangle<float>((radius + 2.f) * 2.f, (radius + 2.f) * 2.f).withCentre(centre), 1.4f);
    }
}

}
