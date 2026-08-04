#include "GraphCompiler.h"

#include "../Nodes/Control/ModulationTriple.h"
#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../Nodes/Unison/UnisonNode.h"

#include <algorithm>

namespace CycleV2 {

namespace {

int indexOfNode(const std::vector<Node>& nodes, const String& nodeId) {
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[static_cast<size_t>(i)].id == nodeId) {
            return i;
        }
    }

    return -1;
}

const Node* findNode(const NodeGraph& graph, const String& nodeId) {
    const int index = indexOfNode(graph.getNodes(), nodeId);

    if (index < 0) {
        return nullptr;
    }

    return &graph.getNodes()[static_cast<size_t>(index)];
}

const Port* findPort(const Node& node, const String& portId, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

int inputPortIndex(const Node& node, const String& portId) {
    for (int i = 0; i < static_cast<int>(node.inputs.size()); ++i) {
        if (node.inputs[static_cast<size_t>(i)].id == portId) {
            return i;
        }
    }

    return -1;
}

PortDomain outputPortDomain(
        const std::vector<Edge>& resolvedEdges,
        const Node& node,
        const Port& port) {
    if (node.kind == NodeKind::Envelope && port.domain == PortDomain::ControlSignal) {
        return PortDomain::ControlSignal;
    }
    for (const auto& edge : resolvedEdges) {
        if (edge.sourceNodeId == node.id && edge.sourcePortId == port.id) {
            return edge.domain;
        }
    }

    return port.domain;
}

ChannelLayout outputPortChannelLayout(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const GraphDomainResolution& domainResolution,
        const Node& node,
        const Port& port) {
    for (const auto& edge : resolvedEdges) {
        if (edge.sourceNodeId == node.id && edge.sourcePortId == port.id) {
            return domainResolver.resolvedChannelLayoutForEdge(
                    graph,
                    edge,
                    domainResolution);
        }
    }

    return port.channelLayout;
}

std::vector<GraphStepOutput> buildStepOutputs(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const GraphDomainResolution& domainResolution,
        const Node& node) {
    std::vector<GraphStepOutput> outputs;
    outputs.reserve(node.outputs.size());

    for (const auto& port : node.outputs) {
        outputs.push_back({
                port.id,
                outputPortDomain(resolvedEdges, node, port),
                outputPortChannelLayout(
                        graph,
                        resolvedEdges,
                        domainResolver,
                        domainResolution,
                        node,
                        port),
                -1
        });
    }

    return outputs;
}

int parameterInt(const Node& node, const String& parameterId, int fallback) {
    const String value = parameterValueForNode(node, parameterId);

    if (value.isEmpty()) {
        return fallback;
    }

    return value.getIntValue();
}

String transformModeForNode(const Node& node) {
    if (node.kind == NodeKind::Fft) {
        return parameterValueForNode(node, "mode", "cycle");
    }

    if (node.kind == NodeKind::Ifft) {
        return parameterValueForNode(node, "mode", "cyclic");
    }

    return {};
}

int latencyCyclesForNode(const Node& node) {
    if (node.kind != NodeKind::Ifft) {
        return 0;
    }

    return parameterValueForNode(node, "mode", "cyclic") == "acyclicCarry" ? 1 : 0;
}

ExecutionCoordinate coordinateForTrait(NodeExecutionTrait trait) {
    switch (trait) {
        case NodeExecutionTrait::ConfigurationOnly:
            return ExecutionCoordinate::Configuration;
        case NodeExecutionTrait::CycleGenerator:
        case NodeExecutionTrait::CoordinateTransform:
            return ExecutionCoordinate::CycleField;
        case NodeExecutionTrait::SpectralTransform:
            return ExecutionCoordinate::SpectralFrame;
        case NodeExecutionTrait::OscillatorMaterializer:
        case NodeExecutionTrait::ControlProducer:
        case NodeExecutionTrait::SampleBlockProcessor:
            return ExecutionCoordinate::SampleBlock;
    }
    return ExecutionCoordinate::SampleBlock;
}

std::vector<String> buildNodeOrder(
        const NodeGraph& graph,
        std::vector<GraphCompileIssue>& issues) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();

    std::vector<int> indegrees(nodes.size(), 0);
    std::vector<bool> emitted(nodes.size(), false);
    std::vector<String> order;
    order.reserve(nodes.size());

    for (const auto& edge : edges) {
        const int sourceIndex = indexOfNode(nodes, edge.sourceNodeId);
        const int destIndex = indexOfNode(nodes, edge.destNodeId);

        if (sourceIndex >= 0 && destIndex >= 0 && sourceIndex != destIndex) {
            ++indegrees[static_cast<size_t>(destIndex)];
        }
    }

    while (order.size() < nodes.size()) {
        int readyIndex = -1;

        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            if (!emitted[static_cast<size_t>(i)] && indegrees[static_cast<size_t>(i)] == 0) {
                readyIndex = i;
                break;
            }
        }

        if (readyIndex < 0) {
            issues.push_back({
                    GraphCompileCode::CycleDetected,
                    "Graph contains a cycle in processing dependencies"
            });
            break;
        }

        emitted[static_cast<size_t>(readyIndex)] = true;
        order.push_back(nodes[static_cast<size_t>(readyIndex)].id);

        for (const auto& edge : edges) {
            if (edge.sourceNodeId != nodes[static_cast<size_t>(readyIndex)].id) {
                continue;
            }

            const int destIndex = indexOfNode(nodes, edge.destNodeId);
            if (destIndex >= 0 && destIndex != readyIndex) {
                --indegrees[static_cast<size_t>(destIndex)];
            }
        }
    }

    return order;
}

