#pragma once

#include "GraphValidator.h"

#include <vector>

namespace CycleV2 {

enum class GraphCompileCode {
    CycleDetected
};

struct GraphCompileIssue {
    GraphCompileCode code {};
    String message;
};

struct GraphExecutionPlan {
    std::vector<String> nodeOrder;
    std::vector<Edge> signalEdges;
    std::vector<Edge> attachments;
};

struct GraphCompileResult {
    GraphExecutionPlan plan;
    std::vector<GraphValidationIssue> validationIssues;
    std::vector<GraphCompileIssue> compileIssues;

    bool succeeded() const;
};

class GraphCompiler {
public:
    GraphCompileResult compile(const NodeGraph& graph) const;

private:
    GraphValidator validator;
};

}
