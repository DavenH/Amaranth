#include "TrimeshGuideCableBundle.h"

#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {
namespace {

bool isTrimeshGuideAssignment(const NodeGraph& graph, const Edge& edge) {
    const Node* source = graph.findNode(edge.sourceNodeId);
    const Node* destination = graph.findNode(edge.destNodeId);
    return edge.isProcessingAttachment()
            && edge.attachmentType == AttachmentType::GuideCurve
            && source != nullptr
            && source->kind == NodeKind::GuideCurve
            && destination != nullptr
            && destination->kind == NodeKind::TrilinearMesh
            && TrimeshGuideAttachmentTarget::parse(edge.destPortId).isValid();
}

}

std::vector<int> TrimeshGuideCableBundle::edgeIndices(
        const NodeGraph& graph,
        int edgeIndex) {
    const auto& edges = graph.getEdges();
    if (!isPositiveAndBelow(edgeIndex, (int) edges.size())) {
        return {};
    }

    const Edge& selected = edges[(size_t) edgeIndex];
    if (!isTrimeshGuideAssignment(graph, selected)) {
        return { edgeIndex };
    }

    std::vector<int> result;
    for (int index = 0; index < (int) edges.size(); ++index) {
        const Edge& edge = edges[(size_t) index];
        if (isTrimeshGuideAssignment(graph, edge)
                && edge.sourceNodeId == selected.sourceNodeId
                && edge.sourcePortId == selected.sourcePortId
                && edge.destNodeId == selected.destNodeId) {
            result.push_back(index);
        }
    }
    return result;
}

std::optional<std::vector<int>> TrimeshGuideCableBundle::bundleBeginningAt(
        const NodeGraph& graph,
        int edgeIndex) {
    auto indices = edgeIndices(graph, edgeIndex);
    if (indices.size() < 2 || indices.front() != edgeIndex) {
        return std::nullopt;
    }
    return indices;
}

}
