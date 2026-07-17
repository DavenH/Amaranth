#include "GraphAudioExecutor.h"
#include "AudioProcessContextUtils.h"

#include <algorithm>
#include <iterator>

namespace CycleV2 {

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
    return processInternal(
            graph,
            plan,
            frameCount,
            timing,
            std::move(voice),
            true,
            nullptr);
}

GraphAudioOutputView GraphAudioExecutor::processRealtime(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        AudioProcessTiming timing,
        const AudioVoiceContext& voice,
        GraphProcessObserver* observer) const {
    processInternal(
            graph,
            plan,
            frameCount,
            timing,
            voice,
            false,
            observer);
    return { realtimeOutput };
}

GraphAudioResult GraphAudioExecutor::processInternal(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        AudioProcessTiming timing,
        const AudioVoiceContext& voice,
        bool captureDiagnostics,
        GraphProcessObserver* observer) const {
    if (captureDiagnostics) {
        AudioExecutionSpec executionSpec;
        executionSpec.maximumFrameCount = frameCount;
        executionSpec.sampleRate = timing.sampleRate;
        executionSpec.bpm = timing.bpm;
        executionSpec.beatsPerMeasure = timing.beatsPerMeasure;
        prepareExecution(plan, executionSpec, voice.voiceIndex);
    }

    const auto preparedVoice = std::find_if(
            preparedVoices.begin(), preparedVoices.end(), [&](const auto& candidate) {
                return candidate.voiceIndex == voice.voiceIndex;
            });
    if (frameCount > workArena.frameCapacity
            || bufferSlots.size() != plan.buffers.size()
            || preparedVoice == preparedVoices.end()
            || preparedVoice->plan != &plan
            || preparedVoice->processors.size() != plan.steps.size()) {
        jassertfalse;
        return {};
    }

    const size_t attachmentCapacity = plan.maximumAttachmentCount;
    GraphAudioResult result;
    realtimeOutput = nullptr;

    for (size_t stepIndex = 0; stepIndex < plan.steps.size(); ++stepIndex) {
        const auto& step = plan.steps[stepIndex];
        NodeAudioProcessor* processor = stepIndex < preparedVoice->processors.size()
                ? preparedVoice->processors[stepIndex]
                : nullptr;

        if (processor == nullptr) {
            continue;
        }

        AudioProcessContext& context = processContext;
        context.frameCount = frameCount;
        context.timing = timing;
        context.voiceView = &voice;
        context.workArena = &workArena;
        context.configuration = &step.configuration;
        context.captureTraversalGrid = captureDiagnostics;
        context.parameterView = nullptr;
        context.parameters.clear();
        context.inputViews.assign(workArena.inputCapacity, nullptr);
        context.inputs.clear();
        context.attachments.clear();
        context.attachments.reserve(attachmentCapacity);
        context.outputPorts.clear();
        context.outputPorts.reserve(step.outputs.size());
        context.outputViews.assign(step.outputs.size(), nullptr);
        context.outputs.clear();
        context.outputs.reserve(workArena.outputCapacity);

        for (size_t outputIndex = 0; outputIndex < step.outputs.size(); ++outputIndex) {
            const auto& output = step.outputs[outputIndex];
            context.outputPorts.push_back({
                    output.portId,
                    output.domain,
                    output.channelLayout
            });
            if (output.bufferIndex >= 0) {
                context.outputViews[outputIndex] = &bufferSlots[(size_t) output.bufferIndex];
            }
        }

        for (const auto& input : step.inputs) {
            if (input.destPortIndex < 0) {
                continue;
            }

            const auto inputIndex = (size_t) input.destPortIndex;
            if (input.sourceBufferIndex >= 0
                    && (size_t) input.sourceBufferIndex < bufferSlots.size()) {
                context.inputViews[inputIndex] = &bufferSlots[(size_t) input.sourceBufferIndex];
            }
        }

        for (const auto& attachment : step.attachments) {
            if (attachment.sourceBufferIndex < 0
                    || (size_t) attachment.sourceBufferIndex >= bufferSlots.size()) {
                continue;
            }

            context.attachments.push_back({
                    attachment.sourceNodeId,
                    attachment.sourcePortId,
                    attachment.destPortId,
                    attachment.domain,
                    &bufferSlots[(size_t) attachment.sourceBufferIndex]
            });
        }

        const bool outputNode = step.outputSink;
        const bool hasBufferOutput = std::any_of(
                step.outputs.begin(), step.outputs.end(), [](const auto& output) {
                    return output.bufferIndex >= 0;
                });
        if (!captureDiagnostics && observer == nullptr
                && !hasBufferOutput && !outputNode) {
            continue;
        }
        if (!captureDiagnostics && outputNode) {
            realtimeOutput = inputAt(context, 0);
            if (observer != nullptr) {
                observer->nodeProcessed(step.nodeId, context);
            }
            continue;
        }

        processor->process(context);

        if (observer != nullptr) {
            observer->nodeProcessed(step.nodeId, context);
        }

        SignalPayload* outputInput = outputNode ? inputAt(context, 0) : nullptr;

        std::vector<std::pair<String, SignalPayload>> nodeOutputs;
        for (size_t i = 0; i < context.outputs.size(); ++i) {
            const String portId = i < context.outputPorts.size()
                    ? context.outputPorts[i].portId
                    : "out";

            if (i < step.outputs.size() && step.outputs[i].bufferIndex >= 0) {
                bufferSlots[(size_t) step.outputs[i].bufferIndex] = std::move(context.outputs[i]);
                if (captureDiagnostics) {
                    nodeOutputs.push_back({ portId, bufferSlots[(size_t) step.outputs[i].bufferIndex] });
                }
            } else if (captureDiagnostics) {
                nodeOutputs.push_back({ portId, std::move(context.outputs[i]) });
            }
        }

        if (!captureDiagnostics) {
            if (outputNode) {
                realtimeOutput = outputInput;
            }
            continue;
        }

        if (captureDiagnostics && nodeOutputs.empty()) {
            SignalPayload silent;
            silent.block.samples.resize(frameCount);
            if (!context.outputPorts.empty()) {
                silent.domain = context.outputPorts.front().domain;
                silent.channelLayout = context.outputPorts.front().channelLayout;
            }

            nodeOutputs.push_back({ "out", std::move(silent) });
        }

        result.nodes.push_back({ step.nodeId, nodeOutputs.front().second, std::move(nodeOutputs) });

        if (outputNode) {
            result.output = result.nodes.back().output;
        }
    }

    removeUnreferencedProcessors();

    return result;
}

