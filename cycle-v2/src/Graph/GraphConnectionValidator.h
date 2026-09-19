#pragma once

#include <vector>

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

struct GraphConnectionProposal {
    GraphEditCode code { GraphEditCode::Connected };
    Edge edge;
    PortAddress source;
    PortAddress destination;

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

struct GraphConnectionValidation : GraphConnectionProposal {
    std::vector<GraphValidationIssue> issues;
};

class GraphConnectionValidator {
public:
    GraphConnectionProposal propose(
            const NodeGraph& graph,
            const PortAddress& first,
            const PortAddress& second) const;
    GraphConnectionValidation validate(
            const NodeGraph& graph,
            const PortAddress& first,
            const PortAddress& second) const;
    std::vector<size_t> edgeIndicesToInput(
            const NodeGraph& graph,
            const PortAddress& destination) const;

private:
    const Port* findPort(const Node& node, const String& portId, bool input) const;
};

}
