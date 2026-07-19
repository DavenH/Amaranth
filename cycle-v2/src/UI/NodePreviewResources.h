#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "../Graph/NodeGraph.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"

namespace CycleV2 {

class NodeEditorCommandService;

struct CachedNodePreviewSprite {
    Image image;
    String signature;
    Image runtimeHeatmap;
    String runtimeHeatmapSignature;
    PortDomain domain { PortDomain::ControlSignal };
    int width {};
    int height {};
};

class NodePreviewResources {
public:
    explicit NodePreviewResources(NodeEditorCommandService& commands);

    TrimeshWidget& trimeshWidget(const String& nodeId);
    Effect2DWidget& effect2DWidget(const Node& node);
    CachedNodePreviewSprite& cachedSprite(const String& nodeId);

    const TrimeshWidget* findTrimeshWidget(const String& nodeId) const;
    void clearCachedSprites();
    void releaseOpenGLResources();
    void hideExpandedHostsExcept(const String& nodeId);
    void detachTrimeshHosts(Component& parent);

private:
    NodeEditorCommandService& editorCommands;
    std::vector<std::pair<String, std::unique_ptr<TrimeshWidget>>> trimeshWidgets;
    std::vector<std::pair<String, std::unique_ptr<Effect2DWidget>>> effect2DWidgets;
    std::vector<std::pair<String, CachedNodePreviewSprite>> cachedSprites;
};

}