std::vector<GraphExecutionStep> buildExecutionSteps(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const GraphDomainResolution& domainResolution,
        const NodeModuleRegistry& moduleRegistry) {
    std::vector<GraphExecutionStep> steps;
    steps.reserve(nodeOrder.size());

    for (const auto& nodeId : nodeOrder) {
        const int nodeIndex = indexOfNode(graph.getNodes(), nodeId);

        if (nodeIndex < 0) {
            continue;
        }

        const Node& node = graph.getNodes()[static_cast<size_t>(nodeIndex)];
        const auto descriptor = moduleRegistry.descriptorFor(node.kind);
        if (!descriptor.executable) {
            continue;
        }
        if (node.kind == NodeKind::ModulationTriple
                && std::none_of(resolvedEdges.begin(), resolvedEdges.end(), [&](const Edge& edge) {
                    return edge.sourceNodeId == node.id;
                })
                && std::none_of(node.outputs.begin(), node.outputs.end(), [&](const Port& port) {
                    return graph.findSignalProbeForSource(node.id, port.id) != nullptr;
                })) {
            continue;
        }
        std::vector<GraphStepInput> inputs;

        for (const auto& edge : resolvedEdges) {
            if (edge.destNodeId != node.id) {
                continue;
            }

            inputs.push_back({
                    edge.sourceNodeId,
                    edge.sourcePortId,
                    edge.destPortId,
                    inputPortIndex(node, edge.destPortId),
                    -1,
                    -1,
                    -1,
                    edge.domain,
                    domainResolver.resolvedChannelLayoutForEdge(
                            graph,
                            edge,
                            domainResolution)
            });
        }

        std::sort(
                inputs.begin(),
                inputs.end(),
                [&](const GraphStepInput& a, const GraphStepInput& b) {
                    return a.destPortIndex < b.destPortIndex;
                });

        steps.push_back({
                node.id,
                node.kind,
                descriptor.executionTrait,
                coordinateForTrait(descriptor.executionTrait),
                RuntimeOwnershipScope::SynthVoice,
                -1,
                node.kind == NodeKind::Output,
                descriptor.audioRole,
                descriptor.previewRole,
                descriptor.previewContract,
                descriptor.previewable,
                descriptor.cycle1AdapterBacked,
                descriptor.cycle1Reference,
                parameterInt(node, "cycleFrames", 2048),
                latencyCyclesForNode(node),
                transformModeForNode(node),
                node.parameters,
                node.model,
                {},
                std::move(inputs),
                buildStepOutputs(
                        graph,
                        resolvedEdges,
                        domainResolver,
                        domainResolution,
                        node)
        });
    }

    return steps;
}

int bufferIndexFor(
        const std::vector<GraphBufferPlan>& buffers,
        const String& nodeId,
        const String& portId) {
    for (int i = 0; i < (int) buffers.size(); ++i) {
        if (buffers[(size_t) i].sourceNodeId == nodeId
                && buffers[(size_t) i].sourcePortId == portId) {
            return i;
        }
    }

    return -1;
}

int stepIndexFor(const GraphExecutionPlan& plan, const String& nodeId) {
    for (int i = 0; i < (int) plan.steps.size(); ++i) {
        if (plan.steps[(size_t) i].nodeId == nodeId) {
            return i;
        }
    }

    return -1;
}

int outputIndexFor(const GraphExecutionStep& step, const String& portId) {
    for (int i = 0; i < (int) step.outputs.size(); ++i) {
        if (step.outputs[(size_t) i].portId == portId) {
            return i;
        }
    }

    return -1;
}

