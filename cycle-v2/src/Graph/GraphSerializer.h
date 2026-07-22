#pragma once

#include "NodeGraph.h"

namespace CycleV2 {

enum class GraphLoadCode {
    InvalidJson,
    InvalidSchema,
    UnsupportedVersion,
    UnknownNodeType,
    InvalidParameter,
    InvalidModel,
    DuplicateIdentity,
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
    static constexpr int currentFormatVersion = 1;

    var writeJSON(const NodeGraph& graph) const;
    GraphLoadResult readJSON(const var& value) const;
    String toJsonString(const NodeGraph& graph) const;
    NodeGraph fromJsonString(const String& json) const;
    GraphLoadResult loadJsonString(const String& json) const;
};

}