void GraphAudioExecutor::prepareExecution(
        const GraphExecutionPlan& plan,
        const AudioExecutionSpec& spec,
        int voiceIndex) const {
    NodeAudioProcessorFactory factory;
    workArena.prepare(
            spec.maximumFrameCount,
            plan.maximumInputCount,
            plan.maximumOutputCount,
            spec.maximumFrameCount * std::max(spec.maximumFrameCount, plan.maximumTraversalColumns));
    bufferSlots.resize(plan.buffers.size());
    for (auto& slot : bufferSlots) {
        slot.block.samples.reserve(spec.maximumFrameCount);
        slot.traversalGrid.values.reserve(workArena.gridValueCapacity);
    }
    processContext.inputViews.reserve(plan.maximumInputCount);
    processContext.attachments.reserve(plan.maximumAttachmentCount);
    processContext.outputPorts.reserve(plan.maximumOutputCount);
    processContext.outputViews.reserve(plan.maximumOutputCount);
    processContext.outputs.reserve(plan.maximumOutputCount);
    auto preparedVoice = std::find_if(
            preparedVoices.begin(), preparedVoices.end(), [&](const auto& candidate) {
                return candidate.voiceIndex == voiceIndex;
            });
    if (preparedVoice == preparedVoices.end()) {
        preparedVoices.push_back({ voiceIndex, &plan, {} });
        preparedVoice = std::prev(preparedVoices.end());
    }
    preparedVoice->plan = &plan;
    preparedVoice->processors.clear();
    preparedVoice->processors.reserve(plan.steps.size());

    for (const auto& step : plan.steps) {
        NodeAudioProcessor* processor = processorFor(step.nodeId, voiceIndex, step.audioRole, factory);
        preparedVoice->processors.push_back(processor);
        if (processor == nullptr) {
            continue;
        }

        auto found = std::find_if(processors.begin(), processors.end(), [&](const auto& entry) {
            return entry.nodeId == step.nodeId && entry.voiceIndex == voiceIndex;
        });
        jassert(found != processors.end());

        AudioExecutionSpec stepSpec = spec;
        if (!step.outputs.empty()) {
            stepSpec.domain = step.outputs.front().domain;
            stepSpec.channelLayout = step.outputs.front().channelLayout;
        }
        const uint64_t revision = step.configuration.revision;
        if (found->preparedRevision == revision
                && found->preparedKey == step.configuration.key
                && found->preparedFrameCount == spec.maximumFrameCount
                && found->preparedSampleRate == spec.sampleRate
                && found->preparedDomain == stepSpec.domain
                && found->preparedChannelLayout == stepSpec.channelLayout
                && found->preparedBpm == stepSpec.bpm
                && found->preparedBeatsPerMeasure == stepSpec.beatsPerMeasure) {
            continue;
        }

        processor->adoptConfiguration(step.configuration);
        processor->prepareExecution(stepSpec);
        found->preparedRevision = revision;
        found->preparedKey = step.configuration.key;
        found->preparedFrameCount = spec.maximumFrameCount;
        found->preparedSampleRate = spec.sampleRate;
        found->preparedDomain = stepSpec.domain;
        found->preparedChannelLayout = stepSpec.channelLayout;
        found->preparedBpm = stepSpec.bpm;
        found->preparedBeatsPerMeasure = stepSpec.beatsPerMeasure;
        ++found->preparationCount;
    }
}

