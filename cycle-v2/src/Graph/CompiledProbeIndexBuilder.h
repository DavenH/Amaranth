#pragma once

namespace CycleV2 {

class NodeGraph;
struct GraphExecutionPlan;

class CompiledProbeIndexBuilder final {
public:
    static void refresh(const NodeGraph& graph, GraphExecutionPlan& plan);
};

}
