#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

struct GraphSpliceValidation {
    GraphEditCode code { GraphEditCode::Connected };
    Edge incomingEdge;
    Edge outgoingEdge;

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

class GraphSpliceValidator {
public:
    GraphSpliceValidation validate(
            const NodeGraph& graph,
            size_t edgeIndex,
            const String& nodeId) const;
};

}
