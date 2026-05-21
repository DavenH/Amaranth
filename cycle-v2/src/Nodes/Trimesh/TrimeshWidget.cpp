#include "TrimeshWidget.h"

#include <array>
#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

}

void TrimeshWidget::syncFromNode(const Node& node) {
    model.syncFromNode(node);
}

void TrimeshWidget::paintCompact(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        float) {
    syncFromNode(node);
    const TrimeshRenderData renderData = model.renderGrid(40, 20);

    if (!renderData.canDrawSurface()) {
        return;
    }

    if (!compactHeatmap.image.isValid()
            || compactHeatmap.valueCount != renderData.surface.size()
            || compactHeatmap.rows != renderData.rows
            || compactHeatmap.columns != renderData.columns
            || compactHeatmap.revision != model.getRevision()) {
        compactHeatmap.image = createHeatmapImage(renderData);
        compactHeatmap.valueCount = renderData.surface.size();
        compactHeatmap.rows = renderData.rows;
        compactHeatmap.columns = renderData.columns;
        compactHeatmap.revision = model.getRevision();
    }

    if (compactHeatmap.image.isValid()) {
        g.setImageResamplingQuality(Graphics::lowResamplingQuality);
        g.drawImage(compactHeatmap.image, meshPreviewContentArea(area));
    }
}

void TrimeshWidget::paintExpanded(Graphics& g, const Node& node, Rectangle<float> content) {
    syncFromNode(node);
    const TrimeshRenderData renderData = model.renderGrid(96, 48);

    const float gap = 14.f;
    auto topRow = content.removeFromTop(content.getHeight() * 0.62f);
    content.removeFromTop(gap);

    auto gridPanel = topRow.removeFromLeft(topRow.getWidth() * 0.60f);
    topRow.removeFromLeft(gap);
    auto sidePanel = topRow;
    auto waveshapePanel = content;

    drawPanelFrame(g, gridPanel, "3D grid heatmap");
    drawPanelFrame(g, sidePanel, "Morph / vertex");
    drawPanelFrame(g, waveshapePanel, "2D waveshape");

    drawMeshHeatmap(g, gridPanel.reduced(12.f, 26.f), renderData, true);

    const auto waveshapeContent = waveshapePanel.reduced(14.f, 28.f);
    g.setColour(Colour(0xff1e2a34).withAlpha(0.56f));
    for (int i = 1; i < 6; ++i) {
        const float y = waveshapeContent.getY() + waveshapeContent.getHeight() * (float) i / 6.f;
        g.drawHorizontalLine(roundToInt(y), waveshapeContent.getX(), waveshapeContent.getRight());
    }

    drawTrace(g, waveshapeContent.reduced(8.f), renderData.slice, Colour(0xffdfe7ff).withAlpha(0.92f));

    Rectangle<float> morphArea = sidePanel.reduced(14.f, 30.f);
    const std::array<std::pair<String, Colour>, 3> axes {
            std::make_pair(String("Yellow"), Colour(0xffe0c247)),
            std::make_pair(String("Red"), Colour(0xffd65a5a)),
            std::make_pair(String("Blue"), Colour(0xff5f91e8))
    };
    const std::array<float, 3> values {
            model.getMorphPosition().time.getCurrentValue(),
            model.getMorphPosition().red.getCurrentValue(),
            model.getMorphPosition().blue.getCurrentValue()
    };

    for (int i = 0; i < (int) axes.size(); ++i) {
        auto row = morphArea.removeFromTop(34.f);
        const Rectangle<float> rail = morphRailBounds(sidePanel.reduced(14.f, 30.f), i);

        g.setColour(kText);
        g.setFont(FontOptions(11.f, Font::bold));
        g.drawText(axes[(size_t) i].first, row.removeFromLeft(62.f), Justification::centredLeft);
        g.setColour(axes[(size_t) i].second.withAlpha(0.26f));
        g.fillRoundedRectangle(rail, 2.f);
        g.setColour(axes[(size_t) i].second.withAlpha(0.92f));
        g.fillEllipse(
                rail.getX() + rail.getWidth() * jlimit(0.f, 1.f, values[(size_t) i]) - 5.f,
                rail.getCentreY() - 5.f,
                10.f,
                10.f);
    }

    morphArea.removeFromTop(8.f);
    g.setColour(Colour(0xff0a0f13).withAlpha(0.72f));
    g.fillRoundedRectangle(morphArea, 5.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.f));
    g.drawText(
            "Vertex controls: amp, phase, sharpness, and component curve.",
            morphArea.reduced(10.f),
            Justification::topLeft);
}