void compileRouting(GraphExecutionPlan& plan) {
    for (int stepIndex = 0; stepIndex < (int) plan.steps.size(); ++stepIndex) {
        auto& step = plan.steps[(size_t) stepIndex];
        plan.maximumOutputCount = std::max(plan.maximumOutputCount, step.outputs.size());

        for (auto& input : step.inputs) {
            if (input.destPortIndex >= 0) {
                plan.maximumInputCount = std::max(
                        plan.maximumInputCount,
                        (size_t) input.destPortIndex + 1);
            }
            input.sourceBufferIndex = bufferIndexFor(
                    plan.buffers,
                    input.sourceNodeId,
                    input.sourcePortId);
            input.sourceStepIndex = stepIndexFor(plan, input.sourceNodeId);
            if (input.sourceStepIndex >= 0) {
                input.sourceOutputIndex = outputIndexFor(
                        plan.steps[(size_t) input.sourceStepIndex],
                        input.sourcePortId);
            }
            if (input.sourceBufferIndex >= 0) {
                plan.buffers[(size_t) input.sourceBufferIndex].lastConsumerStep = stepIndex;
            }
        }

        for (auto& output : step.outputs) {
            output.bufferIndex = bufferIndexFor(plan.buffers, step.nodeId, output.portId);
            if (output.bufferIndex >= 0) {
                auto& buffer = plan.buffers[(size_t) output.bufferIndex];
                buffer.firstProducerStep = stepIndex;
                buffer.lastConsumerStep = std::max(buffer.lastConsumerStep, stepIndex);
            }
        }

        for (const auto& attachment : plan.attachments) {
            if (attachment.destNodeId != step.nodeId) {
                continue;
            }

            const int sourceBufferIndex = bufferIndexFor(
                    plan.buffers,
                    attachment.sourceNodeId,
                    attachment.sourcePortId);
            step.attachments.push_back({
                    attachment.sourceNodeId,
                    attachment.sourcePortId,
                    attachment.destPortId,
                    attachment.domain,
                    sourceBufferIndex
            });
            if (sourceBufferIndex >= 0) {
                plan.buffers[(size_t) sourceBufferIndex].lastConsumerStep = stepIndex;
            }
        }
        plan.maximumAttachmentCount = std::max(
                plan.maximumAttachmentCount,
                step.attachments.size());
    }
}

int dependencyNodeIndex(const GraphDependencyIndex& index, const String& nodeId) {
    const auto found = index.nodeIndexById.find(nodeId);
    return found != index.nodeIndexById.end() ? found->second : -1;
}

void compileDependencyIndex(GraphExecutionPlan& plan) {
    auto& index = plan.dependencyIndex;
    index.nodeIds = plan.nodeOrder;
    index.dependents.assign(index.nodeIds.size(), {});
    index.dependencies.assign(index.nodeIds.size(), {});
    index.nodeIndexById.clear();
    index.nodeIndexById.reserve(index.nodeIds.size());
    for (size_t nodeIndex = 0; nodeIndex < index.nodeIds.size(); ++nodeIndex) {
        index.nodeIndexById.emplace(index.nodeIds[nodeIndex], static_cast<int>(nodeIndex));
    }
    index.stepIndexById.clear();
    index.stepIndexById.reserve(plan.steps.size());
    for (size_t stepIndex = 0; stepIndex < plan.steps.size(); ++stepIndex) {
        index.stepIndexById.emplace(plan.steps[stepIndex].nodeId, static_cast<int>(stepIndex));
    }

    const auto appendEdges = [&](const std::vector<Edge>& edges) {
        for (const auto& edge : edges) {
            const int source = dependencyNodeIndex(index, edge.sourceNodeId);
            const int destination = dependencyNodeIndex(index, edge.destNodeId);
            if (source < 0 || destination < 0) {
                continue;
            }

            auto& destinations = index.dependents[(size_t) source];
            if (std::find(destinations.begin(), destinations.end(), destination)
                    == destinations.end()) {
                destinations.push_back(destination);
                index.dependencies[static_cast<size_t>(destination)].push_back(source);
            }
        }
    };

    appendEdges(plan.signalEdges);
    appendEdges(plan.attachments);
    appendEdges(plan.configurationAttachments);
}

