#pragma once

#include "../Graph/GraphCompiler.h"

#include <vector>

namespace CycleV2 {

struct RuntimeInput {
    String sourceNodeId;
    String sourcePortId;
    String destPortId;
    PortDomain domain {};
};

struct RuntimeNodeTrace {
    String nodeId;
    NodeKind kind { NodeKind::GenericProcessor };
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    bool previewable {};
    bool cycle1AdapterBacked {};
    std::vector<NodeParameter> parameters;
    std::vector<RuntimeInput> signalInputs;
    std::vector<RuntimeInput> attachments;
};

struct RuntimeProcessTrace {
    std::vector<RuntimeNodeTrace> nodes;
};

class GraphRuntime {
public:
    RuntimeProcessTrace process(const NodeGraph& graph, const GraphExecutionPlan& plan) const;

private:
    const Node* findNode(const NodeGraph& graph, const String& nodeId) const;
    std::vector<RuntimeInput> collectInputs(
            const std::vector<Edge>& edges,
            const String& nodeId) const;
};

}
