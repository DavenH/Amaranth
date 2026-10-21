#include "Nodes/Trimesh/Editor/TrimeshWidget.h"

#include "Graph/NodeParameterMap.h"
#include "Graph/MeshDimensionality.h"
#include "UI/Editors/MorphControlLayout.h"
#include "UI/CanvasChromeMetrics.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

#include <Curve/Mesh/Vertex.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <array>
#include <utility>

namespace CycleV2 {

namespace {

constexpr float kExpandedPanelGap      = 8.f;
constexpr float kExpandedTopRowRatio   = 0.54f;
constexpr float kExpandedGridRatio     = 0.50f;
constexpr int kPreviewRows             = 320;
constexpr int kPreviewColumns          = 96;
constexpr int kExpandedRows            = 320;
constexpr int kExpandedColumns         = 768;
constexpr int kVertexParameterCount    = 7;

}

void TrimeshWidget::syncFromNode(const Node& node) {
    bridge.syncFromNode(node, kPreviewRows, lastExpandedColumnCount);
}

void TrimeshWidget::syncExpandedFromNode(const Node& node) {
    bridge.syncFromNode(node, kExpandedRows, lastExpandedColumnCount);
}

void TrimeshWidget::setMorphEditGestureActive(bool active) {
    bridge.setMorphEditGestureActive(active);
}

void TrimeshWidget::syncGuideContext(const NodeGraph& graph, const Node& node) {
    if (bridge.isMeshEditGestureActive()) {
        return;
    }

    String nextKey = GuideCurveMeshPreparation::configurationKey(graph, node.id);
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

    if (bridge.applyPreparedGuides(TrimeshGuidePreparation::prepare(
                graph,
                node,
                model->mesh()))) {
        guideConfigurationKey = nextKey;
    }
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

void TrimeshWidget::setPreviewKeyScaleAxis(const String& axis) {
    int dimension = -1;
    if (axis == "yellow") {
        dimension = Vertex::Time;
    } else if (axis == "red") {
        dimension = Vertex::Red;
    } else if (axis == "blue") {
        dimension = Vertex::Blue;
    }
    bridge.setPreviewKeyScaleAxis(dimension);
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
    syncFromNode(node);
    const TrimeshRenderData& panelData = bridge.getDataSource().getRenderData();
    const Rectangle<float> previewArea = meshPreviewContentArea(area);
    if (MeshDimensionality::dimensions(node).axisCount() == 0) {
        bridge.paintPreview2D(g, previewArea);
        return;
    }
    const float surfaceAspectRatio = previewArea.getWidth()
            / jmax(1.f, previewArea.getHeight());
    const int displayHeight = roundToInt(previewArea.getHeight());

    if (!panelData.canDrawSurface()) {
        return;
    }

    if (needsCompactHeatmapRefresh(panelData, profile, surfaceAspectRatio, displayHeight)) {
        refreshCompactHeatmap(panelData, profile, surfaceAspectRatio, displayHeight);
    }

    if (compactHeatmap.image.isValid()) {
        g.setImageResamplingQuality(Graphics::lowResamplingQuality);
        g.drawImage(compactHeatmap.image, previewArea);
    }
}

bool TrimeshWidget::renderCompactPreviewOpenGL(const Node& node, Rectangle<float> area, float scaleFactor) {
    syncFromNode(node);
    return bridge.renderPreview2D(meshPreviewContentArea(area), scaleFactor);
}

bool TrimeshWidget::needsCompactHeatmapRefresh(
        const TrimeshRenderData& panelData,
        const TrimeshRenderProfile& profile,
        float surfaceAspectRatio,
        int displayHeight) const {
    return !compactHeatmap.image.isValid()
            || compactHeatmap.rows != panelData.rows
            || compactHeatmap.columns != kPreviewColumns
            || compactHeatmap.displayHeight != displayHeight
            || compactHeatmap.revision != bridge.getModel().getDerivedRevisions().compactPreview
            || compactHeatmap.domain != profile.getDomain()
            || compactHeatmap.scalePolicy != profile.getScalePolicy()
            || compactHeatmap.materialStyleSignature
                    != profile.getSurfaceStyle().materialStyleSignature()
            || compactHeatmap.midiNote != panelData.midiNote
            || compactHeatmap.pitchSpansColumns != panelData.pitchSpansColumns
            || compactHeatmap.surfaceAspectRatio != surfaceAspectRatio;
}

void TrimeshWidget::refreshCompactHeatmap(
        const TrimeshRenderData& panelData,
        const TrimeshRenderProfile& profile,
        float surfaceAspectRatio,
        int displayHeight) {
    std::optional<TrimeshRenderData> compactGrid;
    if (panelData.columns != kPreviewColumns) {
        compactGrid = TrimeshGridRenderService::renderGrid(
                bridge.getModel(), panelData.rows, kPreviewColumns, profile,
                panelData.midiNote, bridge.getPreviewKeyScaleAxis());
    }
    const TrimeshRenderData& compactData = compactGrid.has_value()
            ? *compactGrid : panelData;

    compactHeatmap.image = TrimeshSurfaceRenderer::createTrimeshImage(
            compactData, profile, surfaceAspectRatio, displayHeight);
    compactHeatmap.rows = compactData.rows;
    compactHeatmap.columns = compactData.columns;
    compactHeatmap.displayHeight = displayHeight;
    compactHeatmap.revision = bridge.getModel().getDerivedRevisions().compactPreview;
    compactHeatmap.domain = profile.getDomain();
    compactHeatmap.scalePolicy = profile.getScalePolicy();
    compactHeatmap.materialStyleSignature = profile.getSurfaceStyle().materialStyleSignature();
    compactHeatmap.midiNote = compactData.midiNote;
    compactHeatmap.pitchSpansColumns = compactData.pitchSpansColumns;
    compactHeatmap.surfaceAspectRatio = surfaceAspectRatio;
}

void TrimeshWidget::paintExpanded(Graphics& g, const Node& node, Rectangle<float> content) {
    setRenderProfile(displayProfile);
    const TrimeshRenderProfile profile = displayProfile;
    syncExpandedNode(node, content);
    const TrimeshRenderData& renderData = bridge.getDataSource().getRenderData();
    TrimeshNodeModel& model = bridge.getModel();

    const Rectangle<float> gridPanel = expandedGridPanelContentBounds(content, MeshDimensionality::dimensions(node).axisCount());
    const Rectangle<float> sidePanel = expandedSidePanelContentBounds(content, MeshDimensionality::dimensions(node).axisCount());
    const Rectangle<float> waveshapePanel = expandedWavePanelContentBounds(content, MeshDimensionality::dimensions(node).axisCount());

    const bool hasPanel3DHost = bridge.getPanel3DHostComponentIfCreated() != nullptr;
    const bool hasPanel2DHost = bridge.getPanel2DHostComponentIfCreated() != nullptr;

    if (!hasPanel3DHost) {
        Graphics::ScopedSaveState gridClip(g);

        g.reduceClipRegion(gridPanel.toNearestInt());
        TrimeshSurfaceRenderer::drawHeatmap(g, gridPanel, renderData, profile, true);
    }

    if (!hasPanel2DHost) {
        Graphics::ScopedSaveState waveshapeClip(g);

        g.reduceClipRegion(waveshapePanel.toNearestInt());
        TrimeshSliceRenderer2D::drawGrid(g, waveshapePanel, profile);
        TrimeshSliceRenderer2D::drawTraceFill(g, waveshapePanel.reduced(8.f), renderData.slice, profile);
        TrimeshSliceRenderer2D::drawTrace(
                g,
                waveshapePanel.reduced(8.f),
                renderData.slice,
                profile.getCurveStyle().positiveColour.toColour());
        TrimeshSliceRenderer2D::drawVertexMarkers(g, waveshapePanel.reduced(8.f), model.getVertexMarkers());
    }

    const auto selectedParameters = selectedVertexParameters();
    const auto dimensions = MeshDimensionality::dimensions(node);
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
                    parameters.boolValue("link.red", true)
            },
            TrimeshSidePanelRenderer::AxisState {
                    "Blue",
                    "B",
                    Colour(0xff5f91e8),
                    model.getMorphPosition().blue.getCurrentValue(),
                    primaryAxis == Vertex::Blue,
                    parameters.boolValue("link.blue", true)
            }
    };