std::vector<GraphBufferPlan> buildBufferPlan(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const GraphDomainResolution& domainResolution) {
    std::vector<GraphBufferPlan> buffers;

    for (const auto& node : graph.getNodes()) {
        for (const auto& port : node.outputs) {
            if (port.connectionKind == ConnectionKind::ConfigurationAttachment) {
                continue;
            }
            if (node.kind == NodeKind::ModulationTriple
                    && std::none_of(resolvedEdges.begin(), resolvedEdges.end(), [&](const Edge& edge) {
                        return edge.sourceNodeId == node.id && edge.sourcePortId == port.id;
                    })
                    && graph.findSignalProbeForSource(node.id, port.id) == nullptr) {
                continue;
            }
            const PortDomain domain = outputPortDomain(resolvedEdges, node, port);
            if (domain == PortDomain::DomainContext) {
                continue;
            }

            buffers.push_back({
                    node.id + "." + port.id,
                    node.id,
                    port.id,
                    domain,
                    outputPortChannelLayout(
                            graph,
                            resolvedEdges,
                            domainResolver,
                            domainResolution,
                            node,
                            port)
            });
        }
    }

    return buffers;
}

std::vector<CompiledVoiceContext> compileVoiceContexts(
        const NodeGraph& graph,
        const std::vector<Edge>& configurationAttachments,
        const std::vector<Edge>& signalEdges) {
    std::vector<CompiledVoiceContext> contexts;
    const auto defaultModulation = buildModulationTripleConfiguration({});
    const auto defaultUnison = buildUnisonNodeConfiguration({});
    std::vector<std::pair<String, std::shared_ptr<const ModulationTripleConfiguration>>>
            modulationConfigurations;
    std::vector<std::pair<String, std::shared_ptr<const UnisonNodeConfiguration>>>
            unisonConfigurations;
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::VoiceContext) {
            continue;
        }

        CompiledVoiceContext context;
        context.nodeId = node.id;
        context.startDomain = parameterValueForNode(node, "domain", "waveform");
        context.octave = typedParameterInt(node.parameters, "octave", 0);
        context.pitchSemitones = typedParameterFloat(node.parameters, "pitch", 0.f);
        context.portamento = typedParameterBool(node.parameters, "portamento", false);
        context.oversampling = jmax(
                1,
                parameterValueForNode(node, "oversampling", "1x").getIntValue());
        context.defaultModulation = defaultModulation;
        auto unison = defaultUnison;
        context.unison = unison;
        context.lanes = unison->layout;

        for (const auto& attachment : configurationAttachments) {
            if (attachment.destNodeId != node.id) {
                continue;
            }
            const Node* source = findNode(graph, attachment.sourceNodeId);
            if (source == nullptr) {
                continue;
            }
            if (attachment.attachmentType == AttachmentType::ModulationTriple) {
                auto found = std::find_if(
                        modulationConfigurations.begin(),
                        modulationConfigurations.end(),
                        [&](const auto& entry) { return entry.first == source->id; });
                if (found == modulationConfigurations.end()) {
                    modulationConfigurations.push_back({
                            source->id,
                            buildModulationTripleConfiguration(source->parameters)
                    });
                    found = modulationConfigurations.end() - 1;
                }
                context.defaultModulation = found->second;
            } else if (attachment.attachmentType == AttachmentType::Unison) {
                auto found = std::find_if(
                        unisonConfigurations.begin(),
                        unisonConfigurations.end(),
                        [&](const auto& entry) { return entry.first == source->id; });
                if (found == unisonConfigurations.end()) {
                    unisonConfigurations.push_back({
                            source->id,
                            buildUnisonNodeConfiguration(source->parameters, source->model)
                    });
                    found = unisonConfigurations.end() - 1;
                }
                unison = found->second;
                context.unison = unison;
                context.lanes = unison->layout;
            }
        }

        for (const auto& edge : signalEdges) {
            if (edge.destNodeId != node.id || edge.destPortId != "pitch") {
                continue;
            }
            const Node* source = findNode(graph, edge.sourceNodeId);
            if (source != nullptr && source->kind == NodeKind::Envelope) {
                context.pitchEnvelope = EnvelopeSignalProcessor::buildConfiguration(
                        source->parameters,
                        source->model);
                const auto envelope = std::dynamic_pointer_cast<const EnvelopeConfiguration>(
                        context.pitchEnvelope);
                if (envelope != nullptr) {
                    constexpr int previewSamples = 129;
                    Rasterization::EnvelopePlaybackEngine playback;
                    playback.ensureVoiceCount(1);
                    playback.validate(envelope->rasterizer->preparedPlaybackView());
                    playback.noteOn();
                    MeshLibrary::EnvProps props;
                    props.active = true;
                    playback.renderToBuffer(
                            envelope->rasterizer->preparedPlaybackView(),
                            previewSamples,
                            1.0 / (double) (previewSamples - 1),
                            Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex,
                            props,
                            1.f);
                    const Buffer<float> values = playback.output().withSize(previewSamples);
                    context.pitchEnvelopeUnitValues.assign(
                            values.get(),
                            values.get() + previewSamples);
                }
            }
        }
        contexts.push_back(std::move(context));
    }
    return contexts;
}

