#pragma once

#include "Graph/GraphEditTypes.h"

class MorphDimensions;

namespace CycleV2 {

class NodeGraph;

// Each new cube stores its previous ordinal, or -1 for a newly created cube.
void remapMeshGuideAssignments(NodeGraph& graph, const String& nodeId,
        const std::vector<int>& retainedCubeIndices, GraphEditResult& result,
        const MorphDimensions* retainedDimensions = nullptr);

}
