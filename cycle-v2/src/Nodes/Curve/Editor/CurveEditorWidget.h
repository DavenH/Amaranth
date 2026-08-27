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
    void fitEnvelopeVerticalRange();
    void resetEnvelopeVerticalRange();
    void syncFromNode(const Node& node);
    void syncFromGuideResource(const GuideCurveResource& guide);
    void renderExpandedPanelOpenGL(
            const Node& node,
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor);
    void renderGuideExpandedPanelOpenGL(
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor);
    void renderGuidePreviewSnapshotOpenGL(Rectangle<float> bounds, float scaleFactor);
    void renderPreviewSnapshotOpenGL(const Node& node, Rectangle<float> bounds, float scaleFactor);
    bool paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const;
    void releaseSharedGlResources();
    int vertexCountForAutomation() const;
    var automationState() const;
    std::vector<CurvePreviewVertex> previewVertices();
    String serializedMeshState();
    NodeModelStatePtr modelPublication();
    NodeModelStatePtr prepareModelPublication(uint64_t currentRevision);
    uint64_t modelRevision() const;
    uint64_t contentRevision() const;
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const;
    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue);
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