using VoiceContextAssignments = std::vector<std::vector<String>>;

bool mergeVoiceContexts(std::vector<String>& destination, const std::vector<String>& source) {
    bool changed = false;
    for (const auto& contextId : source) {
        if (std::find(destination.begin(), destination.end(), contextId) == destination.end()) {
            destination.push_back(contextId);
            changed = true;
        }
    }
    return changed;
}

VoiceContextAssignments assignVoiceContexts(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan) {
    VoiceContextAssignments assignments(graph.getNodes().size());
    for (size_t nodeIndex = 0; nodeIndex < graph.getNodes().size(); ++nodeIndex) {
        const Node& node = graph.getNodes()[nodeIndex];
        if (node.kind == NodeKind::VoiceContext) {
            assignments[nodeIndex].push_back(node.id);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& edge : plan.signalEdges) {
            const int sourceIndex = indexOfNode(graph.getNodes(), edge.sourceNodeId);
            const int destinationIndex = indexOfNode(graph.getNodes(), edge.destNodeId);
            if (sourceIndex < 0 || destinationIndex < 0) {
                continue;
            }
            changed = mergeVoiceContexts(
                    assignments[(size_t) destinationIndex],
                    assignments[(size_t) sourceIndex]) || changed;

            const Node& source = graph.getNodes()[(size_t) sourceIndex];
            if (source.kind == NodeKind::Envelope) {
                changed = mergeVoiceContexts(
                        assignments[(size_t) sourceIndex],
                        assignments[(size_t) destinationIndex]) || changed;
            }
        }

        for (const auto& attachment : plan.attachments) {
            if (attachment.attachmentType != AttachmentType::ScratchEnvelope) {
                continue;
            }
            const int sourceIndex = indexOfNode(
                    graph.getNodes(),
                    attachment.sourceNodeId);
            const int destinationIndex = indexOfNode(
                    graph.getNodes(),
                    attachment.destNodeId);
            if (sourceIndex >= 0 && destinationIndex >= 0) {
                changed = mergeVoiceContexts(
                        assignments[(size_t) sourceIndex],
                        assignments[(size_t) destinationIndex]) || changed;
            }
        }
    }

    return assignments;
}

const CompiledVoiceContext* voiceContextForNode(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        const VoiceContextAssignments& assignments,
        const Node& node) {
    const int nodeIndex = indexOfNode(graph.getNodes(), node.id);
    if (nodeIndex < 0 || assignments[(size_t) nodeIndex].size() != 1) {
        return nullptr;
    }
    const String& contextId = assignments[(size_t) nodeIndex].front();
    const auto found = std::find_if(
            plan.voiceContexts.begin(),
            plan.voiceContexts.end(),
            [&](const CompiledVoiceContext& context) {
                return context.nodeId == contextId;
            });
    if (found != plan.voiceContexts.end()) {
        return &*found;
    }
    return nullptr;
}

void compileDefaultModulationInputs(
        const NodeGraph& graph,
        GraphExecutionPlan& plan) {
    const VoiceContextAssignments assignments = assignVoiceContexts(graph, plan);
    for (auto& step : plan.steps) {
        const Node* node = findNode(graph, step.nodeId);
        if (node == nullptr) {
            continue;
        }
        const CompiledVoiceContext* context = voiceContextForNode(
                graph,
                plan,
                assignments,
                *node);
        if (context == nullptr) {
            continue;
        }
        for (int portIndex = 0; portIndex < (int) node->inputs.size(); ++portIndex) {
            const Port& port = node->inputs[(size_t) portIndex];
            if (port.defaultModulationSlot == DefaultModulationSlot::None) {
                continue;
            }
            const bool explicitInput = std::any_of(
                    plan.signalEdges.begin(),
                    plan.signalEdges.end(),
                    [&](const Edge& edge) {
                        return edge.destNodeId == node->id && edge.destPortId == port.id;
                    });
            if (explicitInput) {
                continue;
            }
            const String sourcePort = "default."
                    + String((int) port.defaultModulationSlot);
            const bool bufferExists = std::any_of(
                    plan.buffers.begin(),
                    plan.buffers.end(),
                    [&](const GraphBufferPlan& buffer) {
                        return buffer.sourceNodeId == context->nodeId
                                && buffer.sourcePortId == sourcePort;
                    });
            if (!bufferExists) {
                plan.buffers.push_back({
                        context->nodeId + "." + sourcePort,
                        context->nodeId,
                        sourcePort,
                        PortDomain::ControlSignal,
                        ChannelLayout::Mono,
                        -1,
                        -1,
                        port.defaultModulationSlot,
                        context->defaultModulation
                });
            }
            step.inputs.push_back({
                    context->nodeId,
                    sourcePort,
                    port.id,
                    portIndex,
                    -1,
                    -1,
                    -1,
                    PortDomain::ControlSignal,
                    ChannelLayout::Mono
            });
        }
    }
}

