#pragma once

#include "NodeAudioProcessor.h"
#include "GraphRuntime.h"

namespace CycleV2 {

struct NodeAudioResult {
    String nodeId;
    AudioProcessBlock output;
};

struct GraphAudioResult {
    AudioProcessBlock output;
    std::vector<NodeAudioResult> nodes;
};

class GraphAudioExecutor {
public:
    GraphAudioResult process(const NodeGraph& graph, const GraphExecutionPlan& plan, size_t frameCount) const;

private:
    const AudioProcessBlock* findOutputForNode(
            const std::vector<std::pair<String, AudioProcessBlock>>& outputs,
            const String& nodeId) const;
    bool isOutputNode(const NodeGraph& graph, const String& nodeId) const;
};

}
