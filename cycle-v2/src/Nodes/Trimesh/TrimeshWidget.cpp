#include "TrimeshWidget.h"

#include <Curve/Mesh/Vertex.h>

#include <array>
#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

}

void TrimeshWidget::syncFromNode(const Node& node) {
    bridge.syncFromNode(node, 40, 20);
}

void TrimeshWidget::paintCompact(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        float) {
    bridge.syncFromNode(node, 40, 20);
    const TrimeshRenderData& renderData = bridge.getDataSource().getRenderData();
    TrimeshNodeModel& model = bridge.getModel();

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
    bridge.syncFromNode(node, 320, 96);
    const TrimeshRenderData& renderData = bridge.getDataSource().getRenderData();
    TrimeshNodeModel& model = bridge.getModel();

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

    if (bridge.getPanel3D().getComponent() == nullptr) {
        drawMeshHeatmap(g, gridPanel.reduced(12.f, 26.f), renderData, true);
    }

    const auto waveshapeContent = waveshapePanel.reduced(14.f, 28.f);
    drawEditorGrid(g, waveshapeContent);
    drawTraceFill(g, waveshapeContent.reduced(8.f), renderData.slice);
    drawTrace(g, waveshapeContent.reduced(8.f), renderData.slice, Colour(0xffe7edff).withAlpha(0.94f));

    const auto selectedParameters = model.getSelectedVertexParameters();
    drawVertexMarkers(g, waveshapeContent.reduced(8.f), model.getVertexMarkers());

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
    drawMorphCubePreview(g, morphArea.removeFromRight(jmin(96.f, morphArea.getWidth() * 0.34f)).removeFromTop(96.f), values);

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
    auto axisRow = morphArea.removeFromTop(48.f);
    g.setColour(kText);
    g.setFont(FontOptions(11.f, Font::bold));
    g.drawText("View Axis", axisRow.removeFromTop(16.f), Justification::centredLeft);

    for (int i = 0; i < (int) axes.size(); ++i) {
        const Rectangle<float> button = primaryAxisBounds(sidePanel.reduced(14.f, 30.f), i);
        const bool active = model.getPrimaryViewAxis() == (i == 0 ? Vertex::Time : (i == 1 ? Vertex::Red : Vertex::Blue));
        const Colour axisColour = axes[(size_t) i].second;

        g.setColour(axisColour.withAlpha(active ? 0.30f : 0.08f));
        g.fillRoundedRectangle(button, 4.f);
        g.setColour(axisColour.withAlpha(active ? 0.94f : 0.46f));
        g.drawRoundedRectangle(button, 4.f, active ? 1.4f : 1.f);
        g.setColour(active ? kText : kMutedText);
        g.setFont(FontOptions(10.f, Font::bold));
        g.drawText(axes[(size_t) i].first.substring(0, 1), button, Justification::centred);
    }

    morphArea.removeFromTop(6.f);
    drawVertexParameters(g, morphArea, selectedParameters);
}

Component* TrimeshWidget::prepareExpandedPanelComponent(
        const Node& node,
        Rectangle<float> content) {
    bridge.syncFromNode(node, 320, 96);
    return bridge.getPanelComponent();
}

Component* TrimeshWidget::getExpandedPanelComponentIfCreated() {
    return bridge.getPanelComponentIfCreated();
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

bool TrimeshWidget::findPrimaryAxisAt(
        Rectangle<float> content,
        Point<float> position,
        String& axisValue) const {
    const Rectangle<float> morphArea = morphPanelBounds(content);

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> button = primaryAxisBounds(morphArea, i);

        if (!button.expanded(4.f).contains(position)) {
            continue;
        }

        axisValue = primaryAxisValue(i);
        return true;
    }

    return false;
}