bool regionOperation(NodeExecutionTrait trait) {
    return trait == NodeExecutionTrait::CycleGenerator
            || trait == NodeExecutionTrait::CoordinateTransform
            || trait == NodeExecutionTrait::SpectralTransform
            || trait == NodeExecutionTrait::OscillatorMaterializer;
}

bool containsStep(const std::vector<int>& steps, int stepIndex) {
    return std::find(steps.begin(), steps.end(), stepIndex) != steps.end();
}

String directVoiceContext(const GraphExecutionStep& step) {
    const auto found = std::find_if(
            step.inputs.begin(),
            step.inputs.end(),
            [](const GraphStepInput& input) {
                return input.domain == PortDomain::DomainContext;
            });
    return found != step.inputs.end() ? found->sourceNodeId : String {};
}

bool hasSpectralOutput(const GraphExecutionStep& step) {
    return std::any_of(step.outputs.begin(), step.outputs.end(), [](const auto& output) {
        return output.domain == PortDomain::SpectralMagnitudeSignal
                || output.domain == PortDomain::SpectralPhaseSignal;
    });
}

int effectiveLaneCount(
        const std::vector<CompiledVoiceContext>& contexts,
        const String& contextNodeId) {
    const auto found = std::find_if(
            contexts.begin(),
            contexts.end(),
            [&](const CompiledVoiceContext& context) {
                return context.nodeId == contextNodeId;
            });
    if (found == contexts.end()) {
        return 1;
    }
    const auto unison = std::dynamic_pointer_cast<const UnisonNodeConfiguration>(found->unison);
    return unison != nullptr && !unison->isEnabled() ? 1 : found->lanes.order;
}

bool appendDownstreamRegionOperations(
        const GraphExecutionPlan& plan,
        const std::vector<bool>& assigned,
        OscillatorRegionPlan& region,
        std::vector<GraphCompileIssue>& issues) {
    for (size_t cursor = 0; cursor < region.stepIndices.size(); ++cursor) {
        const int sourceStepIndex = region.stepIndices[cursor];
        const auto& source = plan.steps[(size_t) sourceStepIndex];
        if (source.executionTrait == NodeExecutionTrait::OscillatorMaterializer) {
            continue;
        }
        for (const auto& edge : plan.signalEdges) {
            if (edge.sourceNodeId != source.nodeId) {
                continue;
            }
            const int destinationIndex = stepIndexFor(plan, edge.destNodeId);
            if (destinationIndex < 0
                    || !regionOperation(plan.steps[(size_t) destinationIndex].executionTrait)
                    || containsStep(region.stepIndices, destinationIndex)) {
                continue;
            }
            const auto& destination = plan.steps[(size_t) destinationIndex];
            const bool hasMaterializedInput = std::any_of(
                    destination.inputs.begin(),
                    destination.inputs.end(),
                    [&](const GraphStepInput& input) {
                        const int inputStepIndex = stepIndexFor(
                                plan,
                                input.sourceNodeId);
                        return inputStepIndex >= 0
                                && !containsStep(
                                        region.stepIndices,
                                        inputStepIndex)
                                && plan.steps[(size_t) inputStepIndex].executionTrait
                                        == NodeExecutionTrait::OscillatorMaterializer;
                    });
            if (hasMaterializedInput) {
                continue;
            }
            if (assigned[(size_t) destinationIndex]) {
                issues.push_back({
                        GraphCompileCode::AmbiguousVoiceContext,
                        "Oscillator operation '" + edge.destNodeId
                                + "' is reached by multiple Voice Contexts"
                });
                return false;
            }
            region.stepIndices.push_back(destinationIndex);
        }
    }
    return true;
}

void appendUpstreamRegionOperations(
        const GraphExecutionPlan& plan,
        const String& contextNodeId,
        OscillatorRegionPlan& region) {
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const auto& edge : plan.signalEdges) {
            const int destinationIndex = stepIndexFor(plan, edge.destNodeId);
            const int sourceIndex = stepIndexFor(plan, edge.sourceNodeId);
            if (destinationIndex < 0
                    || sourceIndex < 0
                    || !containsStep(region.stepIndices, destinationIndex)
                    || containsStep(region.stepIndices, sourceIndex)
                    || !regionOperation(plan.steps[(size_t) sourceIndex].executionTrait)
                    || plan.steps[(size_t) sourceIndex].executionTrait
                            == NodeExecutionTrait::OscillatorMaterializer) {
                continue;
            }
            const String sourceContext = directVoiceContext(plan.steps[(size_t) sourceIndex]);
            if (sourceContext.isNotEmpty() && sourceContext != contextNodeId) {
                continue;
            }
            region.stepIndices.push_back(sourceIndex);
            expanded = true;
        }
    }
}

