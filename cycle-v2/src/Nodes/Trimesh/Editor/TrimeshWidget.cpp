#include "Nodes/Trimesh/Editor/TrimeshWidget.h"

#include "Graph/NodeParameterMap.h"
#include "UI/CanvasChromeMetrics.h"

#include <Curve/Mesh/Vertex.h>
#include <Util/Arithmetic.h>

#include <array>
#include <utility>

namespace CycleV2 {

namespace {

const Colour kMutedText { 0xff8793a1 };
constexpr float kExpandedPanelGap      = 8.f;
constexpr float kExpandedTopRowRatio   = 0.54f;
constexpr float kExpandedGridRatio     = 0.58f;
constexpr float kPanelHeaderHeight     = 18.f;
constexpr float kPanelContentInsetX    = 6.f;
constexpr float kPanelContentBottomPad = 2.f;
constexpr float kMorphPanelInsetX      = 10.f;
constexpr float kMorphPanelInsetY      = 24.f;
constexpr int kPreviewRows             = 320;
constexpr int kPreviewColumns          = 96;
constexpr int kExpandedRows            = 320;
constexpr int kExpandedColumns         = 96;
constexpr int kVertexParameterCount    = 6;

Rectangle<float> panelBodyBounds(Rectangle<float> panel) {
    panel.removeFromTop(kPanelHeaderHeight + 2.f);
    panel.removeFromBottom(kPanelContentBottomPad);
    return panel.reduced(kPanelContentInsetX, 0.f);
}

}

void TrimeshWidget::syncFromNode(const Node& node) {
    bridge.syncFromNode(node, kPreviewRows, kPreviewColumns);
}

void TrimeshWidget::syncGuideContext(const NodeGraph& graph, const Node& node) {
    String nextKey = TrimeshGuidePreparation::configurationKey(graph, node.id);
    if (node.model != nullptr) {
        nextKey << ":mesh=" << String((int64) node.model->revision());
    }
    if (nextKey == guideConfigurationKey) {
        return;
    }

    const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    if (model == nullptr) {
        return;
    }

    bridge.applyPreparedGuides(TrimeshGuidePreparation::prepare(
            graph,
            node,
            model->mesh()));
    guideConfigurationKey = nextKey;
}

void TrimeshWidget::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshWidget::setRenderProfile(TrimeshRenderProfile profile) {
    displayProfile = profile;
    bridge.setRenderProfile(profile);
}

void TrimeshWidget::setPreviewMidiNote(int midiNote) {
    previewMidiNote = midiNote;
    bridge.setPreviewMidiNote(midiNote);
}

void TrimeshWidget::setGuideAttachmentLabels(std::array<String, 6> labels) {
    guideAttachmentLabels = std::move(labels);
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
    const uint64_t compactRevision = model.getDerivedRevisions().compactPreview;

    if (!renderData.canDrawSurface()) {
        return;
    }

    if (!compactHeatmap.image.isValid()
            || compactHeatmap.valueCount != renderData.surface.size()
            || compactHeatmap.rows != renderData.rows
            || compactHeatmap.columns != renderData.columns
            || compactHeatmap.revision != compactRevision
            || compactHeatmap.domain != profile.getDomain()
            || compactHeatmap.scalePolicy != profile.getScalePolicy()
            || compactHeatmap.midiNote != previewMidiNote) {
        compactHeatmap.image = TrimeshSurfaceRenderer::createHeatmapImage(renderData, profile);
        compactHeatmap.valueCount = renderData.surface.size();
        compactHeatmap.rows = renderData.rows;
        compactHeatmap.columns = renderData.columns;
        compactHeatmap.revision = compactRevision;
        compactHeatmap.domain = profile.getDomain();
        compactHeatmap.scalePolicy = profile.getScalePolicy();
        compactHeatmap.midiNote = previewMidiNote;
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

    const auto& sliceStyle = profile.getSliceStyle();

    drawPanelFrame(g, gridPanel, sliceStyle.panel3DTitle, false);
    drawPanelFrame(g, sidePanel, "Morph / vertex");
    drawPanelFrame(g, waveshapePanel, sliceStyle.panel2DTitle, false);

    const bool hasPanel3DHost = bridge.getPanel3DHostComponentIfCreated() != nullptr;
    const bool hasPanel2DHost = bridge.getPanel2DHostComponentIfCreated() != nullptr;

    if (!hasPanel3DHost) {
        const auto gridContent = panelBodyBounds(gridPanel);
        Graphics::ScopedSaveState gridClip(g);

        g.reduceClipRegion(gridContent.toNearestInt());
        TrimeshSurfaceRenderer::drawHeatmap(g, gridContent, renderData, profile, true);
    }

    const auto waveshapeContent = panelBodyBounds(waveshapePanel);
    if (!hasPanel2DHost) {
        Graphics::ScopedSaveState waveshapeClip(g);

        g.reduceClipRegion(waveshapeContent.toNearestInt());
        TrimeshSliceRenderer2D::drawGrid(g, waveshapeContent, profile);
        TrimeshSliceRenderer2D::drawTraceFill(g, waveshapeContent.reduced(8.f), renderData.slice, profile);
        TrimeshSliceRenderer2D::drawTrace(
                g,
                waveshapeContent.reduced(8.f),
                renderData.slice,
                profile.getCurveStyle().positiveColour.toColour());
        TrimeshSliceRenderer2D::drawVertexMarkers(g, waveshapeContent.reduced(8.f), model.getVertexMarkers());
    }

    const auto selectedParameters = model.getSelectedVertexParameters();
    const int primaryAxis = model.getPrimaryViewAxis();
    const NodeParameterMap parameters(node);
    const std::array<TrimeshSidePanelRenderer::AxisState, 3> axes {
            TrimeshSidePanelRenderer::AxisState {
                    "Yellow",
                    "Y",
                    Colour(0xffe0c247),
                    model.getMorphPosition().time.getCurrentValue(),
                    primaryAxis == Vertex::Time,
                    parameters.boolValue("link.yellow", true)
            },
            TrimeshSidePanelRenderer::AxisState {
                    "Red",
                    "R",
                    Colour(0xffd65a5a),
                    model.getMorphPosition().red.getCurrentValue(),
                    primaryAxis == Vertex::Red,
                    parameters.boolValue("link.red", false)
            },
            TrimeshSidePanelRenderer::AxisState {
                    "Blue",
                    "B",
                    Colour(0xff5f91e8),
                    model.getMorphPosition().blue.getCurrentValue(),
                    primaryAxis == Vertex::Blue,
                    parameters.boolValue("link.blue", false)
            }
    };

    TrimeshSidePanelRenderer::drawSidePanel(
            g,
            sidePanel.reduced(kMorphPanelInsetX, kMorphPanelInsetY),
            axes,
            model.getSelectedCubePreviewVertices(),
            selectedParameters,
            guideAttachmentLabels);
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

Component* TrimeshWidget::getExpandedPanel3DComponentIfCreated() const {
    return bridge.getPanel3DHostComponentIfCreated();
}

Component* TrimeshWidget::prepareExpandedPanel2DComponent(
        const Node& node,
        Rectangle<float> content) {
    setRenderProfile(displayProfile);
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getPanel2DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel2DComponentIfCreated() const {
    return bridge.getPanel2DHostComponentIfCreated();
}

void TrimeshWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

int TrimeshWidget::resolvedSelectedVertexIndexForNode(const Node& node) {
    bridge.syncFromNode(node, kExpandedRows, kExpandedColumns);
    return bridge.getModel().getResolvedSelectedVertexIndex();
}

void TrimeshWidget::setExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    bridge.setPanelHostDelegate(delegate);
}

void TrimeshWidget::clearExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    bridge.clearPanelHostDelegate(delegate);
}

void TrimeshWidget::setMeshEditedCallback(
        std::function<void(TrimeshMeshEditEvent)> callback) {
    bridge.setMeshEditedCallback(std::move(callback));
}

Mesh& TrimeshWidget::currentMesh() {
    return bridge.getModel().currentMesh();
}

bool TrimeshWidget::setVertexParameter(
        int vertexIndex,
        const String& parameterId,
        float value) {
    return bridge.getModel().setVertexParameter(vertexIndex, parameterId, value);
}

std::vector<TrimeshVertexParameter> TrimeshWidget::vertexParametersForIndex(int vertexIndex) {
    return bridge.getModel().getVertexParametersForIndex(vertexIndex);
}

int TrimeshWidget::selectedVertexIndexForPanel() {
    return bridge.selectedVertexIndexForPanel();
}

std::vector<TrimeshVertexMarker> TrimeshWidget::vertexMarkers() {
    return bridge.getModel().getVertexMarkers();
}

const TrimeshRenderData& TrimeshWidget::renderDataForAutomation() const {
    return bridge.getRenderData();
}

TrimeshPanelRenderStats TrimeshWidget::panelRenderStatsForAutomation() const {
    const auto snapshot = bridge.getInteractor2D().rasterizerSnapshot();
    const auto samples = snapshot.waveY();
    const TrimeshPanel2D& panel = bridge.getPanel2D();
    TrimeshPanelRenderStats stats;
    stats.sampleCount = samples.size();
    stats.interceptCount = (int) snapshot.intercepts().size();
    const bool hasPanelSize = panel.getWidth() > 0 && panel.getHeight() > 0;
    if (hasPanelSize) {
        stats.phaseUnitsPerDisplayX = panel.invertScaleX(panel.getWidth())
                - panel.invertScaleX(0);
        stats.ampUnitsPerDisplayY = panel.invertScaleY(0)
                - panel.invertScaleY(panel.getHeight());
    }
    stats.intercepts.reserve(snapshot.intercepts().size());
    stats.displayedIntercepts.reserve(snapshot.intercepts().size());
    stats.displayedCurvePoints.reserve(snapshot.intercepts().size());
    stats.curveHover = bridge.getInteractor2D()
            .state.mouseFlags[PanelState::WithinReshapeThresh];
    for (const auto& intercept : snapshot.intercepts()) {
        stats.intercepts.emplace_back(intercept.x, intercept.y);
        if (hasPanelSize) {
            stats.displayedIntercepts.emplace_back(
                    panel.sx(intercept.x) / panel.getWidth(),
                    panel.sy(intercept.y) / panel.getHeight());
        }
    }
    const Buffer<Float32> waveX = snapshot.waveX();
    const Buffer<Float32> waveY = snapshot.waveY();
    if (hasPanelSize
            && snapshot.intercepts().size() > 1
            && !waveX.empty()
            && !waveY.empty()) {
        for (int index = 0; index + 1 < snapshot.intercepts().size(); ++index) {
            const float centreX = 0.5f * (
                    snapshot.intercepts()[index].x
                    + snapshot.intercepts()[index + 1].x);
            const int sampleIndex = jlimit(
                    0,
                    jmin(waveX.size(), waveY.size()) - 1,
                    Arithmetic::binarySearch(centreX, waveX));
            stats.displayedCurvePoints.emplace_back(
                    panel.sx(waveX[sampleIndex]) / panel.getWidth(),
                    panel.sy(waveY[sampleIndex]) / panel.getHeight());
        }
    }
    for (const auto& curve : snapshot.curves()) {
        VertCube* cube = curve.b.cube;
        if (cube == nullptr) {
            continue;
        }

        if (cube->getCompGuideCurve() >= 0) {
            ++stats.componentGuideSegmentCount;
        }
        if (cube->guideCurveAt(Vertex::Curve) >= 0) {
            ++stats.curveGuideSegmentCount;
        }
        for (int dimension = 0; dimension < Vertex::numElements; ++dimension) {
            if (dimension != Vertex::Time
                    && dimension != Vertex::Curve
                    && cube->guideCurveAt(dimension) >= 0) {
                ++stats.guideRailSegmentCount;
                break;
            }
        }
    }
    if (samples.empty()) {
        return stats;
    }

    stats.minimum = samples[0];
    stats.maximum = samples[0];
    stats.centreSample = samples[samples.size() / 2];
    for (int i = 0; i < samples.size(); ++i) {
        const float sample = samples[i];
        stats.minimum = jmin(stats.minimum, sample);
        stats.maximum = jmax(stats.maximum, sample);
        stats.absoluteSum += sample < 0.f ? -sample : sample;
    }
    return stats;
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

bool TrimeshWidget::findLinkToggleAt(
        Rectangle<float> content,
        Point<float> position,
        String& axisValue) const {
    const Rectangle<float> morphArea = morphPanelBounds(content);

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> button = linkToggleBounds(morphArea, i);

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

    for (int i = 0; i < kVertexParameterCount; ++i) {
        const Rectangle<float> row = vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> rail = vertexParameterRailBounds(row);

        if (!rail.expanded(5.f, 8.f).contains(position)) {
            continue;
        }

        parameterId = vertexParameterId(i);
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool TrimeshWidget::findVertexGuideAttachmentAt(
        Rectangle<float> content,
        Point<float> position,
        String& parameterId) const {
    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    for (int i = 0; i < kVertexParameterCount; ++i) {
        const Rectangle<float> row = vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> guide = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);

        if (!guide.expanded(4.f).contains(position)) {
            continue;
        }

        parameterId = vertexParameterId(i);
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

    for (int i = 0; i < kVertexParameterCount; ++i) {
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

std::vector<TrimeshExpandedHitRegion> TrimeshWidget::expandedControlHitRegions(Rectangle<float> content) {
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

    for (int i = 0; i < 3; ++i) {
        regions.push_back({
                TrimeshExpandedHitRegionKind::LinkToggle,
                linkToggleBounds(morphArea, i).expanded(4.f),
                {},
                primaryAxisValue(i)
        });
    }

    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    for (int i = 0; i < kVertexParameterCount; ++i) {
        const Rectangle<float> row = vertexParameterRowBounds(parameterArea, i);

        regions.push_back({
                TrimeshExpandedHitRegionKind::VertexParameter,
                vertexParameterRailBounds(row).expanded(5.f, 8.f),
                vertexParameterId(i),
                {}
        });
        regions.push_back({
                TrimeshExpandedHitRegionKind::VertexGuideAttachment,
                TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).expanded(4.f),
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
        g.fillRoundedRectangle(area, CanvasChromeMetrics::panelCornerRadius);
        g.setColour(Colour(0xff151a20).withAlpha(0.78f));
        g.fillRect(area.withTrimmedTop(kPanelHeaderHeight));
    } else {
        Rectangle<float> header = area.removeFromTop(kPanelHeaderHeight);
        g.setColour(Colour(0xff0e1318));
        g.fillRoundedRectangle(header, CanvasChromeMetrics::panelCornerRadius);
        g.fillRect(header.withTrimmedTop(jmax(
                0.f,
                header.getHeight() - CanvasChromeMetrics::panelCornerRadius)));
    }

    g.setColour(Colour(0xff26313d));
    g.drawRoundedRectangle(fullArea, CanvasChromeMetrics::panelCornerRadius, 1.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(9.8f));
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
    return TrimeshSidePanelRenderer::morphRailBounds(morphArea, axisIndex);
}

Rectangle<float> TrimeshWidget::primaryAxisBounds(Rectangle<float> morphArea, int axisIndex) {
    return TrimeshSidePanelRenderer::primaryAxisBounds(morphArea, axisIndex);
}

Rectangle<float> TrimeshWidget::linkToggleBounds(Rectangle<float> morphArea, int axisIndex) {
    return TrimeshSidePanelRenderer::linkToggleBounds(morphArea, axisIndex);
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
    return TrimeshSidePanelRenderer::vertexParameterPanelBounds(morphPanelBounds(content));
}

Rectangle<float> TrimeshWidget::vertexParameterRowBounds(Rectangle<float> parameterArea, int parameterIndex) {
    return TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, parameterIndex);
}

Rectangle<float> TrimeshWidget::vertexParameterRailBounds(Rectangle<float> parameterRow) {
    return TrimeshSidePanelRenderer::vertexParameterRailBounds(parameterRow);
}

String TrimeshWidget::vertexParameterId(int parameterIndex) {
    switch (parameterIndex) {
        case 0:     return "vertex.time";
        case 1:     return "vertex.red";
        case 2:     return "vertex.blue";
        case 3:     return "vertex.phase";
        case 4:     return "vertex.amp";
        case 5:     return "vertex.curve";
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
