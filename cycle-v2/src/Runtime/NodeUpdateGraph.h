#pragma once

#include "GraphInvalidation.h"
#include "../Nodes/Trimesh/TrimeshInvalidation.h"

namespace CycleV2 {

struct NodeUpdateResult {
    GraphInvalidationResult graph;
    TrimeshInvalidationResult trimesh;

    bool requiresRecompile() const { return graph.requiresRecompile; }
    bool dirtiesAudio() const { return !graph.audioNodes.empty(); }
    bool dirtiesPreview() const { return !graph.previewNodes.empty() || trimesh.dirtyCompactPreview; }
};

class NodeUpdateGraph {
public:
    NodeUpdateResult invalidateTrimeshChange(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            const TrimeshChange& change) const;

private:
    GraphChangeKind graphChangeKindFor(const TrimeshChange& change) const;
    GraphInvalidation graphInvalidation;
    TrimeshInvalidation trimeshInvalidation;
};

}