bool hasSpectralExecution(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    return std::any_of(
            region.stepIndices.begin(),
            region.stepIndices.end(),
            [&](int stepIndex) {
                const auto& step = plan.steps[(size_t) stepIndex];
                return step.executionTrait == NodeExecutionTrait::SpectralTransform
                        || step.executionTrait == NodeExecutionTrait::OscillatorMaterializer
                        || hasSpectralOutput(step);
            });
}

int terminalRegionStep(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        int fallbackStep) {
    int terminalStep = -1;
    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        const bool hasRegionConsumer = std::any_of(
                plan.signalEdges.begin(),
                plan.signalEdges.end(),
                [&](const Edge& edge) {
                    return edge.sourceNodeId == step.nodeId
                            && containsStep(region.stepIndices, stepIndexFor(plan, edge.destNodeId));
                });
        if (hasRegionConsumer) {
            continue;
        }
        if (terminalStep >= 0) {
            return fallbackStep;
        }
        terminalStep = stepIndex;
    }
    return terminalStep >= 0 ? terminalStep : fallbackStep;
}

void assignOscillatorRegion(
        GraphExecutionPlan& plan,
        int rootIndex,
        std::vector<bool>& assigned,
        OscillatorRegionPlan& region) {
    std::sort(region.stepIndices.begin(), region.stepIndices.end());
    const bool spectral = hasSpectralExecution(plan, region);
    region.strategy = spectral
            ? OscillatorExecutionStrategy::SharedSpectralFrame
            : OscillatorExecutionStrategy::ChainedPerLane;
    region.outputLatencySamples = 0;
    region.tailPolicy = OscillatorTailPolicy::EnvelopeOwned;
    region.outputTailSamples = 0;
    region.materializationStepIndex = terminalRegionStep(plan, region, rootIndex);
    const int regionIndex = (int) plan.oscillatorRegions.size();

    for (const int stepIndex : region.stepIndices) {
        auto& step = plan.steps[(size_t) stepIndex];
        assigned[(size_t) stepIndex] = true;
        step.oscillatorRegionIndex = regionIndex;
        if (step.executionTrait == NodeExecutionTrait::OscillatorMaterializer) {
            step.executionCoordinate = ExecutionCoordinate::SampleBlock;
            step.ownershipScope = RuntimeOwnershipScope::UnisonLane;
            region.materializationStepIndex = stepIndex;
        } else if (hasSpectralOutput(step)
                || step.executionTrait == NodeExecutionTrait::SpectralTransform) {
            step.executionCoordinate = ExecutionCoordinate::SpectralFrame;
            step.ownershipScope = RuntimeOwnershipScope::OscillatorRegion;
        } else {
            step.executionCoordinate = ExecutionCoordinate::CycleField;
            step.ownershipScope = spectral
                    ? RuntimeOwnershipScope::OscillatorRegion
                    : RuntimeOwnershipScope::UnisonLane;
        }
    }
}

void compileOscillatorRegions(
        GraphExecutionPlan& plan,
        std::vector<GraphCompileIssue>& issues) {
    std::vector<bool> assigned(plan.steps.size());
    for (int rootIndex = 0; rootIndex < (int) plan.steps.size(); ++rootIndex) {
        auto& root = plan.steps[(size_t) rootIndex];
        const String contextNodeId = directVoiceContext(root);
        if (assigned[(size_t) rootIndex]
                || root.executionTrait != NodeExecutionTrait::CycleGenerator
                || contextNodeId.isEmpty()) {
            continue;
        }

        OscillatorRegionPlan region;
        region.id = contextNodeId + ".oscillator." + String(plan.oscillatorRegions.size() + 1);
        region.voiceContextNodeId = contextNodeId;
        region.laneCount = effectiveLaneCount(plan.voiceContexts, contextNodeId);
        region.stepIndices.push_back(rootIndex);

        if (!appendDownstreamRegionOperations(plan, assigned, region, issues)) {
            return;
        }
        appendUpstreamRegionOperations(plan, contextNodeId, region);
        assignOscillatorRegion(plan, rootIndex, assigned, region);
        if (region.strategy == OscillatorExecutionStrategy::SharedSpectralFrame) {
            const auto& materializer = plan.steps[
                    (size_t) region.materializationStepIndex];
            if (materializer.executionTrait == NodeExecutionTrait::OscillatorMaterializer
                    && materializer.transformMode != "cyclic") {
                issues.push_back({
                        GraphCompileCode::UnsupportedReconstructionPolicy,
                        "Oscillator region '" + region.id
                                + "' requests unsupported reconstruction mode '"
                                + materializer.transformMode + "'"
                });
                return;
            }
        }
        plan.oscillatorRegions.push_back(std::move(region));
    }

    for (auto& step : plan.steps) {
        if (step.oscillatorRegionIndex < 0
                && step.executionTrait == NodeExecutionTrait::CoordinateTransform) {
            step.executionCoordinate = ExecutionCoordinate::SampleBlock;
        }
    }
}

}

