#include "GraphCompiler.h"

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

int parameterInt(const Node& node, const String& parameterId, int fallback) {
    const String value = parameterValueForNode(node, parameterId);

    if (value.isEmpty()) {
        return fallback;
    }

    return value.getIntValue();
}

String transformModeForNode(const Node& node) {
    if (node.kind == NodeKind::Fft) {
        return parameterValueForNode(node, "window", "blackmanHarris");
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

bool isConcreteSignalDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:
        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
        case PortDomain::EnvelopeSignal:
        case PortDomain::MeshField:
            return true;

        default:
            return false;
    }
}

PortDomain domainFromVoiceContext(const Node& voiceNode) {
    const String domain = parameterValueForNode(voiceNode, "domain", "waveform");

    if (domain == "spectral" || domain == "spectralMagnitude") {
        return PortDomain::SpectralMagnitudeSignal;
    }

    if (domain == "spectralPhase") {
        return PortDomain::SpectralPhaseSignal;
    }

    return PortDomain::TimeSignal;
}

PortDomain domainFromContextInput(const NodeGraph& graph, const Node& node) {
    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment || edge.destNodeId != node.id || edge.destPortId != "context") {
            continue;
        }

        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        if (sourceNode != nullptr && sourceNode->kind == NodeKind::VoiceContext) {
            return domainFromVoiceContext(*sourceNode);
        }
    }

    return PortDomain::ControlSignal;
}

PortDomain firstResolvedInputDomain(const std::vector<Edge>& resolvedEdges, const String& nodeId) {
    for (const auto& edge : resolvedEdges) {
        if (edge.destNodeId == nodeId && isConcreteSignalDomain(edge.domain)) {
            return edge.domain;
        }
    }

    return PortDomain::ControlSignal;
}

PortDomain resolvedControlOutputDomain(
        const NodeGraph& graph,
        const Node& sourceNode,
        const Port& sourcePort,
        const std::vector<Edge>& resolvedEdges) {
    if (sourcePort.domain != PortDomain::ControlSignal) {
        return sourcePort.domain;
    }

    if (sourceNode.kind == NodeKind::TrilinearMesh && sourcePort.id == "out") {
        const PortDomain contextDomain = domainFromContextInput(graph, sourceNode);
        if (contextDomain != PortDomain::ControlSignal) {
            return contextDomain;
        }

        return firstResolvedInputDomain(resolvedEdges, sourceNode.id);
    }

    if (sourceNode.kind == NodeKind::Add || sourceNode.kind == NodeKind::Multiply) {
        return firstResolvedInputDomain(resolvedEdges, sourceNode.id);
    }

    return PortDomain::ControlSignal;
}

PortDomain resolvedEdgeDomain(
        const NodeGraph& graph,
        const Edge& edge,
        const std::vector<Edge>& resolvedEdges) {
    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    const Node* destNode = findNode(graph, edge.destNodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return edge.domain;
    }

    const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
    const Port* dest = findPort(*destNode, edge.destPortId, true);

    if (source == nullptr || dest == nullptr) {
        return edge.domain;
    }

    const PortDomain sourceDomain = resolvedControlOutputDomain(graph, *sourceNode, *source, resolvedEdges);

    if (sourceDomain != PortDomain::ControlSignal) {
        return sourceDomain;
    }

    if (dest->domain != PortDomain::ControlSignal) {
        return dest->domain;
    }

    return edge.domain;
}

ChannelLayout resolvedEdgeChannelLayout(const NodeGraph& graph, const Edge& edge) {
    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    const Node* destNode = findNode(graph, edge.destNodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return ChannelLayout::Mono;
    }

    const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
    const Port* dest = findPort(*destNode, edge.destPortId, true);

    if (source == nullptr || dest == nullptr) {
        return ChannelLayout::Mono;
    }

    if (dest->domain != PortDomain::ControlSignal) {
        return dest->channelLayout;
    }

    return source->channelLayout;
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
                    edge.domain,
                    resolvedEdgeChannelLayout(graph, edge)
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
                descriptor.audioRole,
                descriptor.previewRole,
                descriptor.previewable,
                descriptor.cycle1AdapterBacked,
                descriptor.cycle1Reference,
                parameterInt(node, "cycleFrames", 2048),
                latencyCyclesForNode(node),
                transformModeForNode(node),
                node.parameters,
                std::move(inputs)
        });
    }

    return steps;
}

std::vector<Edge> resolveSignalEdges(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder) {
    std::vector<Edge> resolvedEdges;

    for (const auto& nodeId : nodeOrder) {
        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment || edge.sourceNodeId != nodeId) {
                continue;
            }

            Edge resolved = edge;
            resolved.domain = resolvedEdgeDomain(graph, edge, resolvedEdges);
            resolvedEdges.push_back(std::move(resolved));
        }
    }

    return resolvedEdges;
}

bool hasBufferForSource(
        const std::vector<GraphBufferPlan>& buffers,
        const String& sourceNodeId,
        const String& sourcePortId) {
    for (const auto& buffer : buffers) {
        if (buffer.sourceNodeId == sourceNodeId && buffer.sourcePortId == sourcePortId) {
            return true;
        }
    }

    return false;
}

std::vector<GraphBufferPlan> buildBufferPlan(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges) {
    std::vector<GraphBufferPlan> buffers;

    for (const auto& edge : resolvedEdges) {
        if (edge.domain == PortDomain::DomainContext) {
            continue;
        }

        if (hasBufferForSource(buffers, edge.sourceNodeId, edge.sourcePortId)) {
            continue;
        }

        buffers.push_back({
                edge.sourceNodeId + "." + edge.sourcePortId,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.domain,
                resolvedEdgeChannelLayout(graph, edge)
        });
    }

    return buffers;
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
        result.plan.signalEdges = resolveSignalEdges(graph, result.plan.nodeOrder);
        result.plan.buffers = buildBufferPlan(graph, result.plan.signalEdges);

        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment) {
                result.plan.attachments.push_back(edge);
            }
        }

        result.plan.steps = buildExecutionSteps(graph, result.plan.nodeOrder, result.plan.signalEdges, moduleRegistry);
    }

    if (!result.succeeded()) {
        result.plan = {};
    }

    return result;
}

}
