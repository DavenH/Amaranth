#pragma once

#include <JuceHeader.h>

#include <memory>

#include "Graph/NodeGraph.h"
#include "Nodes/Curve/Panel/CurvePanelController.h"

namespace CycleV2 {

class CurveEditorWidget {
public:
    explicit CurveEditorWidget(NodeKind nodeKind);
    explicit CurveEditorWidget(bool guideResource);
    ~CurveEditorWidget();

    Component* prepareExpandedPanelComponent(const Node& node, Rectangle<float> contentBounds);
    Component* getExpandedPanelComponentIfCreated();
    void setDelegate(CurvePanelControllerDelegate* delegate);
    void setControlValues(bool enabled, float firstValue, float secondValue, float thirdValue, int menuId);
    void setEnvelopeBipolar(bool bipolar);
    void setEnvelopeLogarithmic(bool shouldUseLogarithmicScale);
    void setEnvelopeAxisLinks(bool redLinked, bool blueLinked);
    void setEnvelopeDimensions(int count, bool yellowLinked);
    Component* envelopeMorphSurfaceComponent();
    void prepareEnvelopeMorphSurface(int axis, const MorphPosition& morph);
    void renderEnvelopeMorphSurface(Rectangle<float> bounds, float scaleFactor);
    void setEditGestureActive(bool active);
    void setPreviewMorphPosition(std::optional<MorphPosition> position);
    std::optional<MorphPosition> effectiveMorphPosition() const;
    Rectangle<float> panelContentBounds(Rectangle<float> frame) const;
    void fitEnvelopeVerticalRange();
    void resetEnvelopeVerticalRange();
    void setImpulseResponseAudioResource(const AudioSampleResource* resource);
    void zoomImpulseResponseToAttack();
    void resetImpulseResponseZoom();
    void syncFromNode(const Node& node);
    void restoreFromNode(const Node& node);
    void syncGuideContext(const NodeGraph& graph, const Node& node);
    void syncFromGuideResource(
            const GuideCurveResource& guide,
            const GuideHeatmapAssetPtr& heatmap = nullptr);
    void renderExpandedPanelOpenGL(
            const Node& node,
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor);
    void renderGuideExpandedPanelOpenGL(
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor);
    bool renderGuidePreviewSnapshotOpenGL(Rectangle<float> bounds, float scaleFactor);
    bool renderPreviewSnapshotOpenGL(
            const Node& node,
            Rectangle<float> bounds,
            float scaleFactor);
    bool paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const;
    bool hasVisiblePreviewSnapshot() const;
    void resetDocumentPresentation();
    void releaseSharedGlResources();
    int vertexCountForAutomation() const;
    var automationState() const;
    std::vector<CurvePanelGridLine> verticalMajorGridLines() const;
    std::vector<CurvePreviewVertex> previewVertices();
    std::optional<FlatCurveVertex> pointPreview();
    String serializedMeshState();
    NodeModelStatePtr modelPublication();
    NodeModelStatePtr prepareModelPublication(uint64_t currentRevision);
    uint64_t modelRevision() const;
    uint64_t contentRevision() const;
    uint64_t previewRevision() const { return previewPresentationRevision; }
    uint64_t previewSnapshotRevision() const;
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const;
    std::vector<TrimeshCubePreviewVertex> morphPreviewVertices() const;
    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue);
    int selectedEnvelopeGuideCubeIndex();
    bool hasSingleSelectedEnvelopeVertex();
    bool selectedEnvelopeMarkerState(bool loopMarker) const;
    void toggleSelectedEnvelopeMarker(bool loopMarker);

private:
    NodeKind kind;
    bool guideResource {};
    uint64_t previewPresentationRevision { 1 };
    std::unique_ptr<CurvePanelController> controller;
};

}