    TrimeshSidePanelRenderer::drawSidePanel(
            g,
            sidePanel,
            axes,
            model.getSelectedCubePreviewVertices(),
            selectedParameters,
            guideAttachmentLabels,
            showsOutputScale(),
            profile.getDomain(),
            parameters.floatValue(
                    profile.getSliceStyle().isSpectral() ? "range" : "gain",
                    0.5f), dimensions.axisCount(), propertyFeedback);
}

void TrimeshWidget::renderExpandedPanelsOpenGL(
        const Node& node,
        Rectangle<float> content,
        float scaleFactor) {
    setRenderProfile(displayProfile);
    syncExpandedNode(node, content);
    if (MeshDimensionality::dimensions(node).axisCount() > 0) {
        bridge.renderPanel3D(expandedGridPanelContentBounds(content, MeshDimensionality::dimensions(node).axisCount()), scaleFactor);
    }
    bridge.renderPanel2D(expandedWavePanelContentBounds(content, MeshDimensionality::dimensions(node).axisCount()), scaleFactor);
}

Component* TrimeshWidget::prepareExpandedPanel3DComponent(
        const Node& node,
        Rectangle<float> content) {
    setRenderProfile(displayProfile);
    syncExpandedNode(node, content);
    return bridge.getPanel3DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel3DComponentIfCreated() const {
    return bridge.getPanel3DHostComponentIfCreated();
}

Component* TrimeshWidget::prepareExpandedPanel2DComponent(
        const Node& node,
        Rectangle<float> content) {
    setRenderProfile(displayProfile);
    syncExpandedNode(node, content);
    return bridge.getPanel2DHostComponent();
}

Component* TrimeshWidget::getExpandedPanel2DComponentIfCreated() const {
    return bridge.getPanel2DHostComponentIfCreated();
}

void TrimeshWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

int TrimeshWidget::resolvedSelectedVertexIndexForNode(const Node& node) {
    bridge.syncFromNode(node, kExpandedRows, lastExpandedColumnCount);
    return bridge.getModel().getResolvedSelectedVertexIndex();
}

void TrimeshWidget::setExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    bridge.setPanelHostDelegate(delegate);
}

