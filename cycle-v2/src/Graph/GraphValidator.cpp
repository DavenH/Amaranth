#include "Graph/GraphValidator.h"

#include "Graph/GraphAudioScopeValidator.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphGuideValidator.h"
#include "Graph/GraphTopologyValidator.h"
#include "Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

namespace {

const Port* findPort(const Node& node, const String& id, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

bool isValidScratchBindingSource(const Node& sourceNode, const Port& source) {
    if (source.connectionKind != ConnectionKind::ProcessingAttachment
            || source.attachmentType != AttachmentType::ScratchEnvelope) {
        return false;
    }
    return sourceNode.kind != NodeKind::Envelope
            || envelopePurposeFor(sourceNode) == EnvelopePurpose::Scratch;
}

bool isValidScratchBindingDestination(
        const Node& sourceNode,
        const Node& destNode,
        const Port& dest) {
    if (dest.connectionKind != ConnectionKind::ProcessingAttachment
            || dest.attachmentType != AttachmentType::ScratchEnvelope
            || dest.purpose != PortPurpose::ScratchAttachment) {
        return false;
    }
    return sourceNode.kind != NodeKind::ScratchDefaultOverride
            || destNode.kind == NodeKind::TrilinearMesh;
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

bool sameValidationIssue(
        const GraphValidationIssue& first,
        const GraphValidationIssue& second) {
    return first.code == second.code
            && first.sourceNodeId == second.sourceNodeId
            && first.sourcePortId == second.sourcePortId
            && first.destNodeId == second.destNodeId
            && first.destPortId == second.destPortId;
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
    return validate(graph, GraphEdgeView(graph.getEdges()));
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    std::vector<GraphValidationIssue> issues;
    EdgeIssueReporter reporter(issues);
    const GraphDomainResolution resolution = domainResolver.resolve(graph, edges);
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(graph, edges);
    const bool explicitAudioGraph = GraphAudioScopeValidator::usesExplicitAudioGraph(graph);

    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        validateEdge(
                graph,
                edges[edgeIndex],
                resolution.domains[edgeIndex],
                explicitAudioGraph ? &scopeAnalysis : nullptr,
                reporter);
    }

    GraphGuideValidator().validate(graph, issues);
    GraphTopologyValidator().validate(graph, edges, resolution, issues);
    GraphAudioScopeValidator().validate(graph, edges, scopeAnalysis, issues);

    return issues;
}

bool GraphValidator::acceptsProposedIssues(
        const std::vector<GraphValidationIssue>& before,
        const std::vector<GraphValidationIssue>& after) {
    if (after.empty()) {
        return true;
    }
    if (before.empty() || after.size() >= before.size()) {
        return false;
    }
    return std::all_of(after.begin(), after.end(), [&](const auto& issue) {
        return std::any_of(before.begin(), before.end(), [&](const auto& prior) {
            return sameValidationIssue(issue, prior);
        });
    });
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const {
    return validationIssueForEdge(graph, edge).message.isNotEmpty();
}

GraphValidationIssue GraphValidator::validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const {
    EdgeIssueReporter reporter;
    const auto analysis = GraphAudioScopeAnalyzer().analyze(graph);
    const bool explicitAudioGraph = GraphAudioScopeValidator::usesExplicitAudioGraph(graph);
    validateEdge(
            graph,
            edge,
            domainResolver.resolvedDomainForEdge(graph, edge),
            explicitAudioGraph ? &analysis : nullptr,
            reporter);
    return reporter.getFirstIssue();
}

void GraphValidator::validateEdge(
        const NodeGraph& graph,
        const Edge& edge,
        PortDomain resolvedDomain,
        const GraphAudioScopeAnalysis* scopeAnalysis,
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

    const Node* sourceNode = graph.findNode(edge.sourceNodeId);
    const Node* destNode = graph.findNode(edge.destNodeId);

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
    if (source == nullptr) {
        report(
                GraphValidationCode::MissingSourcePort,
                "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId);
        return;
    }

    if (dest == nullptr) {
        report(
                GraphValidationCode::MissingDestinationPort,
                "Missing destination port: " + edge.destNodeId + "." + edge.destPortId);
        return;
    }

    if (scopeAnalysis != nullptr
            && scopeAnalysis->scopeFor(sourceNode->id)
                    != scopeAnalysis->scopeFor(destNode->id)) {
        report(
                GraphValidationCode::ProcessingScopeMismatch,
                "Processing scope mismatch: " + sourceNode->id + "." + source->id
                        + " -> " + destNode->id + "." + dest->id);
        return;
    }

    if (edge.isConfigurationAttachment()) {
        if (source->connectionKind != ConnectionKind::ConfigurationAttachment
                || dest == nullptr
                || dest->connectionKind != ConnectionKind::ConfigurationAttachment
                || source->attachmentType == AttachmentType::None
                || source->attachmentType != dest->attachmentType
                || edge.attachmentType != dest->attachmentType) {
            report(
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Configuration attachment types do not match: "
                            + edge.sourceNodeId + " -> " + edge.destNodeId);
        }
        return;
    }

    if (edge.isProcessingAttachment()) {
        if (!isValidScratchBindingSource(*sourceNode, *source)
                || edge.attachmentType != source->attachmentType) {
            if (!report(
                        GraphValidationCode::InvalidAttachmentSource,
                        "Scratch attachments require a registered scratch-binding source: "
                                + edge.sourceNodeId)) {
                return;
            }
        }

        if (!isValidScratchBindingDestination(*sourceNode, *destNode, *dest)
                || edge.attachmentType != dest->attachmentType) {
            report(
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Attachment destination is not compatible with the scratch binding: "
                            + edge.destNodeId + "." + dest->id);
        }

        return;
    }

    if (dest->connectionKind != ConnectionKind::Signal
            || source->connectionKind != ConnectionKind::Signal) {
        report(
                GraphValidationCode::ScratchPortRequiresAttachment,
                "Attachment ports require typed attachment routing: "
                        + edge.sourceNodeId + " -> " + edge.destNodeId);
        return;
    }

    if (dest->purpose == PortPurpose::ScratchAttachment) {
        report(
                GraphValidationCode::ScratchPortRequiresAttachment,
                "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id);
        return;
    }

    if (sourceNode->kind == NodeKind::Envelope) {
        const EnvelopePurpose purpose = envelopePurposeFor(*sourceNode);
        if (edge.connectionKind != envelopeConnectionKind(purpose)) {
            report(
                    GraphValidationCode::InvalidAttachmentSource,
                    "Envelope purpose does not match connection kind: " + edge.sourceNodeId);
            return;
        }
        if (purpose == EnvelopePurpose::Volume && destNode->kind != NodeKind::Multiply) {
            report(
                    GraphValidationCode::DomainMismatch,
                    "Volume envelopes can only feed gain/factor operations: " + edge.destNodeId);
            return;
        }
        if (purpose == EnvelopePurpose::Pitch
                && !(destNode->kind == NodeKind::VoiceContext && dest->id == "pitch")) {
            report(
                    GraphValidationCode::PitchRequiresVoiceAwareDestination,
                    "Pitch envelopes can only feed Voice Context pitch: " + edge.destNodeId);
            return;
        }
    }

    Port resolvedSource = *source;
    resolvedSource.domain = resolvedDomain;

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
