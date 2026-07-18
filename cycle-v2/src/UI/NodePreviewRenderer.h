#pragma once

#include "NodePreviewResources.h"

#include "../Nodes/Trimesh/TrimeshRenderProfile.h"
#include "../Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct NodePreviewRenderRequest {
    const Node& node;
    const NodePreviewResult* runtimeResult {};
    Rectangle<float> area;
    TrimeshRenderProfile profile;
    float zoom {};
    bool cache { true };
};

class NodePreviewRenderer {
public:
    explicit NodePreviewRenderer(NodePreviewResources& resources);

    static bool requiresEffect2DModel(NodeKind kind);

    Rectangle<float> boundsFor(
            const Node& node,
            Rectangle<float> nodeBounds,
            float zoom) const;
    void paint(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool renderOpenGL(
            const Node& node,
            Rectangle<float> area,
            float scaleFactor);

private:
    bool paintAuthoritativeModel(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool paintRuntimeResult(Graphics& graphics, const NodePreviewRenderRequest& request);
    void paintQualitative(Graphics& graphics, const NodePreviewRenderRequest& request);
    void paintUncached(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool paintCachedHeatmap(Graphics& graphics, const NodePreviewRenderRequest& request);

    NodePreviewResources& resources;
};

}
