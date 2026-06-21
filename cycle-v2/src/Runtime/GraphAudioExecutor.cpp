#include "GraphAudioExecutor.h"

namespace CycleV2 {

GraphAudioResult GraphAudioExecutor::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount) const {
    std::vector<PortOutput> outputs;
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
        context.outputPorts.reserve(step.outputs.size());

        for (const auto& output : step.outputs) {
            context.outputPorts.push_back({
                    output.portId,
                    output.domain,
                    output.channelLayout
            });
        }

        for (const auto& input : step.inputs) {
            if (input.destPortIndex < 0) {
                continue;
            }

            const auto inputIndex = (size_t) input.destPortIndex;
            if (context.inputs.size() <= inputIndex) {
                context.inputs.resize(inputIndex + 1);
            }

            const SignalPayload* sourceOutput = findOutputForNode(
                    outputs,
                    input.sourceNodeId,
                    input.sourcePortId);
            if (sourceOutput != nullptr) {
                context.inputs[inputIndex] = *sourceOutput;
                context.inputs[inputIndex].domain = input.domain;
                context.inputs[inputIndex].channelLayout = input.channelLayout;
            }
        }

        processor->prepare(frameCount);
        processor->process(context);

        std::vector<std::pair<String, SignalPayload>> nodeOutputs;
        for (size_t i = 0; i < context.outputs.size(); ++i) {
            const String portId = i < context.outputPorts.size()
                    ? context.outputPorts[i].portId
                    : "out";

            outputs.push_back({ step.nodeId, portId, context.outputs[i] });
            nodeOutputs.push_back({ portId, outputs.back().payload });
        }

        if (nodeOutputs.empty()) {
            SignalPayload silent;
            silent.block.samples.resize(frameCount);
            if (!context.outputPorts.empty()) {
                silent.domain = context.outputPorts.front().domain;
                silent.channelLayout = context.outputPorts.front().channelLayout;
            }

            outputs.push_back({ step.nodeId, "out", std::move(silent) });
            nodeOutputs.push_back({ "out", outputs.back().payload });
        }

        result.nodes.push_back({ step.nodeId, nodeOutputs.front().second, std::move(nodeOutputs) });

        if (isOutputNode(graph, step.nodeId)) {
            result.output = result.nodes.back().output;
        }
    }

    return result;
}

const SignalPayload* GraphAudioExecutor::findOutputForNode(
        const std::vector<PortOutput>& outputs,
        const String& nodeId,
        const String& portId) const {
    for (auto it = outputs.rbegin(); it != outputs.rend(); ++it) {
        if (it->nodeId == nodeId && it->portId == portId) {
            return &it->payload;
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
