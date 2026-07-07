#pragma once

#include "NodeAudioProcessor.h"
#include "GraphRuntime.h"

namespace CycleV2 {

struct NodeAudioResult {
    String nodeId;
    SignalPayload output;
    std::vector<std::pair<String, SignalPayload>> outputs;
};

struct GraphAudioResult {
    SignalPayload output;
    std::vector<NodeAudioResult> nodes;
};

class GraphAudioExecutor {
public:
    GraphAudioResult process(const NodeGraph& graph, const GraphExecutionPlan& plan, size_t frameCount) const;
    GraphAudioResult process(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing) const;

private:
    struct PortOutput {
        String nodeId;
        String portId;
        SignalPayload payload;
    };

    const SignalPayload* findOutputForNode(
            const std::vector<PortOutput>& outputs,
            const String& nodeId,
            const String& portId) const;
    bool isOutputNode(const NodeGraph& graph, const String& nodeId) const;
};

}
