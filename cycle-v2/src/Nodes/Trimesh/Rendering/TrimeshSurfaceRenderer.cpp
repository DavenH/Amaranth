#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"

namespace CycleV2 {

Colour TrimeshSurfaceRenderer::colourForProfile(float value, const TrimeshRenderProfile& profile) {
    return profile.getSurfaceStyle().colourForValue(value);
}

Image TrimeshSurfaceRenderer::createHeatmapImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool opaque,
        float surfaceAspectRatio) {
    if (!renderData.canDrawSurface()) {
        return {};
    }

    const std::vector<float> pitchSurface = renderData.pitchSpansColumns
            ? profile.mapPitchColumnsToDisplay(
                    renderData.surface,
                    (size_t) renderData.columns,
                    (size_t) renderData.rows)
            : std::vector<float>();
    const std::vector<float>& surface = pitchSurface.empty()
            ? renderData.surface
            : pitchSurface;

    return ScalarSurfaceMaterialEvaluator::createImage(
            surface.data(),
            (int) surface.size(),
            renderData.columns,
            renderData.rows,
            profile.getSurfaceStyle().surfaceMaterial(),
            opaque,
            surfaceAspectRatio);
}

void TrimeshSurfaceRenderer::drawHeatmap(
        Graphics& g,
        Rectangle<float> area,
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool drawGrid) {
    if (!renderData.canDrawSurface()) {
        return;
    }

    const Rectangle<float> surface = area.reduced(area.getWidth() * 0.025f, area.getHeight() * 0.06f);
    const Image heatmap = createHeatmapImage(
            renderData,
            profile,
            false,
            surface.getWidth() / jmax(1.f, surface.getHeight()));
    if (!heatmap.isValid()) {
        return;
    }
    g.setImageResamplingQuality(Graphics::highResamplingQuality);
    g.drawImage(heatmap, surface);

    if (!drawGrid) {
        return;
    }

    const float cellWidth = surface.getWidth() / (float) renderData.columns;
    const float cellHeight = surface.getHeight() / (float) renderData.rows;

    const auto& surfaceStyle = profile.getSurfaceStyle();

    g.setColour(surfaceStyle.minorGridColour);
    const int minorHorizontalStep = jmax(1, renderData.rows / 16);
    for (int row = 0; row <= renderData.rows; row += minorHorizontalStep) {
        const float y = surface.getY() + (float) row * cellHeight;
        g.drawHorizontalLine(roundToInt(y), surface.getX(), surface.getRight());
    }

    const int minorVerticalStep = jmax(1, renderData.columns / 24);
    for (int column = 0; column <= renderData.columns; column += minorVerticalStep) {
        const float x = surface.getX() + (float) column * cellWidth;
        g.drawVerticalLine(roundToInt(x), surface.getY(), surface.getBottom());
    }

    g.setColour(surfaceStyle.majorGridColour);
    const int horizontalStep = jmax(1, renderData.rows / 4);
    for (int row = 0; row <= renderData.rows; row += horizontalStep) {
        const float y = surface.getY() + (float) row * cellHeight;
        g.drawHorizontalLine(roundToInt(y), surface.getX(), surface.getRight());
    }

    const int verticalStep = jmax(1, renderData.columns / 8);
    for (int column = 0; column <= renderData.columns; column += verticalStep) {
        const float x = surface.getX() + (float) column * cellWidth;
        g.drawVerticalLine(roundToInt(x), surface.getY(), surface.getBottom());
    }
}

}