bool TrimeshWidget::findMorphControlAt(
        Rectangle<float> content,
        Point<float> position,
        String& parameterId,
        float& value) const {
    const Rectangle<float> morphArea = morphPanelBounds(content);
    const std::array<String, 3> parameterIds {
            String("yellow"),
            String("red"),
            String("blue")
    };

    for (int i = 0; i < (int) parameterIds.size(); ++i) {
        const Rectangle<float> rail = morphRailBounds(morphArea, i);

        if (!rail.expanded(8.f, 12.f).contains(position)) {
            continue;
        }

        parameterId = parameterIds[(size_t) i];
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool TrimeshWidget::morphValueForParameterAt(
        Rectangle<float> content,
        const String& parameterId,
        Point<float> position,
        float& value) const {
    const std::array<String, 3> parameterIds {
            String("yellow"),
            String("red"),
            String("blue")
    };

    for (int i = 0; i < (int) parameterIds.size(); ++i) {
        if (parameterIds[(size_t) i] != parameterId) {
            continue;
        }

        const Rectangle<float> rail = morphRailBounds(morphPanelBounds(content), i);
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

Colour TrimeshWidget::meshSurfaceColour(float value) {
    const float v = jlimit(0.f, 1.f, value);
    const Colour low(0xff06090d);
    const Colour mid(0xff53657a);
    const Colour high(0xffd7eaff);

    if (v < 0.5f) {
        return low.interpolatedWith(mid, v * 2.f);
    }

    return mid.interpolatedWith(high, (v - 0.5f) * 2.f);
}

Rectangle<float> TrimeshWidget::meshPreviewContentArea(Rectangle<float> area) {
    return area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.024f);
}

void TrimeshWidget::drawPanelFrame(Graphics& g, Rectangle<float> area, const String& title) {
    g.setColour(Colour(0xff0e1318));
    g.fillRoundedRectangle(area, 6.f);
    g.setColour(Colour(0xff26313d));
    g.drawRoundedRectangle(area, 6.f, 1.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText(title, area.reduced(10.f, 6.f).removeFromTop(15.f), Justification::centredLeft);
}

void TrimeshWidget::drawMeshHeatmap(
        Graphics& g,
        Rectangle<float> area,
        const TrimeshRenderData& renderData,
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
            const Rectangle<float> cell(
                    surface.getX() + (float) column * cellWidth,
                    surface.getY() + (float) row * cellHeight,
                    cellWidth + 0.75f,
                    cellHeight + 0.75f);

            g.setColour(meshSurfaceColour(value).withAlpha(0.82f));
            g.fillRect(cell);
        }
    }

    if (drawGrid) {
        g.setColour(Colour(0xff1e2a34).withAlpha(0.52f));
        const int horizontalStep = jmax(1, renderData.rows / 5);
        for (int row = 0; row <= renderData.rows; row += horizontalStep) {
            const float y = surface.getY() + (float) row * cellHeight;
            g.drawHorizontalLine(roundToInt(y), surface.getX(), surface.getRight());
        }

        const int verticalStep = jmax(1, renderData.columns / 6);
        for (int column = 0; column <= renderData.columns; column += verticalStep) {
            const float x = surface.getX() + (float) column * cellWidth;
            g.drawVerticalLine(roundToInt(x), surface.getY(), surface.getBottom());
        }
    }
}

void TrimeshWidget::drawTrace(
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

Rectangle<float> TrimeshWidget::morphPanelBounds(Rectangle<float> content) {
    constexpr float gap = 14.f;
    auto topRow = content.removeFromTop(content.getHeight() * 0.62f);
    content.removeFromTop(gap);

    ignoreUnused(content);
    topRow.removeFromLeft(topRow.getWidth() * 0.60f);
    topRow.removeFromLeft(gap);
    return topRow.reduced(14.f, 30.f);
}

Rectangle<float> TrimeshWidget::morphRailBounds(Rectangle<float> morphArea, int axisIndex) {
    auto row = morphArea.translated(0.f, (float) axisIndex * 34.f).withHeight(34.f);
    return row.withTrimmedLeft(68.f).withTrimmedRight(10.f)
            .withSizeKeepingCentre(row.getWidth() - 92.f, 4.f);
}

Image TrimeshWidget::createHeatmapImage(const TrimeshRenderData& renderData) const {
    if (!renderData.canDrawSurface()) {
        return {};
    }

    Image image(Image::ARGB, renderData.columns, renderData.rows, true);

    for (int column = 0; column < renderData.columns; ++column) {
        for (int row = 0; row < renderData.rows; ++row) {
            const float value = renderData.surface[(size_t) column * (size_t) renderData.rows + (size_t) row];
            image.setPixelAt(column, row, meshSurfaceColour(value).withAlpha(0.82f));
        }
    }

    return image;
}

}
