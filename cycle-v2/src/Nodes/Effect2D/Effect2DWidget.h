#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DPanelBridge.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace CycleV2 {

class Effect2DWidget {
public:
    explicit Effect2DWidget(NodeKind nodeKind);
    ~Effect2DWidget();

    Component* prepareExpandedPanelComponent(const Node& node, Rectangle<float> contentBounds);
    Component* getExpandedPanelComponentIfCreated();
    void setExpandedPanelCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
    void setMeshEditedCallback(std::function<void()> callback);
    void setControlValues(bool enabled, float firstValue, float secondValue, float thirdValue, int menuId);
    void setEnvelopeLogarithmic(bool shouldUseLogarithmicScale);
    void setEnvelopeAxisLinks(bool redLinked, bool blueLinked);
    void syncFromNode(const Node& node);
    void renderExpandedPanelOpenGL(
            const Node& node,
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor);
    void renderPreviewSnapshotOpenGL(const Node& node, Rectangle<float> bounds, float scaleFactor);
    bool paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const;
    void releaseSharedGlResources();
    int vertexCountForAutomation() const;
    var automationState() const;
    std::vector<Effect2DPanelBridge::PreviewVertex> previewVertices();
    String serializedMeshState();
    String serializedModelSnapshot();
    uint64_t modelRevision() const;
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const;
    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue);
    bool selectedEnvelopeMarkerState(bool loopMarker) const;
    void toggleSelectedEnvelopeMarker(bool loopMarker);

private:
    NodeKind kind;
    Effect2DPanelBridge bridge;
};

}
