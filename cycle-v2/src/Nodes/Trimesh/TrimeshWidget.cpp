#include "TrimeshWidget.h"

#include <Curve/Mesh/Vertex.h>

#include <array>
#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kExpandedPanelGap      = 8.f;
constexpr float kExpandedTopRowRatio   = 0.54f;
constexpr float kExpandedGridRatio     = 0.62f;
constexpr float kPanelHeaderHeight     = 18.f;
constexpr float kPanelContentInsetX    = 6.f;
constexpr float kPanelContentBottomPad = 2.f;
constexpr float kMorphPanelInsetX      = 10.f;
constexpr float kMorphPanelInsetY      = 24.f;
constexpr int kPreviewRows             = 320;
constexpr int kPreviewColumns          = 96;
constexpr int kExpandedRows            = 320;
constexpr int kExpandedColumns         = 96;

Rectangle<float> panelBodyBounds(Rectangle<float> panel) {
    panel.removeFromTop(kPanelHeaderHeight + 2.f);
    panel.removeFromBottom(kPanelContentBottomPad);
    return panel.reduced(kPanelContentInsetX, 0.f);
}

}

void TrimeshWidget::syncFromNode(const Node& node) {
    bridge.syncFromNode(node, kPreviewRows, kPreviewColumns);
}

void TrimeshWidget::setDisplayDomain(PortDomain domain) {
    displayDomain = domain;
    bridge.setDisplayDomain(domain);
}

void TrimeshWidget::paintCompact(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        float,
        PortDomain domain) {
    setDisplayDomain(domain);
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(domain);
    bridge.syncFromNode(node, kPreviewRows, kPreviewColumns);
    const TrimeshRenderData& renderData = bridge.getDataSource().getRenderData();
    TrimeshNodeModel& model = bridge.getModel();

    if (!renderData.canDrawSurface()) {
        return;
    }

    if (!compactHeatmap.image.isValid()
            || compactHeatmap.valueCount != renderData.surface.size()
            || compactHeatmap.rows != renderData.rows
            || compactHeatmap.columns != renderData.columns
            || compactHeatmap.revision != model.getRevision()
            || compactHeatmap.domain != domain) {
        compactHeatmap.image = createHeatmapImage(renderData, profile);
        compactHeatmap.valueCount = renderData.surface.size();
        compactHeatmap.rows = renderData.rows;
        compactHeatmap.columns = renderData.columns;
        compactHeatmap.revision = model.getRevision();
        compactHeatmap.domain = domain;
    }

    if (compactHeatmap.image.isValid()) {
        g.setImageResamplingQuality(Graphics::lowResamplingQuality);
        g.drawImage(compactHeatmap.image, meshPreviewContentArea(area));
    }
}

