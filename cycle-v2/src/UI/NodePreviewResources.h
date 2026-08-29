#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "Graph/NodeGraph.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"

namespace CycleV2 {

class NodeEditorCommandService;

struct CachedNodePreviewSprite {
    Image image;
    String signature;
    Image runtimeHeatmap;
    String runtimeHeatmapSignature;
    PortDomain domain { PortDomain::ControlSignal };
    RenderScalePolicy scalePolicy { RenderScalePolicy::Bipolar };
    int width {};
    int height {};
};

class NodePreviewResources {
public:
    explicit NodePreviewResources(NodeEditorCommandService& commands);

    TrimeshWidget& trimeshWidget(const String& nodeId);
    TrimeshWidget& trimeshWidget(const Node& node);
    TrimeshWidget* findTrimeshWidget(const String& nodeId);
    void setGraph(const NodeGraph* graphToUse) { graph = graphToUse; }
    CurveEditorWidget& curveEditorWidget(const Node& node);
    void syncCurveEditorWidget(const Node& node);
    CachedNodePreviewSprite& cachedSprite(const String& nodeId);

    const TrimeshWidget* findTrimeshWidget(const String& nodeId) const;
    void clearCachedSprites();
    void releaseOpenGLResources();
    void hideExpandedHostsExcept(const String& nodeId);
    void detachTrimeshHosts(Component& parent);

private:
    NodeEditorCommandService& editorCommands;
    const NodeGraph* graph {};
    std::vector<std::pair<String, std::unique_ptr<TrimeshWidget>>> trimeshWidgets;
    std::vector<std::pair<String, std::unique_ptr<CurveEditorWidget>>> curveEditorWidgets;
    std::vector<std::pair<String, CachedNodePreviewSprite>> cachedSprites;
};

}