bool GraphCompileResult::succeeded() const {
    return validationIssues.empty() && compileIssues.empty();
}

GraphCompileResult GraphCompiler::compile(const NodeGraph& graph) const {
    GraphCompileResult result;
    result.validationIssues = validator.validate(graph);

    if (!result.validationIssues.empty()) {
        return result;
    }

    result.plan.nodeOrder = buildNodeOrder(graph, result.compileIssues);

    if (result.compileIssues.empty()) {
        const GraphDomainResolution domainResolution = domainResolver.resolve(graph);
        result.plan.signalEdges = domainResolver.resolveSignalEdges(
                graph,
                result.plan.nodeOrder,
                domainResolution);
        result.plan.buffers = buildBufferPlan(
                graph,
                result.plan.signalEdges,
                domainResolver,
                domainResolution);

        for (const auto& edge : graph.getEdges()) {
            if (edge.isProcessingAttachment()) {
                result.plan.attachments.push_back(edge);
            } else if (edge.isConfigurationAttachment()) {
                result.plan.configurationAttachments.push_back(edge);
            }
        }

        result.plan.voiceContexts = compileVoiceContexts(
                graph,
                result.plan.configurationAttachments,
                result.plan.signalEdges);

        result.plan.steps = buildExecutionSteps(
                graph,
                result.plan.nodeOrder,
                result.plan.signalEdges,
                domainResolver,
                domainResolution,
                moduleRegistry);
        compileDefaultModulationInputs(graph, result.plan);
        compileOscillatorRegions(result.plan, result.compileIssues);
        if (!result.compileIssues.empty()) {
            result.plan = {};
            return result;
        }
        compileRouting(result.plan);
        compileDependencyIndex(result.plan);
        refreshSignalProbes(graph, result.plan);
        publishConfigurations(graph, result.plan.steps);
    }

    if (!result.succeeded()) {
        result.plan = {};
    }

    return result;
}

void GraphCompiler::refreshSignalProbes(
        const NodeGraph& graph,
        GraphExecutionPlan& plan) const {
    plan.signalProbes.clear();
    plan.signalProbes.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        CompiledSignalProbe compiled;
        compiled.probeId = probe.id;
        const auto source = plan.dependencyIndex.stepIndexById.find(probe.sourceNodeId);
        if (source != plan.dependencyIndex.stepIndexById.end()) {
            compiled.sourceStepIndex = source->second;
            const auto& outputs = plan.steps[static_cast<size_t>(source->second)].outputs;
            const auto output = std::find_if(
                    outputs.begin(),
                    outputs.end(),
                    [&](const auto& candidate) {
                        return candidate.portId == probe.sourcePortId;
                    });
            if (output != outputs.end()) {
                compiled.sourceOutputIndex = static_cast<int>(
                        std::distance(outputs.begin(), output));
            }
        }
        plan.signalProbes.push_back(std::move(compiled));
    }
}

void GraphCompiler::publishConfigurations(
        const NodeGraph& graph,
        std::vector<GraphExecutionStep>& steps) const {
    const AudioExecutionSpec spec;

    for (auto& step : steps) {
        const String key = configurationFactory.keyFor(
                step.audioRole,
                step.parameters,
                step.model,
                spec,
                &graph,
                step.nodeId);
        auto found = std::find_if(configurations.begin(), configurations.end(), [&](const auto& entry) {
            return entry.nodeId == step.nodeId;
        });

        if (found == configurations.end()) {
            configurations.push_back({ step.nodeId, {} });
            found = configurations.end() - 1;
        }

        step.configuration = found->publisher.publish(key, [&]() {
            return configurationFactory.create(
                    step.audioRole,
                    step.parameters,
                    step.model,
                    spec,
                    &graph,
                    step.nodeId);
        });
    }
}

}
