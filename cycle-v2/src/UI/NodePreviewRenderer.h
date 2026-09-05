#pragma once

#include <optional>

#include "UI/NodePreviewResources.h"
#include "UI/OutputMeterPresentation.h"

#include "Nodes/Unison/UnisonPreviewPainter.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct NodePreviewRenderRequest {
    const Node& node;
    const NodePreviewResult* runtimeResult {};
    Rectangle<float> area;
    TrimeshRenderProfile profile;
    float zoom {};
    bool cache { true };
    UnisonPreviewContext unisonContext;
    bool highQuality {};
    std::optional<OutputMeterLevels> liveOutputLevels;
};

class NodePreviewRenderer {
public:
    explicit NodePreviewRenderer(NodePreviewResources& resources);

    static bool requiresCurveModel(NodeKind kind);
    static Image createRuntimeHeatmapImage(
            const NodePreviewResult& preview,
            bool desaturated = false);
    static Image createRuntimeHeatmapImage(
            const NodePreviewResult& preview,
            const TrimeshRenderProfile& profile,
            bool desaturated = false);

    Rectangle<float> boundsFor(
            const Node& node,
            Rectangle<float> nodeBounds,
            float zoom) const;
    void paint(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool renderOpenGL(
            const Node& node,
            Rectangle<float> area,
            float scaleFactor);
    uint64_t nodePresentationFingerprint(const String& nodeId) const;

private:
    bool paintAuthoritativeModel(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool paintRuntimeResult(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool paintRuntimeHeatmap(Graphics& graphics, const NodePreviewRenderRequest& request);
    void paintQualitative(Graphics& graphics, const NodePreviewRenderRequest& request);
    void paintUncached(Graphics& graphics, const NodePreviewRenderRequest& request);
    bool paintCachedHeatmap(Graphics& graphics, const NodePreviewRenderRequest& request);

    NodePreviewResources& resources;
};

}
