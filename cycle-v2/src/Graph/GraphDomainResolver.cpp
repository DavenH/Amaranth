#include "GraphDomainResolver.h"

namespace CycleV2 {

const Node* GraphDomainResolver::findNode(const NodeGraph& graph, const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const Port* GraphDomainResolver::findPort(const Node& node, const String& id, bool input) const {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

PortDomain GraphDomainResolver::domainFromVoiceContext(const Node& voiceNode) const {
    const String domain = parameterValueForNode(voiceNode, "domain", "waveform");

    if (domain == "spectral" || domain == "spectralMagnitude") {
        return PortDomain::SpectralMagnitudeSignal;
    }

    if (domain == "spectralPhase") {
        return PortDomain::SpectralPhaseSignal;
    }

    return PortDomain::TimeSignal;
}

bool GraphDomainResolver::isConcreteOperationDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:
        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
            return true;

        default:
            return false;
    }
}

bool GraphDomainResolver::isConcreteSignalDomain(PortDomain domain) {
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

bool GraphDomainResolver::isContextResolvedSource(const Node& node, const Port& port) {
    if (port.id != "out") {
        return false;
    }

    switch (node.kind) {
        case NodeKind::TrilinearMesh:
        case NodeKind::TrilinearWaveSurface:
            return true;

        default:
            return false;
    }
}

PortDomain GraphDomainResolver::domainFromContextInput(const NodeGraph& graph, const Node& node) const {
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

PortDomain GraphDomainResolver::firstResolvedInputDomain(
        const NodeGraph& graph,
        const String& nodeId,
        int depth) const {
    if (depth > (int) graph.getNodes().size()) {
        return PortDomain::ControlSignal;
    }

    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment || edge.destNodeId != nodeId) {
            continue;
        }

        const PortDomain domain = resolvedEdgeDomain(graph, edge, depth + 1);
        if (isConcreteOperationDomain(domain)) {
            return domain;
        }
    }

    return PortDomain::ControlSignal;
}

PortDomain GraphDomainResolver::firstResolvedInputDomain(
        const std::vector<Edge>& resolvedEdges,
        const String& nodeId) const {
    for (const auto& edge : resolvedEdges) {
        if (edge.destNodeId == nodeId && isConcreteSignalDomain(edge.domain)) {
            return edge.domain;
        }
    }

    return PortDomain::ControlSignal;
}

PortDomain GraphDomainResolver::resolvedControlOutputDomain(
        const NodeGraph& graph,
        const Node& sourceNode,
        const Port& sourcePort,
        const std::vector<Edge>& resolvedEdges) const {
    if (sourcePort.domain != PortDomain::ControlSignal) {
        return sourcePort.domain;
    }

    if (sourceNode.kind == NodeKind::TrilinearMesh && sourcePort.id == "out") {
        const PortDomain contextDomain = domainFromContextInput(graph, sourceNode);
        if (contextDomain != PortDomain::ControlSignal) {
            return contextDomain;
        }
    }

    if (sourceNode.kind == NodeKind::Add || sourceNode.kind == NodeKind::Multiply) {
        return firstResolvedInputDomain(resolvedEdges, sourceNode.id);
    }

    return PortDomain::ControlSignal;
}

PortDomain GraphDomainResolver::resolvedEdgeDomain(
        const NodeGraph& graph,
        const Edge& edge,
        int depth) const {
    if (depth > (int) graph.getNodes().size()) {
        return edge.domain;
    }

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

    PortDomain sourceDomain = source->domain;

    if (sourceDomain == PortDomain::ControlSignal && isContextResolvedSource(*sourceNode, *source)) {
        sourceDomain = domainFromContextInput(graph, *sourceNode);
    }

    if (sourceDomain == PortDomain::ControlSignal
            && (sourceNode->kind == NodeKind::Add || sourceNode->kind == NodeKind::Multiply)) {
        sourceDomain = firstResolvedInputDomain(graph, sourceNode->id, depth + 1);
    }

    if (sourceDomain != PortDomain::ControlSignal) {
        return sourceDomain;
    }

    if (dest->domain != PortDomain::ControlSignal) {
        return dest->domain;
    }

    if (destNode->kind == NodeKind::Add || destNode->kind == NodeKind::Multiply) {
        const PortDomain operationDomain = firstResolvedInputDomain(graph, destNode->id, depth + 1);
        if (operationDomain != PortDomain::ControlSignal) {
            return operationDomain;
        }
    }

    return edge.domain;
}

PortDomain GraphDomainResolver::resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const {
    return resolvedEdgeDomain(graph, edge, 0);
}

PortDomain GraphDomainResolver::resolvedDomainForEdge(
        const NodeGraph& graph,
        const Edge& edge,
        const std::vector<Edge>& resolvedEdges) const {
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

    if (destNode->kind == NodeKind::Add || destNode->kind == NodeKind::Multiply) {
        const PortDomain operationDomain = firstResolvedInputDomain(resolvedEdges, destNode->id);
        if (operationDomain != PortDomain::ControlSignal) {
            return operationDomain;
        }
    }

    return edge.domain;
}

ChannelLayout GraphDomainResolver::resolvedChannelLayoutForEdge(
        const NodeGraph& graph,
        const Edge& edge) const {
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

std::vector<Edge> GraphDomainResolver::resolveSignalEdges(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder) const {
    std::vector<Edge> resolvedEdges;

    for (const auto& nodeId : nodeOrder) {
        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment || edge.sourceNodeId != nodeId) {
                continue;
            }

            Edge resolved = edge;
            resolved.domain = resolvedDomainForEdge(graph, edge, resolvedEdges);
            resolvedEdges.push_back(std::move(resolved));
        }
    }

    bool changed = true;
    int remainingPasses = (int) resolvedEdges.size();

    while (changed && remainingPasses > 0) {
        changed = false;
        --remainingPasses;

        for (auto& edge : resolvedEdges) {
            const PortDomain domain = resolvedDomainForEdge(graph, edge, resolvedEdges);

            if (edge.domain != domain) {
                edge.domain = domain;
                changed = true;
            }
        }
    }

    return resolvedEdges;
}

}
