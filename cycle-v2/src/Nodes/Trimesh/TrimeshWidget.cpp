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
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshWidget::setRenderProfile(TrimeshRenderProfile profile) {
    displayProfile = profile;
    bridge.setRenderProfile(profile);
}

void TrimeshWidget::paintCompact(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        float,
        PortDomain domain) {
    paintCompact(g, node, area, 1.f, TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshWidget::paintCompact(
        Graphics& g,
        const Node& node,
        Rectangle<float> area,
        float,
        const TrimeshRenderProfile& profile) {
    setRenderProfile(profile);
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
            || compactHeatmap.domain != profile.getDomain()
            || compactHeatmap.scalePolicy != profile.getScalePolicy()) {
        compactHeatmap.image = TrimeshSurfaceRenderer::createHeatmapImage(renderData, profile);
        compactHeatmap.valueCount = renderData.surface.size();
        compactHeatmap.rows = renderData.rows;
        compactHeatmap.columns = renderData.columns;
        compactHeatmap.revision = model.getRevision();
        compactHeatmap.domain = profile.getDomain();
        compactHeatmap.scalePolicy = profile.getScalePolicy();
    }

    if (compactHeatmap.image.isValid()) {
        g.setImageResamplingQuality(Graphics::lowResamplingQuality);
        g.drawImage(compactHeatmap.image, meshPreviewContentArea(area));
    }
}

void TrimeshWidget::paintExpanded(Graphics& g, const Node& node, Rectangle<float> content) {
    setRenderProfile(displayProfile);
    const TrimeshRenderProfile profile = displayProfile;
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
        TrimeshSurfaceRenderer::drawHeatmap(g, panelBodyBounds(gridPanel), renderData, profile, true);
    }

    const auto waveshapeContent = panelBodyBounds(waveshapePanel);
    if (!hasPanel2DHost) {
        TrimeshSliceRenderer2D::drawGrid(g, waveshapeContent, profile);
        TrimeshSliceRenderer2D::drawTraceFill(g, waveshapeContent.reduced(8.f), renderData.slice, profile);
        TrimeshSliceRenderer2D::drawTrace(
                g,
                waveshapeContent.reduced(8.f),
                renderData.slice,
                profile.positiveCurveColour().toColour());
        TrimeshSliceRenderer2D::drawVertexMarkers(g, waveshapeContent.reduced(8.f), model.getVertexMarkers());
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
    TrimeshSidePanelRenderer::drawMorphCubePreview(
            g,
            morphArea.removeFromRight(jmin(104.f, morphArea.getWidth() * 0.36f)).removeFromTop(104.f),
            values,
            model.getResolvedSelectedVertexIndex());

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
    TrimeshSidePanelRenderer::drawVertexParameters(g, morphArea, selectedParameters);
}

void TrimeshWidget::renderExpandedPanelsOpenGL(
        const Node& node,
        Rectangle<float> content,
        float scaleFactor) {
    setRenderProfile(displayProfile);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    bridge.renderPanel3D(expandedGridPanelContentBounds(content), scaleFactor);
    bridge.renderPanel2D(expandedWavePanelContentBounds(content), scaleFactor);
}

Component* TrimeshWidget::prepareExpandedPanel3DComponent(
        const Node& node,
        Rectangle<float> content) {
    setRenderProfile(displayProfile);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getPanel3DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel3DComponentIfCreated() {
    return bridge.getPanel3DHostComponentIfCreated();
}

Component* TrimeshWidget::prepareExpandedPanel2DComponent(
        const Node& node,
        Rectangle<float> content) {
    setRenderProfile(displayProfile);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getPanel2DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel2DComponentIfCreated() {
    return bridge.getPanel2DHostComponentIfCreated();
}

void TrimeshWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

int TrimeshWidget::resolvedSelectedVertexIndexForNode(const Node& node) {
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getModel().getResolvedSelectedVertexIndex();
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
    return TrimeshSurfaceRenderer::colourForProfile(value, profile);
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
    return TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, parameterIndex);
}

Rectangle<float> TrimeshWidget::vertexParameterRailBounds(Rectangle<float> parameterRow) {
    return TrimeshSidePanelRenderer::vertexParameterRailBounds(parameterRow);
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

}
