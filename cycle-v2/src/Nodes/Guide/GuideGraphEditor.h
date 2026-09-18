#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

class GuideGraphEditor {
public:
    GraphEditResult createGuideCurve(NodeGraph& graph) const;
    GraphEditResult duplicateGuideCurve(NodeGraph& graph, const String& guideId) const;
    GraphEditResult reorderGuideCurve(NodeGraph& graph, const String& guideId, int shelfOrder) const;
    GraphEditResult removeGuideCurve(NodeGraph& graph, const String& guideId) const;
    GraphEditResult renameGuideCurve(
            NodeGraph& graph,
            const String& guideId,
            const String& name) const;
    GraphEditResult replaceGuideCurve(
            NodeGraph& graph,
            const String& guideId,
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) const;
    GraphEditResult setGuideHeatmap(
            NodeGraph& graph,
            const String& guideId,
            GuideHeatmapAssetPtr asset) const;
    GraphEditResult clearGuideHeatmap(NodeGraph& graph, const String& guideId) const;
    GraphEditResult assignGuideCurveToMeshComponent(
            NodeGraph& graph,
            const String& guideId,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField) const;
    GraphEditResult detachGuideCurveFromMeshComponent(
            NodeGraph& graph,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField) const;
    GraphEditResult createGuideCurveAndAssignToMeshComponent(
            NodeGraph& graph,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField) const;
};

}