size_t GraphAudioExecutor::preparationCount(const String& nodeId, int voiceIndex) const {
    const auto found = std::find_if(processors.begin(), processors.end(), [&](const auto& entry) {
        return entry.nodeId == nodeId && entry.voiceIndex == voiceIndex;
    });
    return found == processors.end() ? 0 : found->preparationCount;
}

size_t GraphAudioExecutor::serviceNonRealtimePreparation() const {
    size_t preparedCount = 0;
    for (const auto& entry : processors) {
        if (entry.processor != nullptr
                && entry.processor->serviceNonRealtimePreparation()) {
            ++preparedCount;
        }
    }
    return preparedCount;
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
            found->preparedRevision = 0;
            found->preparedKey = {};
            found->preparedFrameCount = 0;
            found->preparedSampleRate = 0.0;
            found->preparedDomain = PortDomain::ControlSignal;
            found->preparedChannelLayout = ChannelLayout::Mono;
            found->preparedBpm = 0.0;
            found->preparedBeatsPerMeasure = 0;
        }

        return found->processor.get();
    }

    auto processor = factory.create(role);
    NodeAudioProcessor* result = processor.get();
    processors.push_back({
            nodeId,
            voiceIndex,
            role,
            std::move(processor),
            0,
            {},
            0,
            0.0,
            PortDomain::ControlSignal,
            ChannelLayout::Mono,
            0.0,
            0,
            0
    });
    return result;
}

void GraphAudioExecutor::removeUnreferencedProcessors() const {
    processors.erase(
            std::remove_if(processors.begin(), processors.end(), [&](const auto& entry) {
                return std::none_of(preparedVoices.begin(), preparedVoices.end(), [&](const auto& voice) {
                    return std::find(
                            voice.processors.begin(),
                            voice.processors.end(),
                            entry.processor.get()) != voice.processors.end();
                });
            }),
            processors.end());
}

}
