#pragma once

#include "NodeGraph.h"

namespace CycleV2 {

enum class GraphLoadCode {
    InvalidXml,
    UnsupportedVersion,
    UnknownNodeType,
    InvalidGraph
};

struct GraphLoadIssue {
    GraphLoadCode code {};
    String message;
};

struct GraphLoadResult {
    NodeGraph graph;
    std::vector<GraphLoadIssue> issues;

    bool succeeded() const { return issues.empty(); }
};

class GraphSerializer {
public:
    static constexpr int currentFormatVersion = 2;

    ValueTree toValueTree(const NodeGraph& graph) const;
    NodeGraph fromValueTree(const ValueTree& tree) const;
    GraphLoadResult loadValueTree(const ValueTree& tree) const;
    String toXmlString(const NodeGraph& graph) const;
    NodeGraph fromXmlString(const String& xml) const;
    GraphLoadResult loadXmlString(const String& xml) const;
};

}
