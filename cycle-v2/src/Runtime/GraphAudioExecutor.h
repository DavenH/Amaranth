#pragma once

#include "NodeAudioProcessor.h"
#include "GraphRuntime.h"

namespace CycleV2 {

struct NodeAudioResult {
    String nodeId;
    AudioProcessBlock output;
    std::vector<std::pair<String, AudioProcessBlock>> outputs;
};

struct GraphAudioResult {
    AudioProcessBlock output;
    std::vector<NodeAudioResult> nodes;
};

class GraphAudioExecutor {
public:
    GraphAudioResult process(const NodeGraph& graph, const GraphExecutionPlan& plan, size_t frameCount) const;

private:
    struct PortOutput {
        String nodeId;
        String portId;
        AudioProcessBlock block;
    };

    const AudioProcessBlock* findOutputForNode(
            const std::vector<PortOutput>& outputs,
            const String& nodeId,
            const String& portId) const;
    bool isOutputNode(const NodeGraph& graph, const String& nodeId) const;
};

}
