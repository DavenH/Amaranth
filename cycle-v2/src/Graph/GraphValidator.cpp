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

bool isFixedWaveContextMismatch(const Node& sourceNode, const Node& destNode, const Port& dest) {
    GraphDomainResolver resolver;

    return sourceNode.kind == NodeKind::VoiceContext
        && destNode.kind == NodeKind::WaveSource
        && dest.id == "context"
        && resolver.domainFromVoiceContext(sourceNode) != PortDomain::TimeSignal;
}

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message,
        const Edge* edge = nullptr) {
    GraphValidationIssue issue { code, message };

    if (edge != nullptr) {
        issue.sourceNodeId = edge->sourceNodeId;
        issue.sourcePortId = edge->sourcePortId;
        issue.destNodeId = edge->destNodeId;
        issue.destPortId = edge->destPortId;
    }

    issues.push_back(std::move(issue));
}

}

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    std::vector<GraphValidationIssue> issues;

    for (const auto& edge : graph.getEdges()) {
        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        const Node* destNode = findNode(graph, edge.destNodeId);

        if (sourceNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourceNode,
                     "Missing source node: " + edge.sourceNodeId, &edge);
            continue;
        }

        if (destNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationNode,
                     "Missing destination node: " + edge.destNodeId, &edge);
            continue;
        }

        const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* dest = findPort(*destNode, edge.destPortId, true);

        if (source == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourcePort,
                     "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId, &edge);
            continue;
        }

        if (dest == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationPort,
                     "Missing destination port: " + edge.destNodeId + "." + edge.destPortId, &edge);
            continue;
        }

        if (edge.attachment) {
            if (source->domain != PortDomain::EnvelopeSignal) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentSource,
                         "Attachments currently require an envelope source: " + edge.sourceNodeId, &edge);
            }

            if (dest->purpose != PortPurpose::ScratchAttachment) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentDestination,
                         "Attachment destination is not a scratch port: " + edge.destNodeId + "." + dest->id, &edge);
            }

            continue;
        }

        if (dest->purpose == PortPurpose::ScratchAttachment) {
            addIssue(issues, GraphValidationCode::ScratchPortRequiresAttachment,
                     "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id, &edge);
            continue;
        }

        if (isFixedWaveContextMismatch(*sourceNode, *destNode, *dest)) {
            addIssue(issues, GraphValidationCode::DomainMismatch,
                     "Wave source requires waveform Voice Context: " + edge.sourceNodeId + " -> " + edge.destNodeId, &edge);
        }

        Port resolvedSource = *source;
        resolvedSource.domain = domainResolver.resolvedDomainForEdge(graph, edge);

        if (source->domain == PortDomain::ControlSignal
                && dest->domain != PortDomain::ControlSignal
                && resolvedSource.domain == dest->domain) {
            resolvedSource.channelLayout = dest->channelLayout;
        }

        if (!domainsCompatible(resolvedSource, *dest)) {
            addIssue(issues, GraphValidationCode::DomainMismatch,
                     "Domain mismatch: " + labelForDomain(resolvedSource.domain) + " -> " + labelForDomain(dest->domain),
                     &edge);
        }

        if (!channelLayoutsCompatible(resolvedSource, *dest)) {
            addIssue(issues, GraphValidationCode::ChannelLayoutMismatch,
                     "Channel layout mismatch: " + edge.sourceNodeId + "." + source->id
                         + " -> " + edge.destNodeId + "." + dest->id,
                     &edge);
        }

        if (source->domain == PortDomain::PitchSignal && !isVoiceAwareDestination(*dest)) {
            addIssue(issues, GraphValidationCode::PitchRequiresVoiceAwareDestination,
                     "Pitch can only feed voice-aware generators or processors: " + edge.destNodeId + "." + dest->id,
                     &edge);
        }
    }

    validateOperationInputs(graph, issues);

    return issues;
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const {
    return validationIssueForEdge(graph, edge).message.isNotEmpty();
}

