#include "GraphValidator.h"

namespace CycleV2 {

namespace {

const Node* findNode(const NodeGraph& graph, const String& id) {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const Port* findPort(const Node& node, const String& id, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

bool isContextResolvedSource(const Node& node, const Port& port) {
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

bool isFixedWaveContextMismatch(const Node& sourceNode, const Node& destNode, const Port& dest) {
    return sourceNode.kind == NodeKind::VoiceContext
        && destNode.kind == NodeKind::WaveSource
        && dest.id == "context"
        && domainFromVoiceContext(sourceNode) != PortDomain::TimeSignal;
}

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message) {
    issues.push_back({ code, message });
}

}

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    std::vector<GraphValidationIssue> issues;

    for (const auto& edge : graph.getEdges()) {
        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        const Node* destNode = findNode(graph, edge.destNodeId);

        if (sourceNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourceNode,
                     "Missing source node: " + edge.sourceNodeId);
            continue;
        }

        if (destNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationNode,
                     "Missing destination node: " + edge.destNodeId);
            continue;
        }

        const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* dest = findPort(*destNode, edge.destPortId, true);

        if (source == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourcePort,
                     "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId);
            continue;
        }

        if (dest == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationPort,
                     "Missing destination port: " + edge.destNodeId + "." + edge.destPortId);
            continue;
        }

        if (edge.attachment) {
            if (source->domain != PortDomain::EnvelopeSignal) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentSource,
                         "Attachments currently require an envelope source: " + edge.sourceNodeId);
            }

            if (dest->purpose != PortPurpose::ScratchAttachment) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentDestination,
                         "Attachment destination is not a scratch port: " + edge.destNodeId + "." + dest->id);
            }

            continue;
        }

        if (dest->purpose == PortPurpose::ScratchAttachment) {
            addIssue(issues, GraphValidationCode::ScratchPortRequiresAttachment,
                     "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id);
            continue;
        }

        if (isFixedWaveContextMismatch(*sourceNode, *destNode, *dest)) {
            addIssue(issues, GraphValidationCode::DomainMismatch,
                     "Wave source requires waveform Voice Context: " + edge.sourceNodeId + " -> " + edge.destNodeId);
        }

        Port resolvedSource = *source;
        resolvedSource.domain = resolvedEdgeDomain(graph, edge);

        if (source->domain == PortDomain::ControlSignal
                && dest->domain != PortDomain::ControlSignal
                && resolvedSource.domain == dest->domain) {
            resolvedSource.channelLayout = dest->channelLayout;
        }

        if (!domainsCompatible(resolvedSource, *dest)) {
            addIssue(issues, GraphValidationCode::DomainMismatch,
                     "Domain mismatch: " + labelForDomain(resolvedSource.domain) + " -> " + labelForDomain(dest->domain));
        }

        if (!channelLayoutsCompatible(resolvedSource, *dest)) {
            addIssue(issues, GraphValidationCode::ChannelLayoutMismatch,
                     "Channel layout mismatch: " + edge.sourceNodeId + "." + source->id
                         + " -> " + edge.destNodeId + "." + dest->id);
        }

        if (source->domain == PortDomain::PitchSignal && !isVoiceAwareDestination(*dest)) {
            addIssue(issues, GraphValidationCode::PitchRequiresVoiceAwareDestination,
                     "Pitch can only feed voice-aware generators or processors: " + edge.destNodeId + "." + dest->id);
        }
    }

    validateOperationInputs(graph, issues);

    return issues;
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::isVoiceAwareDestination(const Port& port) const {
    return port.domain == PortDomain::PitchSignal || port.domain == PortDomain::VoiceControlSignal;
}

bool GraphValidator::concreteOperationDomain(PortDomain domain) const {
    switch (domain) {
        case PortDomain::TimeSignal:
        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
            return true;

        default:
            return false;
    }
}

PortDomain GraphValidator::domainFromContextInput(const NodeGraph& graph, const Node& node) const {
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

PortDomain GraphValidator::firstResolvedInputDomain(
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
        if (concreteOperationDomain(domain)) {
            return domain;
        }
    }

    return PortDomain::ControlSignal;
}

PortDomain GraphValidator::resolvedEdgeDomain(const NodeGraph& graph, const Edge& edge, int depth) const {
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

        if (sourceDomain == PortDomain::ControlSignal && sourceNode->kind == NodeKind::TrilinearMesh) {
            sourceDomain = firstResolvedInputDomain(graph, sourceNode->id, depth + 1);
        }
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

    return edge.domain;
}

void GraphValidator::validateOperationInputs(
        const NodeGraph& graph,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::Add && node.kind != NodeKind::Multiply) {
            continue;
        }

        PortDomain firstConcreteDomain {};
        bool hasConcreteDomain = false;

        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment || edge.destNodeId != node.id) {
                continue;
            }

            const PortDomain domain = resolvedEdgeDomain(graph, edge);

            if (!concreteOperationDomain(domain)) {
                continue;
            }

            if (!hasConcreteDomain) {
                firstConcreteDomain = domain;
                hasConcreteDomain = true;
                continue;
            }

            if (domain != firstConcreteDomain) {
                addIssue(issues, GraphValidationCode::MixedOperationDomains,
                         "Operation node mixes incompatible domains: " + node.id);
                break;
            }
        }
    }
}

bool GraphValidator::domainsCompatible(const Port& source, const Port& dest) const {
    if (source.domain == PortDomain::DomainContext || dest.domain == PortDomain::DomainContext) {
        return source.domain == dest.domain;
    }

    if (source.domain == PortDomain::ControlSignal || dest.domain == PortDomain::ControlSignal) {
        return true;
    }

    return source.domain == dest.domain;
}

bool GraphValidator::channelLayoutsCompatible(const Port& source, const Port& dest) const {
    if (source.domain != dest.domain) {
        return true;
    }

    switch (source.domain) {
        case PortDomain::TimeSignal:
        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
            return source.channelLayout == dest.channelLayout;

        default:
            return true;
    }
}

}
