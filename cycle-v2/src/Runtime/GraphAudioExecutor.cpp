#include "GraphAudioExecutor.h"

#include <algorithm>

namespace CycleV2 {

namespace {

size_t maxInputCount(const GraphExecutionPlan& plan) {
    size_t maxInputs = 0;

    for (const auto& step : plan.steps) {
        for (const auto& input : step.inputs) {
            if (input.destPortIndex >= 0) {
                maxInputs = std::max(maxInputs, (size_t) input.destPortIndex + 1);
            }
        }
    }

    return maxInputs;
}

size_t maxAttachmentCount(const GraphExecutionPlan& plan) {
    size_t maxAttachments = 0;

    for (const auto& step : plan.steps) {
        size_t attachmentCount = 0;

        for (const auto& attachment : plan.attachments) {
            if (attachment.destNodeId == step.nodeId) {
                ++attachmentCount;
            }
        }

        maxAttachments = std::max(maxAttachments, attachmentCount);
    }

    return maxAttachments;
}

size_t maxOutputCount(const GraphExecutionPlan& plan) {
    size_t maxOutputs = 1;

    for (const auto& step : plan.steps) {
        maxOutputs = std::max(maxOutputs, step.outputs.size());
    }

    return maxOutputs;
}

}

GraphAudioResult GraphAudioExecutor::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount) const {
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, voice.voiceIndex });
    return process(graph, plan, frameCount, {}, std::move(voice));
}

GraphAudioResult GraphAudioExecutor::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        AudioProcessTiming timing) const {
    return process(graph, plan, frameCount, timing, {});
}

GraphAudioResult GraphAudioExecutor::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        AudioProcessTiming timing,
        AudioVoiceContext voice) const {
    std::vector<PortOutput> outputs;
    outputs.reserve(plan.steps.size());

    workArena.prepare(
            frameCount,
            maxInputCount(plan),
            maxOutputCount(plan),
            frameCount * std::max(frameCount, (size_t) 8));

    const size_t attachmentCapacity = maxAttachmentCount(plan);
    NodeAudioProcessorFactory processorFactory;
    GraphAudioResult result;

    for (const auto& step : plan.steps) {
        NodeAudioProcessor* processor = processorFor(
                step.nodeId,
                voice.voiceIndex,
                step.audioRole,
                processorFactory);

        if (processor == nullptr) {
            continue;
        }

        AudioProcessContext context;
        context.frameCount = frameCount;
        context.timing = timing;
        context.voice = voice;
        context.workArena = &workArena;
        context.configuration = &step.configuration;
        context.parameters = step.parameters;
        context.inputs.reserve(workArena.inputCapacity);
        context.attachments.reserve(attachmentCapacity);
        context.outputPorts.reserve(step.outputs.size());
        context.outputs.reserve(workArena.outputCapacity);

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

        for (const auto& attachment : plan.attachments) {
            if (attachment.destNodeId != step.nodeId) {
                continue;
            }

            const SignalPayload* sourceOutput = findOutputForNode(
                    outputs,
                    attachment.sourceNodeId,
                    attachment.sourcePortId);
            if (sourceOutput == nullptr) {
                continue;
            }

            auto payload = *sourceOutput;
            payload.domain = attachment.domain;
            context.attachments.push_back({
                    attachment.sourceNodeId,
                    attachment.sourcePortId,
                    attachment.destPortId,
                    attachment.domain,
                    std::move(payload)
            });
        }

        if (context.configuration != nullptr) {
            processor->adoptConfiguration(*context.configuration);
        }
        processor->prepareExecution({ frameCount, timing.sampleRate, ChannelLayout::Mono });
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

    removeStaleProcessors(plan);

    return result;
}

NodeAudioProcessor* GraphAudioExecutor::processorFor(
        const String& nodeId,
        int voiceIndex,
        AudioModuleRole role,
        const NodeAudioProcessorFactory& factory) const {
    const auto found = std::find_if(processors.begin(), processors.end(), [&](const auto& entry) {
        return entry.nodeId == nodeId && entry.voiceIndex == voiceIndex;
    });

    if (found != processors.end()) {
        if (found->role != role) {
            found->role = role;
            found->processor = factory.create(role);
        }

        return found->processor.get();
    }

    auto processor = factory.create(role);
    NodeAudioProcessor* result = processor.get();
    processors.push_back({ nodeId, voiceIndex, role, std::move(processor) });
    return result;
}

void GraphAudioExecutor::removeStaleProcessors(const GraphExecutionPlan& plan) const {
    processors.erase(
            std::remove_if(processors.begin(), processors.end(), [&](const auto& entry) {
                return std::none_of(plan.steps.begin(), plan.steps.end(), [&](const auto& step) {
                    return step.nodeId == entry.nodeId;
                });
            }),
            processors.end());
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