GraphValidationIssue GraphValidator::validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const {
    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    const Node* destNode = findNode(graph, edge.destNodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return {
                sourceNode == nullptr ? GraphValidationCode::MissingSourceNode : GraphValidationCode::MissingDestinationNode,
                sourceNode == nullptr ? "Missing source node: " + edge.sourceNodeId : "Missing destination node: " + edge.destNodeId,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
    const Port* dest = findPort(*destNode, edge.destPortId, true);

    if (source == nullptr || dest == nullptr) {
        return {
                source == nullptr ? GraphValidationCode::MissingSourcePort : GraphValidationCode::MissingDestinationPort,
                source == nullptr
                        ? "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId
                        : "Missing destination port: " + edge.destNodeId + "." + edge.destPortId,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    if (edge.attachment) {
        if (source->domain != PortDomain::EnvelopeSignal) {
            return {
                    GraphValidationCode::InvalidAttachmentSource,
                    "Attachments currently require an envelope source: " + edge.sourceNodeId,
                    edge.sourceNodeId,
                    edge.sourcePortId,
                    edge.destNodeId,
                    edge.destPortId
            };
        }

        if (dest->purpose != PortPurpose::ScratchAttachment) {
            return {
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Attachment destination is not a scratch port: " + edge.destNodeId + "." + dest->id,
                    edge.sourceNodeId,
                    edge.sourcePortId,
                    edge.destNodeId,
                    edge.destPortId
            };
        }

        return {};
    }

    if (dest->purpose == PortPurpose::ScratchAttachment) {
        return {
                GraphValidationCode::ScratchPortRequiresAttachment,
                "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    if (isFixedWaveContextMismatch(*sourceNode, *destNode, *dest)) {
        return {
                GraphValidationCode::DomainMismatch,
                "Wave source requires waveform Voice Context: " + edge.sourceNodeId + " -> " + edge.destNodeId,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    Port resolvedSource = *source;
    resolvedSource.domain = resolvedDomainForEdge(graph, edge);

    if (source->domain == PortDomain::ControlSignal
            && dest->domain != PortDomain::ControlSignal
            && resolvedSource.domain == dest->domain) {
        resolvedSource.channelLayout = dest->channelLayout;
    }

    if (!domainsCompatible(resolvedSource, *dest)) {
        return {
                GraphValidationCode::DomainMismatch,
                "Domain mismatch: " + labelForDomain(resolvedSource.domain) + " -> " + labelForDomain(dest->domain),
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    if (!channelLayoutsCompatible(resolvedSource, *dest)) {
        return {
                GraphValidationCode::ChannelLayoutMismatch,
                "Channel layout mismatch: " + edge.sourceNodeId + "." + source->id
                    + " -> " + edge.destNodeId + "." + dest->id,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    if (source->domain == PortDomain::PitchSignal && !isVoiceAwareDestination(*dest)) {
        return {
                GraphValidationCode::PitchRequiresVoiceAwareDestination,
                "Pitch can only feed voice-aware generators or processors: " + edge.destNodeId + "." + dest->id,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        };
    }

    return {};
}

PortDomain GraphValidator::resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const {
    return domainResolver.resolvedDomainForEdge(graph, edge);
}

bool GraphValidator::isVoiceAwareDestination(const Port& port) const {
    return port.domain == PortDomain::PitchSignal || port.domain == PortDomain::VoiceControlSignal;
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

            const PortDomain domain = domainResolver.resolvedDomainForEdge(graph, edge);

            if (!GraphDomainResolver::isConcreteOperationDomain(domain)) {
                continue;
            }

            if (node.kind == NodeKind::Multiply && domain == PortDomain::SpectralPhaseSignal) {
                addIssue(issues, GraphValidationCode::DomainMismatch,
                         "Multiply cannot process spectral phase: " + node.id);
                break;
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