void TrimeshWidget::clearExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    bridge.clearPanelHostDelegate(delegate);
    lastExpandedColumnCount = kPreviewColumns;
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
    return bridge.getModel().setVertexParameter(
            vertexIndex,
            parameterId,
            value,
            bridge.getInteractor2D().getCollisionDetector());
}

std::optional<std::array<float, 8>> TrimeshWidget::cubeCurveValuesForVertex(
        int vertexIndex) {
    return bridge.getModel().cubeCurveValuesForVertex(vertexIndex);
}

bool TrimeshWidget::setVertexGuideGain(
        int vertexIndex,
        const String& parameterId,
        float value) {
    return bridge.getModel().setVertexGuideGain(vertexIndex, parameterId, value);
}

bool TrimeshWidget::guideGainValueForParameter(
        const String& parameterId,
        float& value) {
    const int selectedVertex = bridge.getModel().getSelectedVertexIndex();
    if (selectedVertex < 0) {
        return false;
    }
    value = bridge.getModel().vertexGuideGain(selectedVertex, parameterId);
    return true;
}

bool TrimeshWidget::hasGuideAttachmentForParameter(const String& parameterId) const {
    const String field = parameterId.fromLastOccurrenceOf(".", false, false);
    const auto& fields = TrimeshGuideAttachmentTarget::fields();
    const auto found = std::find(fields.begin(), fields.end(), field);
    return found != fields.end()
            && guideAttachmentLabels[(size_t) std::distance(fields.begin(), found)].isNotEmpty();
}