void TrimeshWidget::paintExpanded(Graphics& g, const Node& node, Rectangle<float> content) {
    setDisplayDomain(displayDomain);
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(displayDomain);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    const TrimeshRenderData& renderData = bridge.getDataSource().getRenderData();
    TrimeshNodeModel& model = bridge.getModel();

    auto topRow = content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    content.removeFromTop(kExpandedPanelGap);

    auto gridPanel = topRow.removeFromLeft(topRow.getWidth() * kExpandedGridRatio);
    topRow.removeFromLeft(kExpandedPanelGap);
    auto sidePanel = topRow;
    auto waveshapePanel = content;

    drawPanelFrame(g, gridPanel, profile.panel3DTitle(), false);
    drawPanelFrame(g, sidePanel, "Morph / vertex");
    drawPanelFrame(g, waveshapePanel, profile.panel2DTitle(), false);

    const bool hasPanel3DHost = bridge.getPanel3DHostComponentIfCreated() != nullptr;
    const bool hasPanel2DHost = bridge.getPanel2DHostComponentIfCreated() != nullptr;

    if (!hasPanel3DHost) {
        drawMeshHeatmap(g, panelBodyBounds(gridPanel), renderData, profile, true);
    }

    const auto waveshapeContent = panelBodyBounds(waveshapePanel);
    if (!hasPanel2DHost) {
        drawEditorGrid(g, waveshapeContent, profile);
        drawTraceFill(g, waveshapeContent.reduced(8.f), renderData.slice, profile);
        drawTrace(g, waveshapeContent.reduced(8.f), renderData.slice, profile.positiveCurveColour().toColour());
        drawVertexMarkers(g, waveshapeContent.reduced(8.f), model.getVertexMarkers());
    }

    const auto selectedParameters = model.getSelectedVertexParameters();
    Rectangle<float> morphArea = sidePanel.reduced(kMorphPanelInsetX, kMorphPanelInsetY);
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
    drawMorphCubePreview(g, morphArea.removeFromRight(jmin(104.f, morphArea.getWidth() * 0.36f)).removeFromTop(104.f), values);

    for (int i = 0; i < (int) axes.size(); ++i) {
        auto row = morphArea.removeFromTop(34.f);
        const Rectangle<float> rail = morphRailBounds(sidePanel.reduced(kMorphPanelInsetX, kMorphPanelInsetY), i);

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
        const Rectangle<float> button = primaryAxisBounds(sidePanel.reduced(kMorphPanelInsetX, kMorphPanelInsetY), i);
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
    auto linkRow = morphArea.removeFromTop(28.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(9.5f, Font::bold));
    g.drawText("Link", linkRow.removeFromLeft(44.f), Justification::centredLeft);

    for (int i = 0; i < (int) axes.size(); ++i) {
        const Colour axisColour = axes[(size_t) i].second;
        const Rectangle<float> swatch(13.f, 13.f);
        const Point<float> centre(linkRow.getX() + 12.f + (float) i * 24.f, linkRow.getCentreY());

        g.setColour(axisColour.withAlpha(i == 0 ? 0.42f : 0.18f));
        g.fillRoundedRectangle(swatch.withCentre(centre), 2.f);
        g.setColour(axisColour.withAlpha(i == 0 ? 0.92f : 0.48f));
        g.drawRoundedRectangle(swatch.withCentre(centre), 2.f, 1.f);
    }

    morphArea.removeFromTop(3.f);
    drawVertexParameters(g, morphArea, selectedParameters);
}

void TrimeshWidget::renderExpandedPanelsOpenGL(
        const Node& node,
        Rectangle<float> content,
        float scaleFactor) {
    setDisplayDomain(displayDomain);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    bridge.renderPanel3D(expandedGridPanelContentBounds(content), scaleFactor);
    bridge.renderPanel2D(expandedWavePanelContentBounds(content), scaleFactor);
}

Component* TrimeshWidget::prepareExpandedPanel3DComponent(
        const Node& node,
        Rectangle<float> content) {
    setDisplayDomain(displayDomain);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getPanel3DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel3DComponentIfCreated() {
    return bridge.getPanel3DHostComponentIfCreated();
}

Component* TrimeshWidget::prepareExpandedPanel2DComponent(
        const Node& node,
        Rectangle<float> content) {
    setDisplayDomain(displayDomain);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getPanel2DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel2DComponentIfCreated() {
    return bridge.getPanel2DHostComponentIfCreated();
}

void TrimeshWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

void TrimeshWidget::setExpandedPanelCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback,
        std::function<void(Point<float>)> hoverCallback) {
    bridge.setPanelHostCallbacks(
            std::move(repaintCallback),
            std::move(cursorCallback),
            std::move(hoverCallback));
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

std::vector<TrimeshExpandedHitRegion> TrimeshWidget::expandedControlHitRegions(Rectangle<float> content) const {
    std::vector<TrimeshExpandedHitRegion> regions;
    const Rectangle<float> morphArea = morphPanelBounds(content);
    const std::array<String, 3> parameterIds {
            String("yellow"),
            String("red"),
            String("blue")
    };

    for (int i = 0; i < (int) parameterIds.size(); ++i) {
        regions.push_back({
                TrimeshExpandedHitRegionKind::MorphControl,
                morphRailBounds(morphArea, i).expanded(8.f, 12.f),
                parameterIds[(size_t) i],
                {}
        });
    }

    for (int i = 0; i < 3; ++i) {
        regions.push_back({
                TrimeshExpandedHitRegionKind::PrimaryAxis,
                primaryAxisBounds(morphArea, i).expanded(4.f),
                {},
                primaryAxisValue(i)
        });
    }

    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    for (int i = 0; i < 3; ++i) {
        regions.push_back({
                TrimeshExpandedHitRegionKind::VertexParameter,
                vertexParameterRowBounds(parameterArea, i).expanded(5.f, 4.f),
                vertexParameterId(i),
                {}
        });
    }

    return regions;
}

Colour TrimeshWidget::surfaceColourForDomain(float value, PortDomain domain) {
    return surfaceColourForProfile(value, TrimeshRenderProfile::fromDomain(domain));
}

Colour TrimeshWidget::surfaceColourForProfile(float value, const TrimeshRenderProfile& profile) {
    return profile.surfaceColour(value);
}

Rectangle<float> TrimeshWidget::meshPreviewContentArea(Rectangle<float> area) {
    return area.reduced(jmin(area.getWidth(), area.getHeight()) * 0.024f);
}

void TrimeshWidget::drawPanelFrame(
        Graphics& g,
        Rectangle<float> area,
        const String& title,
        bool fillBody) {
    const Rectangle<float> fullArea = area;

    if (fillBody) {
        g.setColour(Colour(0xff0e1318));
        g.fillRoundedRectangle(area, 6.f);
        g.setColour(Colour(0xff151a20).withAlpha(0.78f));
        g.fillRect(area.withTrimmedTop(kPanelHeaderHeight));
    } else {
        Rectangle<float> header = area.removeFromTop(kPanelHeaderHeight);
        g.setColour(Colour(0xff0e1318));
        g.fillRoundedRectangle(header, 6.f);
        g.fillRect(header.withTrimmedTop(jmax(0.f, header.getHeight() - 6.f)));
    }

    g.setColour(Colour(0xff26313d));
    g.drawRoundedRectangle(fullArea, 6.f, 1.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(9.8f, Font::bold));
    g.drawText(title, fullArea.reduced(9.f, 4.f).removeFromTop(14.f), Justification::centredLeft);
}

void TrimeshWidget::drawMeshHeatmap(
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

            g.setColour(surfaceColourForProfile(value, profile));
            g.fillRect(cell);
        }
    }

    if (drawGrid) {
        const bool spectral = profile.getSliceBackground() == TrimeshSliceBackground::Spectrum;
        g.setColour((spectral ? Colour(0xffd7b166) : Colour(0xffeef5ff)).withAlpha(0.08f));
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

        g.setColour((spectral ? Colour(0xffffd68a) : Colour(0xffeef5ff)).withAlpha(0.18f));
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

void TrimeshWidget::drawEditorGrid(
        Graphics& g,
        Rectangle<float> area,
        const TrimeshRenderProfile& profile) {
    const bool spectral = profile.getSliceBackground() == TrimeshSliceBackground::Spectrum;

    g.setColour((spectral ? Colour(0xff080608) : Colour(0xff05070a)).withAlpha(0.58f));
    g.fillRoundedRectangle(area, 4.f);

    g.setColour((spectral ? Colour(0xff241b18) : Colour(0xff1b2430)).withAlpha(0.70f));
    for (int i = 1; i < 32; ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / 32.f;
        g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 16; ++i) {
        const float y = area.getY() + area.getHeight() * (float) i / 16.f;
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }

    g.setColour((spectral ? Colour(0xff806646) : Colour(0xff546276)).withAlpha(0.34f));
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
        const std::vector<float>& values,
        const TrimeshRenderProfile& profile) {
    if (values.size() < 2) {
        return;
    }

    Path fillPath;
    const float denominator = (float) (values.size() - 1);
    const float baseY = profile.curveIsBipolar() ? area.getCentreY() : area.getBottom();

    fillPath.startNewSubPath(area.getX(), baseY);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = area.getX() + area.getWidth() * (float) i / denominator;
        const float y = area.getBottom() - area.getHeight() * jlimit(0.f, 1.f, values[(size_t) i]);
        fillPath.lineTo(x, y);
    }

    fillPath.lineTo(area.getRight(), baseY);
    fillPath.closeSubPath();

    g.setColour(profile.positiveCurveColour().toColour().withAlpha(0.22f));
    g.fillPath(fillPath);
    g.setColour(profile.positiveCurveColour().toColour().withAlpha(0.20f));
    g.strokePath(fillPath, PathStrokeType(5.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void TrimeshWidget::drawVertexMarkers(
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

void TrimeshWidget::drawMorphCubePreview(
        Graphics& g,
        Rectangle<float> area,
        const std::array<float, 3>& values) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }

    area = area.reduced(3.f);
    g.setColour(Colour(0xff05070a).withAlpha(0.42f));
    g.fillRoundedRectangle(area, 4.f);

    const Point<float> centre = area.getCentre();
    const float size = jmin(area.getWidth(), area.getHeight()) * 0.30f;
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

    g.setColour(Colour(0xff7e8794).withAlpha(0.16f));
    g.fillPath(leftFace);
    g.setColour(Colour(0xffd8e0ea).withAlpha(0.11f));
    g.fillPath(rightFace);
    g.setColour(Colour(0xff42505f).withAlpha(0.15f));
    g.fillPath(lowerFace);
    g.setColour(Colour(0xffbac5d0).withAlpha(0.26f));
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

    g.setColour(Colour(0xffe0c247).withAlpha(0.68f));
    g.drawLine(yellowAxis.x, yellowAxis.y, redAxis.x, redAxis.y, 1.4f);
    g.setColour(Colour(0xffd65a5a).withAlpha(0.68f));
    g.drawLine(left.x, left.y, right.x, right.y, 1.2f);
    g.setColour(Colour(0xff5f91e8).withAlpha(0.70f));
    g.drawLine(centre.x, back.y, centre.x, top.y, 1.2f);

    g.setColour(Colour(0xffe0c247).withAlpha(0.38f));
    g.fillEllipse(Rectangle<float>(4.f, 4.f).withCentre(yellowAxis));
    g.setColour(Colour(0xffd65a5a).withAlpha(0.38f));
    g.fillEllipse(Rectangle<float>(4.f, 4.f).withCentre(right));
    g.setColour(Colour(0xff5f91e8).withAlpha(0.38f));
    g.fillEllipse(Rectangle<float>(4.f, 4.f).withCentre(top));

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

    area = area.withHeight(jmin(area.getHeight(), 140.f));
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
    auto topRow = content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    content.removeFromTop(kExpandedPanelGap);

    ignoreUnused(content);
    topRow.removeFromLeft(topRow.getWidth() * kExpandedGridRatio);
    topRow.removeFromLeft(kExpandedPanelGap);
    return topRow.reduced(kMorphPanelInsetX, kMorphPanelInsetY);
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
    content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    content.removeFromTop(kExpandedPanelGap);
    return panelBodyBounds(content);
}

Rectangle<float> TrimeshWidget::expandedGridPanelContentBounds(Rectangle<float> content) {
    auto topRow = content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    auto gridPanel = topRow.removeFromLeft(topRow.getWidth() * kExpandedGridRatio);
    return panelBodyBounds(gridPanel);
}

Rectangle<float> TrimeshWidget::expandedWavePanelContentBounds(Rectangle<float> content) {
    content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    content.removeFromTop(kExpandedPanelGap);
    return panelBodyBounds(content);
}

Image TrimeshWidget::createHeatmapImage(
        const TrimeshRenderData& renderData,
        const TrimeshRenderProfile& profile) const {
    if (!renderData.canDrawSurface()) {
        return {};
    }

    Image image(Image::ARGB, renderData.columns, renderData.rows, true);

    for (int column = 0; column < renderData.columns; ++column) {
        for (int row = 0; row < renderData.rows; ++row) {
            const float value = renderData.surface[(size_t) column * (size_t) renderData.rows + (size_t) row];
            image.setPixelAt(column, renderData.rows - 1 - row, surfaceColourForProfile(value, profile));
        }
    }

    return image;
}

}
