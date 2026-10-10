#pragma once

#include "Graph/NodeGraph.h"
#include "Graph/PresetPresentation.h"

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
    PresetPresentation presentation;
    String presentationWarning;
    std::vector<GraphLoadIssue> issues;

    bool succeeded() const { return issues.empty(); }
};

class GraphSerializer {
public:
    static constexpr int currentFormatVersion = 7;

    var writeJSON(const NodeGraph& graph) const;
    var writeJSON(
            const NodeGraph& graph,
            const PresetPresentation& presentation) const;
    GraphLoadResult readJSON(const var& value) const;
    String toJsonString(const NodeGraph& graph) const;
    String toJsonString(
            const NodeGraph& graph,
            const PresetPresentation& presentation) const;
    String toJsonString(const var& graphRepresentation) const;
    NodeGraph fromJsonString(const String& json) const;
    GraphLoadResult loadJsonString(const String& json) const;
};

}
