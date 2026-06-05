#include "NodeUpdateGraph.h"

namespace CycleV2 {

NodeUpdateResult NodeUpdateGraph::invalidateTrimeshChange(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const TrimeshChange& change) const {
    NodeUpdateResult result;
    result.graph = graphInvalidation.invalidateFrom(plan, nodeId, graphChangeKindFor(change));
    result.trimesh = trimeshInvalidation.invalidate(change);
    return result;
}

GraphChangeKind NodeUpdateGraph::graphChangeKindFor(const TrimeshChange& change) const {
    switch (change.kind) {
        case TrimeshChangeKind::None:
        case TrimeshChangeKind::Morph:
        case TrimeshChangeKind::PrimaryAxis:
        case TrimeshChangeKind::RenderProfile:
        case TrimeshChangeKind::Layout:
            return GraphChangeKind::NodeParameters;

        case TrimeshChangeKind::MeshEdit:
        case TrimeshChangeKind::VertexEdit:
            return GraphChangeKind::NodeContent;
    }

    return GraphChangeKind::NodeParameters;
}

}
