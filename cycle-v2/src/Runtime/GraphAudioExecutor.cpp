#include "GraphAudioExecutor.h"

namespace CycleV2 {

GraphAudioResult GraphAudioExecutor::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount) const {
    std::vector<std::pair<String, AudioProcessBlock>> outputs;
    outputs.reserve(plan.steps.size());

    NodeAudioProcessorFactory processorFactory;
    GraphAudioResult result;

    for (const auto& step : plan.steps) {
        auto processor = processorFactory.create(step.audioRole);

        if (processor == nullptr) {
            continue;
        }

        AudioProcessContext context;
        context.frameCount = frameCount;
        context.parameters = step.parameters;

        for (const auto& edge : plan.signalEdges) {
            if (edge.destNodeId != step.nodeId) {
                continue;
            }

            const AudioProcessBlock* sourceOutput = findOutputForNode(outputs, edge.sourceNodeId);
            if (sourceOutput != nullptr) {
                context.inputs.push_back(*sourceOutput);
            }
        }

        processor->prepare(frameCount);
        processor->process(context);
        outputs.push_back({ step.nodeId, std::move(context.output) });

        if (isOutputNode(graph, step.nodeId)) {
            result.output = outputs.back().second;
        }
    }

    return result;
}

const AudioProcessBlock* GraphAudioExecutor::findOutputForNode(
        const std::vector<std::pair<String, AudioProcessBlock>>& outputs,
        const String& nodeId) const {
    for (auto it = outputs.rbegin(); it != outputs.rend(); ++it) {
        if (it->first == nodeId) {
            return &it->second;
        }
    }

    return nullptr;
}

bool GraphAudioExecutor::isOutputNode(const NodeGraph& graph, const String& nodeId) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == nodeId) {
            return node.kind == NodeKind::Output;
        }
    }

    return false;
}

}
