#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

class GraphValidationContext;

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
    GraphSpliceValidation validateAfterLayoutChanges(
            const NodeGraph& graph,
            const GraphValidationContext& context,
            size_t edgeIndex,
            const String& nodeId) const;
};

}