std::vector<TrimeshVertexParameter> TrimeshWidget::vertexParametersForIndex(int vertexIndex) {
    return bridge.getModel().getVertexParametersForIndex(vertexIndex);
}

std::vector<TrimeshVertexParameter> TrimeshWidget::selectedVertexParameters() {
    auto parameters = bridge.getModel().getSelectedVertexParameters();
    if (parameters.empty()) {
        return parameters;
    }
    const auto& yellow = parameters.front();
    parameters.insert(parameters.end(),
            { "guide.time", "component curve", 0.f, 0.f, 1.f, yellow.guideGain, yellow.enabled, true });
    for (auto& parameter : parameters) {
        parameter.enabled &= vertexPropertyActive(parameter.id, bridge.getModel().getDimensionCount());
    }
    return parameters;
}

bool TrimeshWidget::vertexPropertyActive(const String& parameterId, int dimensionCount) {
    const String field = parameterId.fromLastOccurrenceOf(".", false, false);
    const int axis = field == "time" && parameterId == "vertex.time" ? Vertex::Time
            : field == "red" ? Vertex::Red : field == "blue" ? Vertex::Blue : -1;
    return axis < 0 || MorphDimensions::trimesh(dimensionCount).active(axis);
}

int TrimeshWidget::selectedVertexIndexForPanel() const {
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
    const TrimeshRenderData& renderData = bridge.getRenderData();
    stats.surfaceColumnCount = renderData.columns;
    stats.surfaceRowCount = renderData.rows;
    stats.surfaceRebuildCount = bridge.getDataSource().getRenderCounters().surfaceRebuilds;
    const auto& panelColumns = bridge.getDataSource().getPanelColumns();
    if (!panelColumns.empty()) {
        stats.firstPanelMidiNote = panelColumns.front().midiKey;
        stats.lastPanelMidiNote = panelColumns.back().midiKey;
        stats.pitchSpansColumns = stats.firstPanelMidiNote
                != stats.lastPanelMidiNote;
    }
    stats.sampleCount = samples.size();
    stats.interceptCount = (int) snapshot.intercepts().size();
    stats.hoveredInterceptIndex = bridge.getInteractor2D().state.currentIcpt;
    const Vertex* hoveredVertex = bridge.getInteractor2D().state.currentVertex;
    const auto& vertices = bridge.getModel().currentMesh().getVerts();
    for (int index = 0; index < (int) vertices.size(); ++index) {
        if (vertices[(size_t) index] == hoveredVertex) {
            stats.hoveredVertexIndex = index;
            break;
        }
    }
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

Rectangle<float> TrimeshWidget::controlRail(Rectangle<float> content, int axis) const {
    const int count = bridge.getModel().getDimensionCount();
    auto area = morphPanelBounds(content, count);
    if (showsOutputScale()) {
        area.removeFromBottom(52.f);
    }
    return MorphControlLayout(area, count, MorphDimensions::Plane::YellowRed).rail(axis);
}

bool TrimeshWidget::findMorphControlAt(
        Rectangle<float> content,
        Point<float> position,
        String& parameterId,
        float& value) const {
    const std::array<String, 3> parameterIds {
            String("yellow"),
            String("red"),
            String("blue")
    };

    for (int i = 0; i < (int) parameterIds.size(); ++i) {
        const Rectangle<float> rail = controlRail(content, i);
        if (rail.isEmpty()) {
            continue;
        }

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

        const Rectangle<float> rail = controlRail(content, i);
        if (rail.isEmpty()) {
            return false;
        }
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool TrimeshWidget::outputScaleValueAt(
        Rectangle<float> content,
        Point<float> position,
        float& value) const {
    const Rectangle<float> rail = TrimeshSidePanelRenderer::outputScaleRailBounds(
            morphPanelBounds(content, bridge.getModel().getDimensionCount()),
            bridge.getModel().getDimensionCount());
    value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
    return true;
}

bool TrimeshWidget::findPrimaryAxisAt(
        Rectangle<float> content,
        Point<float> position,
        String& axisValue) const {
    const auto regions = expandedControlHitRegions(content, showsOutputScale(),
            outputScaleParameterId(), bridge.getModel().getDimensionCount());
    for (const auto& region : regions) {
        if (region.kind == TrimeshExpandedHitRegionKind::PrimaryAxis
                && region.bounds.contains(position)) {
            axisValue = region.axisValue;
            return true;
        }
    }
    return false;
}

bool TrimeshWidget::findLinkToggleAt(
        Rectangle<float> content,
        Point<float> position,
        String& axisValue) const {
    const auto regions = expandedControlHitRegions(content, showsOutputScale(),
            outputScaleParameterId(), bridge.getModel().getDimensionCount());
    for (const auto& region : regions) {
        if (region.kind == TrimeshExpandedHitRegionKind::LinkToggle
                && region.bounds.contains(position)) {
            axisValue = region.axisValue;
            return true;
        }
    }
    return false;
}

bool TrimeshWidget::findVertexParameterAt(
        Rectangle<float> content,
        Point<float> position,
        String& parameterId,
        float& value) const {
    const Rectangle<float> parameterArea = vertexParameterPanelBounds(content);

    const auto dimensions = MorphDimensions::trimesh(bridge.getModel().getDimensionCount());
    for (int i = 0; i < kVertexParameterCount; ++i) {
        if (!vertexPropertyActive(vertexParameterId(i), dimensions.axisCount())) {
            continue;
        }
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i,
                        1.f, 7);
        const Rectangle<float> rail = vertexParameterRailBounds(row);

        if (!rail.expanded(5.f, 8.f).contains(position)) {
            continue;
        }

        if (vertexParameterId(i) == "guide.time") {
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

    const auto dimensions = MorphDimensions::trimesh(bridge.getModel().getDimensionCount());
    for (int i = 0; i < kVertexParameterCount; ++i) {
        if (!vertexPropertyActive(vertexParameterId(i), dimensions.axisCount())) {
            continue;
        }
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i,
                        1.f, 7);
        if (!TrimeshSidePanelRenderer::showsGuideControlsForParameter(vertexParameterId(i))) {
            continue;
        }
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

    const auto dimensions = MorphDimensions::trimesh(bridge.getModel().getDimensionCount());
    for (int i = 0; i < kVertexParameterCount; ++i) {
        if (!vertexPropertyActive(vertexParameterId(i), dimensions.axisCount())) {
            continue;
        }
        if (vertexParameterId(i) == "guide.time" || vertexParameterId(i) != parameterId) {
            continue;
        }

        const Rectangle<float> rail = vertexParameterRailBounds(
                TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i,
                        1.f, 7));
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
    const Rectangle<float> waveshape = waveshapeContentBounds(
            content, bridge.getModel().getDimensionCount());

    if (!waveshape.expanded(8.f).contains(position)) {
        return false;
    }

    const float phase = jlimit(0.f, 1.f, (position.x - waveshape.getX()) / waveshape.getWidth());
    const float amp = jlimit(0.f, 1.f, (waveshape.getBottom() - position.y) / waveshape.getHeight());
    vertexIndex = bridge.getModel().findNearestVertexIndexForPhaseAmp(phase, amp);
    return vertexIndex >= 0;
}

std::vector<TrimeshExpandedHitRegion> TrimeshWidget::expandedControlHitRegions(
        Rectangle<float> content,
        bool showOutputScale,
        const String& outputScaleParameter,
        int dimensionCount) {
    std::vector<TrimeshExpandedHitRegion> regions;
    const Rectangle<float> morphArea = morphPanelBounds(content, dimensionCount);
    auto reducedArea = morphArea;
    if (showOutputScale) {
        reducedArea.removeFromBottom(52.f);
    }
    const MorphControlLayout reduced(reducedArea, dimensionCount, MorphDimensions::Plane::YellowRed);
    const auto dimensions = MorphDimensions::trimesh(dimensionCount);
    const std::array<String, 3> parameterIds {
            String("yellow"),
            String("red"),
            String("blue")
    };

    for (int i = 0; i < (int) parameterIds.size(); ++i) {
        if (!dimensions.active(MorphDimensions::axes[(size_t) i])) {
            continue;
        }
        regions.push_back({
                TrimeshExpandedHitRegionKind::MorphControl,
                reduced.rail(i).expanded(8.f, 12.f),
                parameterIds[(size_t) i],
                {}
        });
    }

    if (showOutputScale) {
        regions.push_back({
                TrimeshExpandedHitRegionKind::OutputScale,
                TrimeshSidePanelRenderer::outputScaleRailBounds(morphArea, dimensionCount)
                        .expanded(8.f, 12.f),
                outputScaleParameter,
                {}
        });
    }

    for (int i = 0; i < 3; ++i) {
        if (dimensionCount <= 1 || !dimensions.active(MorphDimensions::axes[(size_t) i])) {
            continue;
        }
        regions.push_back({
                TrimeshExpandedHitRegionKind::PrimaryAxis,
                reduced.axis(i).expanded(4.f),
                {},
                primaryAxisValue(i)
        });
    }

    for (int i = 0; i < 3; ++i) {
        if (!dimensions.active(MorphDimensions::axes[(size_t) i])) {
            continue;
        }
        regions.push_back({
                TrimeshExpandedHitRegionKind::LinkToggle,
                reduced.link(i).expanded(4.f),
                {},
                primaryAxisValue(i)
        });
    }

    const Rectangle<float> parameterArea = reduced.vertices();

    for (int i = 0; i < kVertexParameterCount; ++i) {
        const String id = vertexParameterId(i);
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i, 1.f, 7);
        if (id != "guide.time") {
            regions.push_back({ TrimeshExpandedHitRegionKind::VertexParameter,
                    TrimeshSidePanelRenderer::vertexParameterRailBounds(row).expanded(5.f, 8.f), id, {} });
        }
        if (TrimeshSidePanelRenderer::showsGuideControlsForParameter(id)) {
            regions.push_back({ TrimeshExpandedHitRegionKind::VertexGuideGain,
                    TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row).expanded(3.f, 2.f),
                    guideGainParameterId(i), {} });
            regions.push_back({ TrimeshExpandedHitRegionKind::VertexGuideAttachment,
                    TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).expanded(4.f), id, {} });
        }
    }

    return regions;
}

Rectangle<float> TrimeshWidget::expandedMorphKeyboardFocusBounds(
        Rectangle<float> content,
        int axisIndex,
        bool showOutputScale,
        int dimensionCount) {
    return TrimeshSidePanelRenderer::keyboardFocusBounds(
            morphPanelBounds(content, dimensionCount),
            axisIndex,
            showOutputScale,
            dimensionCount);
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

Rectangle<float> TrimeshWidget::expandedSidePanelContentBounds(Rectangle<float> content, int dimensionCount) {
    auto topRow = content.removeFromTop(content.getHeight() * (dimensionCount == 0 ? 1.f : kExpandedTopRowRatio));
    const float width = jmax(topRow.getWidth() * (1.f - kExpandedGridRatio) - kExpandedPanelGap,
            MorphControlLayout::minimumWidth());
    return topRow.withLeft(topRow.getRight() - width);
}

Rectangle<float> TrimeshWidget::morphPanelBounds(Rectangle<float> content, int dimensionCount) {
    return expandedSidePanelContentBounds(content, dimensionCount);
}

Rectangle<float> TrimeshWidget::morphRailBounds(
        Rectangle<float> morphArea,
        int axisIndex,
        bool showOutputScale) {
    return TrimeshSidePanelRenderer::morphRailBounds(
            morphArea, axisIndex, showOutputScale);
}

Rectangle<float> TrimeshWidget::primaryAxisBounds(
        Rectangle<float> morphArea,
        int axisIndex,
        bool showOutputScale) {
    return TrimeshSidePanelRenderer::primaryAxisBounds(
            morphArea, axisIndex, showOutputScale);
}

Rectangle<float> TrimeshWidget::linkToggleBounds(
        Rectangle<float> morphArea,
        int axisIndex,
        bool showOutputScale) {
    return TrimeshSidePanelRenderer::linkToggleBounds(
            morphArea, axisIndex, showOutputScale);
}

String TrimeshWidget::primaryAxisValue(int axis) {
    switch (axis) {
        case 1:     return "red";
        case 2:     return "blue";
        case 0:
        default:    return "yellow";
    }
}

Rectangle<float> TrimeshWidget::vertexParameterPanelBounds(Rectangle<float> content) const {
    auto area = morphPanelBounds(content, bridge.getModel().getDimensionCount());
    if (showsOutputScale()) {
        area.removeFromBottom(52.f);
    }
    return MorphControlLayout(area, bridge.getModel().getDimensionCount()).vertices();
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
        case 6:     return "guide.time";
        default:    return {};
    }
}

String TrimeshWidget::guideGainParameterId(int parameterIndex) {
    const String parameterId = vertexParameterId(parameterIndex);
    return parameterId.isNotEmpty()
            ? "guideGain." + parameterId.fromLastOccurrenceOf(".", false, false)
            : String();
}

Rectangle<float> TrimeshWidget::waveshapeContentBounds(Rectangle<float> content, int dimensionCount) {
    return expandedWavePanelContentBounds(content, dimensionCount);
}

Rectangle<float> TrimeshWidget::staticEditorBounds(Rectangle<float> fullBounds) {
    const float chromeHeight = CanvasChromeMetrics::embeddedEditorHeaderHeight + 16.f;
    return fullBounds.withHeight(chromeHeight + (fullBounds.getHeight() - chromeHeight) * kExpandedTopRowRatio);
}

Rectangle<float> TrimeshWidget::expandedGridPanelContentBounds(Rectangle<float> content, int dimensionCount) {
    if (dimensionCount == 0) {
        return {};
    }
    const auto sidePanel = expandedSidePanelContentBounds(content, dimensionCount);
    auto topRow = content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    return topRow.withRight(sidePanel.getX() - kExpandedPanelGap);
}

int TrimeshWidget::expandedColumnCount(Rectangle<float>) {
    return kExpandedColumns;
}

void TrimeshWidget::syncExpandedNode(const Node& node, Rectangle<float> content) {
    lastExpandedColumnCount = expandedColumnCount(content);
    bridge.syncFromNode(node, kExpandedRows, lastExpandedColumnCount);
}

Rectangle<float> TrimeshWidget::expandedWavePanelContentBounds(Rectangle<float> content, int dimensionCount) {
    if (dimensionCount == 0) {
        const auto controls = expandedSidePanelContentBounds(content, 0).withTrimmedBottom(52.f);
        const auto properties = MorphControlLayout(controls, 0).vertices();
        return content.withRight(properties.getX() - kExpandedPanelGap);
    }
    content.removeFromTop(content.getHeight() * kExpandedTopRowRatio);
    content.removeFromTop(kExpandedPanelGap);
    return content;
}

}
