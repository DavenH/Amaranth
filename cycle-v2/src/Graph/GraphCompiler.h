#pragma once

#include "GraphValidator.h"
#include "../Runtime/NodeModuleRegistry.h"

#include <vector>

namespace CycleV2 {

enum class GraphCompileCode {
    CycleDetected
};

struct GraphCompileIssue {
    GraphCompileCode code {};
    String message;
};

struct GraphStepInput {
    String sourceNodeId;
    String sourcePortId;
    String destPortId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct GraphBufferPlan {
    String id;
    String sourceNodeId;
    String sourcePortId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct GraphExecutionStep {
    String nodeId;
    NodeKind kind { NodeKind::GenericProcessor };
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    bool previewable {};
    bool cycle1AdapterBacked {};
    String cycle1Reference;
    std::vector<NodeParameter> parameters;
    std::vector<GraphStepInput> inputs;
};

struct GraphExecutionPlan {
    std::vector<String> nodeOrder;
    std::vector<GraphExecutionStep> steps;
    std::vector<GraphBufferPlan> buffers;
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
    NodeModuleRegistry moduleRegistry;
};

}
