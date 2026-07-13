#include "TrimeshSurfaceRenderer.h"

namespace CycleV2 {

Colour TrimeshSurfaceRenderer::colourForProfile(float value, const TrimeshRenderProfile& profile) {
    return profile.getSurfaceStyle().colourForValue(value);
}

Image TrimeshSurfaceRenderer::createHeatmapImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile,
        bool opaque) {
    if (!renderData.canDrawSurface()) {
        return {};
    }

    Image image(opaque ? Image::RGB : Image::ARGB, renderData.columns, renderData.rows, true);

    for (int column = 0; column < renderData.columns; ++column) {
        for (int row = 0; row < renderData.rows; ++row) {
            const float value = renderData.surface[(size_t) column * (size_t) renderData.rows + (size_t) row];
            Colour colour = colourForProfile(value, profile);

            if (opaque) {
                colour = colour.withAlpha(1.f);
            }

            image.setPixelAt(column, renderData.rows - 1 - row, colour);
        }
    }

    return image;
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
    const float cellWidth = surface.getWidth() / (float) renderData.columns;
    const float cellHeight = surface.getHeight() / (float) renderData.rows;

    for (int column = 0; column < renderData.columns; ++column) {
        for (int row = 0; row < renderData.rows; ++row) {
            const float value = renderData.surface[(size_t) column * (size_t) renderData.rows + (size_t) row];
            const int displayRow = renderData.rows - 1 - row;
            const Rectangle<float> cell(
                    surface.getX() + (float) column * cellWidth,
                    surface.getY() + (float) displayRow * cellHeight,
                    cellWidth + 0.75f,
                    cellHeight + 0.75f);

            g.setColour(colourForProfile(value, profile));
            g.fillRect(cell);
        }
    }

    if (!drawGrid) {
        return;
    }

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
