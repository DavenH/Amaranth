#include "GraphValidator.h"

#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"

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

bool isTrimeshGuideTarget(const Node& node, const String& portId) {
    if (node.kind != NodeKind::TrilinearMesh) {
        return false;
    }

    return TrimeshGuideAttachmentTarget::parse(portId).isValid();
}

bool isFixedWaveContextMismatch(const Node& sourceNode, const Node& destNode, const Port& dest) {
    GraphDomainResolver resolver;

    return sourceNode.kind == NodeKind::VoiceContext
        && destNode.kind == NodeKind::WaveSource
        && dest.id == "context"
        && resolver.domainFromVoiceContext(sourceNode) != PortDomain::TimeSignal;
}

bool isSpySignalDomain(PortDomain domain) {
    return domain == PortDomain::TimeSignal
        || domain == PortDomain::SpectralMagnitudeSignal
        || domain == PortDomain::SpectralPhaseSignal;
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

class GraphValidator::EdgeIssueReporter {
public:
    explicit EdgeIssueReporter(std::vector<GraphValidationIssue>& issues) :
            issues(&issues) {}

    EdgeIssueReporter() = default;

    bool report(GraphValidationIssue issue) {
        if (issues != nullptr) {
            issues->push_back(std::move(issue));
            return true;
        }

        firstIssue = std::move(issue);
        return false;
    }

    const GraphValidationIssue& getFirstIssue() const { return firstIssue; }

private:
    std::vector<GraphValidationIssue>* issues {};
    GraphValidationIssue firstIssue;
};

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    std::vector<GraphValidationIssue> issues;
    EdgeIssueReporter reporter(issues);
    const GraphDomainResolution resolution = domainResolver.resolve(graph);

    for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
        validateEdge(
                graph,
                graph.getEdges()[edgeIndex],
                resolution.domains[edgeIndex],
                reporter);
    }

    validateOperationInputs(graph, resolution, issues);

    return issues;
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const {
    return validationIssueForEdge(graph, edge).message.isNotEmpty();
}

GraphValidationIssue GraphValidator::validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const {
    EdgeIssueReporter reporter;
    validateEdge(
            graph,
            edge,
            domainResolver.resolvedDomainForEdge(graph, edge),
            reporter);
    return reporter.getFirstIssue();
}

void GraphValidator::validateEdge(
        const NodeGraph& graph,
        const Edge& edge,
        PortDomain resolvedDomain,
        EdgeIssueReporter& reporter) const {
    auto report = [&edge, &reporter](GraphValidationCode code, String message) {
        return reporter.report({
                code,
                std::move(message),
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destNodeId,
                edge.destPortId
        });
    };

    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    const Node* destNode = findNode(graph, edge.destNodeId);

    if (sourceNode == nullptr) {
        report(GraphValidationCode::MissingSourceNode, "Missing source node: " + edge.sourceNodeId);
        return;
    }

    if (destNode == nullptr) {
        report(GraphValidationCode::MissingDestinationNode, "Missing destination node: " + edge.destNodeId);
        return;
    }

    const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
    const Port* dest = findPort(*destNode, edge.destPortId, true);
    const bool trimeshGuideTarget = edge.attachment
            && dest == nullptr
            && isTrimeshGuideTarget(*destNode, edge.destPortId);

    if (source == nullptr) {
        report(
                GraphValidationCode::MissingSourcePort,
                "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId);
        return;
    }

    if (dest == nullptr && !trimeshGuideTarget) {
        report(
                GraphValidationCode::MissingDestinationPort,
                "Missing destination port: " + edge.destNodeId + "." + edge.destPortId);
        return;
    }

    if (edge.attachment) {
        if (trimeshGuideTarget) {
            if (sourceNode->kind != NodeKind::GuideCurve) {
                report(
                        GraphValidationCode::InvalidAttachmentSource,
                        "Trimesh guide attachments require a Guide Curve source: " + edge.sourceNodeId);
            }

            return;
        }

        if (source->domain != PortDomain::EnvelopeSignal) {
            if (!report(
                        GraphValidationCode::InvalidAttachmentSource,
                        "Attachments currently require an envelope source: " + edge.sourceNodeId)) {
                return;
            }
        }

        if (dest->purpose != PortPurpose::ScratchAttachment) {
            report(
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Attachment destination is not a scratch port: " + edge.destNodeId + "." + dest->id);
        }

        return;
    }

    if (dest->purpose == PortPurpose::ScratchAttachment) {
        report(
                GraphValidationCode::ScratchPortRequiresAttachment,
                "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id);
        return;
    }

    if (isFixedWaveContextMismatch(*sourceNode, *destNode, *dest)) {
        if (!report(
                    GraphValidationCode::DomainMismatch,
                    "Wave source requires waveform Voice Context: " + edge.sourceNodeId + " -> " + edge.destNodeId)) {
            return;
        }
    }

    Port resolvedSource = *source;
    resolvedSource.domain = resolvedDomain;

    if (destNode->kind == NodeKind::Spy && !isSpySignalDomain(resolvedSource.domain)) {
        if (!report(
                    GraphValidationCode::DomainMismatch,
                    "Spy nodes can only monitor time, magnitude, or phase signals: " + edge.destNodeId)) {
            return;
        }
    }

    if (source->domain == PortDomain::ControlSignal
            && dest->domain != PortDomain::ControlSignal
            && resolvedSource.domain == dest->domain) {
        resolvedSource.channelLayout = dest->channelLayout;
    }

    if (!domainsCompatible(resolvedSource, *dest)) {
        if (!report(
                    GraphValidationCode::DomainMismatch,
                    "Domain mismatch: " + labelForDomain(resolvedSource.domain) + " -> " + labelForDomain(dest->domain))) {
            return;
        }
    }

    if (!channelLayoutsCompatible(resolvedSource, *dest)) {
        if (!report(
                    GraphValidationCode::ChannelLayoutMismatch,
                    "Channel layout mismatch: " + edge.sourceNodeId + "." + source->id
                        + " -> " + edge.destNodeId + "." + dest->id)) {
            return;
        }
    }

    if (source->domain == PortDomain::PitchSignal && !isVoiceAwareDestination(*dest)) {
        report(
                GraphValidationCode::PitchRequiresVoiceAwareDestination,
                "Pitch can only feed voice-aware generators or processors: " + edge.destNodeId + "." + dest->id);
    }
}

PortDomain GraphValidator::resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const {
    return domainResolver.resolvedDomainForEdge(graph, edge);
}

bool GraphValidator::isVoiceAwareDestination(const Port& port) const {
    return port.domain == PortDomain::PitchSignal || port.domain == PortDomain::VoiceControlSignal;
}

void GraphValidator::validateOperationInputs(
        const NodeGraph& graph,
        const GraphDomainResolution& resolution,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::Add && node.kind != NodeKind::Multiply) {
            continue;
        }

        PortDomain firstConcreteDomain {};
        bool hasConcreteDomain = false;

        for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
            const Edge& edge = graph.getEdges()[edgeIndex];
            if (edge.attachment || edge.destNodeId != node.id) {
                continue;
            }

            const PortDomain domain = resolution.domains[edgeIndex];

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