bool TrimeshWidget::findVertexParameterAt(
        Rectangle<float> content,
        Point<float> position,
        String& parameterId,
        float& value) const {
    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> row = vertexParameterRowBounds(parameterArea, i);

        if (!row.expanded(5.f, 4.f).contains(position)) {
            continue;
        }

        parameterId = vertexParameterId(i);
        const Rectangle<float> rail = vertexParameterRailBounds(row);
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool TrimeshWidget::vertexParameterValueForParameterAt(
        Rectangle<float> content,
        const String& parameterId,
        Point<float> position,
        float& value) const {
    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    for (int i = 0; i < 3; ++i) {
        if (vertexParameterId(i) != parameterId) {
            continue;
        }

        const Rectangle<float> rail = vertexParameterRailBounds(
                vertexParameterRowBounds(parameterArea, i));
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool TrimeshWidget::findVertexSelectionAt(
        const Node& node,
        Rectangle<float> content,
        Point<float> position,
        int& vertexIndex) {
    syncFromNode(node);
    const Rectangle<float> waveshape = waveshapeContentBounds(content);

    if (!waveshape.expanded(8.f).contains(position)) {
        return false;
    }

    const float phase = jlimit(0.f, 1.f, (position.x - waveshape.getX()) / waveshape.getWidth());
    const float amp = jlimit(0.f, 1.f, (waveshape.getBottom() - position.y) / waveshape.getHeight());
    vertexIndex = bridge.getModel().findNearestVertexIndexForPhaseAmp(phase, amp);
    return vertexIndex >= 0;
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
    g.setColour(Colour(0xff151a20).withAlpha(0.78f));
    g.fillRect(area.withTrimmedTop(22.f));
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
        g.setColour(Colour(0xffeef5ff).withAlpha(0.08f));
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

        g.setColour(Colour(0xffeef5ff).withAlpha(0.18f));
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

void TrimeshWidget::drawEditorGrid(Graphics& g, Rectangle<float> area) {
    g.setColour(Colour(0xff05070a).withAlpha(0.52f));
    g.fillRoundedRectangle(area, 4.f);

    g.setColour(Colour(0xff1b2430).withAlpha(0.70f));
    for (int i = 1; i < 32; ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / 32.f;
        g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 16; ++i) {
        const float y = area.getY() + area.getHeight() * (float) i / 16.f;
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }

    g.setColour(Colour(0xff546276).withAlpha(0.34f));
    for (int i = 1; i < 8; ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / 8.f;
        g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 4; ++i) {
        const float y = area.getY() + area.getHeight() * (float) i / 4.f;
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }
}

void TrimeshWidget::drawTraceFill(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<float>& values) {
    if (values.size() < 2) {
        return;
    }

    Path fillPath;
    const float denominator = (float) (values.size() - 1);
    const float centreY = area.getCentreY();

    fillPath.startNewSubPath(area.getX(), centreY);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / denominator;
        const float y = area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, values[(size_t) i]);
        fillPath.lineTo(x, y);
    }

    fillPath.lineTo(area.getRight(), centreY);
    fillPath.closeSubPath();

    g.setColour(Colour(0xff8ea0c6).withAlpha(0.18f));
    g.fillPath(fillPath);
    g.setColour(Colour(0xfff1f4ff).withAlpha(0.16f));
    g.strokePath(fillPath, PathStrokeType(5.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void TrimeshWidget::drawVertexMarkers(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexMarker>& markers) {
    for (const auto& marker : markers) {
        const Point<float> centre(
                area.getX() + area.getWidth() * jlimit(0.f, 1.f, marker.phase),
                area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, marker.amp));
        const float radius = marker.selected ? 5.5f : 3.2f;

        g.setColour(marker.selected
                ? Colour(0xffffd13d).withAlpha(0.92f)
                : Colour(0xfff3f7ff).withAlpha(0.76f));
        g.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre(centre));
        g.setColour(Colour(0xff05070a).withAlpha(marker.selected ? 0.86f : 0.66f));
        g.drawEllipse(Rectangle<float>((radius + 2.f) * 2.f, (radius + 2.f) * 2.f).withCentre(centre), 1.4f);
    }
}

void TrimeshWidget::drawMorphCubePreview(
        Graphics& g,
        Rectangle<float> area,
        const std::array<float, 3>& values) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }

    const Point<float> centre = area.getCentre();
    const float size = jmin(area.getWidth(), area.getHeight()) * 0.34f;
    const Point<float> top(centre.x, centre.y - size * 0.86f);
    const Point<float> left(centre.x - size, centre.y - size * 0.28f);
    const Point<float> right(centre.x + size, centre.y - size * 0.28f);
    const Point<float> bottom(centre.x, centre.y + size * 0.34f);
    const Point<float> back(centre.x, centre.y + size * 0.98f);

    Path leftFace;
    leftFace.startNewSubPath(top);
    leftFace.lineTo(left);
    leftFace.lineTo(bottom);
    leftFace.lineTo(centre);
    leftFace.closeSubPath();

    Path rightFace;
    rightFace.startNewSubPath(top);
    rightFace.lineTo(right);
    rightFace.lineTo(bottom);
    rightFace.lineTo(centre);
    rightFace.closeSubPath();

    Path lowerFace;
    lowerFace.startNewSubPath(left);
    lowerFace.lineTo(bottom);
    lowerFace.lineTo(back);
    lowerFace.lineTo(centre.x - size * 0.12f, centre.y + size * 0.08f);
    lowerFace.closeSubPath();

    g.setColour(Colour(0xff7e8794).withAlpha(0.18f));
    g.fillPath(leftFace);
    g.setColour(Colour(0xffd8e0ea).withAlpha(0.13f));
    g.fillPath(rightFace);
    g.setColour(Colour(0xff42505f).withAlpha(0.18f));
    g.fillPath(lowerFace);
    g.setColour(Colour(0xffbac5d0).withAlpha(0.30f));
    g.strokePath(leftFace, PathStrokeType(1.f));
    g.strokePath(rightFace, PathStrokeType(1.f));
    g.strokePath(lowerFace, PathStrokeType(1.f));

    const Point<float> yellowAxis(left.x, back.y);
    const Point<float> redAxis(right.x, back.y);
    const Point<float> blueAxis(centre.x, top.y);
    const Point<float> point(
            yellowAxis.x + (redAxis.x - yellowAxis.x) * jlimit(0.f, 1.f, values[0]),
            back.y + (blueAxis.y - back.y) * jlimit(0.f, 1.f, values[2]));
    const Point<float> lifted = point.translated(
            (jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.72f,
            -(jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.34f);

    g.setColour(Colour(0xffe0c247).withAlpha(0.64f));
    g.drawLine(yellowAxis.x, yellowAxis.y, redAxis.x, redAxis.y, 1.4f);
    g.setColour(Colour(0xffd65a5a).withAlpha(0.64f));
    g.drawLine(left.x, left.y, right.x, right.y, 1.2f);
    g.setColour(Colour(0xff5f91e8).withAlpha(0.66f));
    g.drawLine(centre.x, back.y, centre.x, top.y, 1.2f);
    g.setColour(Colour(0xffffd13d).withAlpha(0.94f));
    g.fillEllipse(Rectangle<float>(7.f, 7.f).withCentre(lifted));
    g.setColour(Colour(0xff05070a).withAlpha(0.72f));
    g.drawEllipse(Rectangle<float>(10.f, 10.f).withCentre(lifted), 1.2f);
}

void TrimeshWidget::drawVertexParameters(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexParameter>& parameters) {
    if (area.getHeight() < 28.f) {
        return;
    }

    g.setColour(Colour(0xff0a0f13).withAlpha(0.58f));
    g.fillRoundedRectangle(area, 5.f);
    g.setColour(kText);
    g.setFont(FontOptions(11.f, Font::bold));
    g.drawText("Selected Vertex", area.reduced(10.f, 8.f).removeFromTop(15.f), Justification::centredLeft);

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const auto& parameter = parameters[(size_t) i];
        const auto row = vertexParameterRowBounds(area, i);
        auto labelBox = row;
        auto valueBox = row;
        const auto rail = vertexParameterRailBounds(row);

        g.setColour(kMutedText);
        g.setFont(FontOptions(10.f, Font::bold));
        g.drawText(parameter.label, labelBox.removeFromLeft(70.f), Justification::centredLeft);

        const float range = parameter.maximum - parameter.minimum;
        const float normalized = range > 0.f
                ? jlimit(0.f, 1.f, (parameter.value - parameter.minimum) / range)
                : 0.f;
        valueBox = valueBox.removeFromRight(44.f);

        g.setColour(Colour(0xff273342).withAlpha(0.84f));
        g.fillRoundedRectangle(rail, 2.5f);
        g.setColour(Colour(0xffc5d1dc).withAlpha(0.86f));
        g.fillRoundedRectangle(rail.withWidth(rail.getWidth() * normalized), 2.5f);

        g.setColour(kText.withAlpha(0.88f));
        g.setFont(FontOptions(9.5f));
        g.drawText(String(parameter.value, 2), valueBox, Justification::centredRight);
    }
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
    return row.withTrimmedLeft(68.f).withTrimmedRight(jmax(16.f, row.getWidth() * 0.38f))
            .withSizeKeepingCentre(jmax(24.f, row.getWidth() * 0.42f), 4.f);
}

Rectangle<float> TrimeshWidget::primaryAxisBounds(Rectangle<float> morphArea, int axisIndex) {
    const float top = morphArea.getY() + 110.f;
    const float width = jmax(28.f, (morphArea.getWidth() - 16.f) / 3.f);
    return {
            morphArea.getX() + (float) axisIndex * (width + 8.f),
            top,
            width,
            24.f
    };
}

String TrimeshWidget::primaryAxisValue(int axis) {
    switch (axis) {
        case 1:     return "red";
        case 2:     return "blue";
        case 0:
        default:    return "yellow";
    }
}

Rectangle<float> TrimeshWidget::vertexParameterPanelBounds(Rectangle<float> content) {
    auto area = morphPanelBounds(content);
    area.removeFromTop(34.f * 3.f);
    area.removeFromTop(8.f);
    area.removeFromTop(48.f);
    area.removeFromTop(6.f);
    return area;
}

Rectangle<float> TrimeshWidget::vertexParameterRowBounds(Rectangle<float> parameterArea, int parameterIndex) {
    auto rows = parameterArea.reduced(10.f, 28.f);
    rows.translate(0.f, (float) parameterIndex * 32.f);
    return rows.removeFromTop(28.f);
}

Rectangle<float> TrimeshWidget::vertexParameterRailBounds(Rectangle<float> parameterRow) {
    parameterRow.removeFromLeft(70.f);
    parameterRow.removeFromRight(44.f);
    return parameterRow.withTrimmedRight(8.f)
            .withSizeKeepingCentre(jmax(4.f, parameterRow.getWidth() - 8.f), 5.f);
}

String TrimeshWidget::vertexParameterId(int parameterIndex) {
    switch (parameterIndex) {
        case 0:     return "vertex.amp";
        case 1:     return "vertex.phase";
        case 2:     return "vertex.curve";
        default:    return {};
    }
}

Rectangle<float> TrimeshWidget::waveshapeContentBounds(Rectangle<float> content) {
    constexpr float gap = 14.f;
    content.removeFromTop(content.getHeight() * 0.62f);
    content.removeFromTop(gap);
    return content.reduced(14.f, 28.f);
}

Rectangle<float> TrimeshWidget::expandedGridPanelContentBounds(Rectangle<float> content) {
    auto topRow = content.removeFromTop(content.getHeight() * 0.62f);
    auto gridPanel = topRow.removeFromLeft(topRow.getWidth() * 0.60f);
    return gridPanel.reduced(12.f, 26.f);
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
