#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "Graph/GraphEditTypes.h"
#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"
#include "Runtime/PresentationRefreshPolicy.h"

namespace CycleV2 {

class GraphCommandDispatcher;
class GraphDocument;

struct NodeCanvasGuideEditorCallbacks {
    std::function<bool(const String&, GuideHeatmapAssetPtr, uint64_t)> setHeatmap;
    std::function<bool(const String&, uint64_t)> clearHeatmap;
    std::function<void()> refreshCompiledState;
    std::function<void()> scheduleCompiledStateRefresh;
    std::function<void()> requestCanvasRepaint;
    std::function<void()> notifyOcclusionChanged;
};

class NodeCanvasGuideEditorCoordinator {
public:
    NodeCanvasGuideEditorCoordinator(
            Component& owner,
            GraphDocument& document,
            GraphCommandDispatcher& commands,
            CurveExpandedEditorDelegate& delegate,
            NodeCanvasGuideEditorCallbacks callbacks);

    void open(const String& guideId, Rectangle<float> availableBounds);
    void close();
    void rebind();
    void layout(Rectangle<float> availableBounds);
    void renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    void repaint();

    bool publishCurveState(
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls,
            ProbeRefreshMode refreshMode);
    void beginTransaction();
    void commitTransaction();

    bool isOpen() const;
    const String& guideId() const { return expandedGuideId; }
    Rectangle<float> bounds() const;
    var automationState() const;
    std::vector<std::pair<String, Rectangle<float>>> automationPointerTargets() const;

private:
    void ensureEditor();

    Component& owner;
    GraphDocument& document;
    GraphCommandDispatcher& commands;
    CurveExpandedEditorDelegate& delegate;
    NodeCanvasGuideEditorCallbacks callbacks;
    std::unique_ptr<CurveEditorWidget> widget;
    std::unique_ptr<GuideCurveEditorComponent> editor;
    String expandedGuideId;
    std::optional<uint64_t> transactionBaseRevision;
};

}
