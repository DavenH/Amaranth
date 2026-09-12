#include "Graph/GraphValidator.h"

#include <unordered_map>
#include <unordered_set>

#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

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

using NodeIdSet = std::unordered_set<
        String,
        GraphAudioScopeAnalysis::StringHash>;
using NodeAdjacency = std::unordered_map<
        String,
        std::vector<String>,
        GraphAudioScopeAnalysis::StringHash>;

NodeIdSet reachableGlobalNodes(
        const NodeGraph& graph,
        const GraphAudioScopeAnalysis& analysis,
        const String& root,
        bool reverse) {
    NodeAdjacency adjacency;
    for (const auto& edge : graph.getEdges()) {
        if (!edge.isAttachment()) {
            const String& from = reverse ? edge.destNodeId : edge.sourceNodeId;
            const String& to = reverse ? edge.sourceNodeId : edge.destNodeId;
            adjacency[from].push_back(to);
        }
    }
    NodeIdSet reachable { root };
    std::vector<String> pending { root };
    for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const auto found = adjacency.find(pending[cursor]);
        if (found == adjacency.end()) {
            continue;
        }
        for (const auto& nodeId : found->second) {
            if (analysis.scopeFor(nodeId) == AuthoredAudioScope::Global
                    && reachable.insert(nodeId).second) {
                pending.push_back(nodeId);
            }
        }
    }
    return reachable;
}

bool isLinkedStereoTimeOutput(const Port& port) {
    return port.domain == PortDomain::TimeSignal
            && port.channelLayout == ChannelLayout::LinkedStereo;
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
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(graph);

    for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
        validateEdge(
                graph,
                graph.getEdges()[edgeIndex],
                resolution.domains[edgeIndex],
                &scopeAnalysis,
                reporter);
    }

    for (const auto& assignment : graph.getGuideAssignments()) {
        const GuideCurveResource* guide = graph.findGuideCurve(assignment.guideId);
        const Node* target = graph.findNode(assignment.targetNodeId);
        if (guide == nullptr || target == nullptr
                || !TrimeshGuideAttachmentTarget::isValid(*target, assignment.target)) {
            addIssue(
                    issues,
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Guide assignment references an invalid resource or Trimesh cube component");
        }
    }

    for (const auto& guide : graph.getGuideCurves()) {
        if (guide.revision < 1
                || (guide.heatmapAssetId.isNotEmpty()
                        && graph.findGuideHeatmap(guide.heatmapAssetId) == nullptr)) {
            addIssue(
                    issues,
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Guide resource references an invalid heatmap asset");
        }
    }

    validateOperationInputs(graph, resolution, issues);
    validateAudioScopes(graph, scopeAnalysis, issues);

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
    const auto analysis = GraphAudioScopeAnalyzer().analyze(graph);
    validateEdge(
            graph,
            edge,
            domainResolver.resolvedDomainForEdge(graph, edge),
            &analysis,
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

    if (isFixedWaveContextMismatch(*sourceNode, *destNode, *dest)) {
        if (!report(
                    GraphValidationCode::DomainMismatch,
                    "Wave source requires waveform Voice Context: " + edge.sourceNodeId + " -> " + edge.destNodeId)) {
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

void GraphValidator::validateOperationInputs(
        const NodeGraph& graph,
        const GraphDomainResolution& resolution,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::SpectralLayer) {
            const auto found = std::find_if(
                    graph.getEdges().begin(),
                    graph.getEdges().end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment() && edge.destNodeId == node.id;
                    });
            if (found != graph.getEdges().end()) {
                const size_t edgeIndex = (size_t) std::distance(
                        graph.getEdges().begin(),
                        found);
                const PortDomain domain = resolution.domains[edgeIndex];
                if (domain != PortDomain::TimeSignal
                        && domain != PortDomain::SpectralMagnitudeSignal
                        && domain != PortDomain::SpectralPhaseSignal) {
                    addIssue(issues, GraphValidationCode::DomainMismatch,
                            "Pan requires time, magnitude, or phase input: " + node.id);
                }
            }
            continue;
        }
        if (node.kind != NodeKind::Add && node.kind != NodeKind::Multiply) {
            continue;
        }

        PortDomain firstConcreteDomain {};
        bool hasConcreteDomain = false;

        for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
            const Edge& edge = graph.getEdges()[edgeIndex];
            if (edge.isAttachment() || edge.destNodeId != node.id) {
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

void GraphValidator::validateAudioScopes(
        const NodeGraph& graph,
        const GraphAudioScopeAnalysis& analysis,
        std::vector<GraphValidationIssue>& issues) const {
    std::vector<String> globalInputIds;
    std::vector<String> voiceOutputIds;
    std::vector<String> outputIds;
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::VoiceOutput) {
            voiceOutputIds.push_back(node.id);
        } else if (node.kind == NodeKind::GlobalInput) {
            globalInputIds.push_back(node.id);
        } else if (node.kind == NodeKind::Output) {
            outputIds.push_back(node.id);
        }
    }
    if (voiceOutputIds.size() != 1) {
        addIssue(
                issues,
                voiceOutputIds.empty()
                        ? GraphValidationCode::MissingRequiredNode
                        : GraphValidationCode::DuplicateSingletonNode,
                "Audio graph requires exactly one Voice Output");
    }
    if (globalInputIds.size() != 1) {
        addIssue(
                issues,
                globalInputIds.empty()
                        ? GraphValidationCode::MissingRequiredNode
                        : GraphValidationCode::DuplicateSingletonNode,
                "Audio graph requires exactly one Global Input");
    }
    if (outputIds.size() != 1) {
        addIssue(
                issues,
                outputIds.empty()
                        ? GraphValidationCode::MissingRequiredNode
                        : GraphValidationCode::DuplicateSingletonNode,
                "Audio graph requires exactly one Output");
    }

    for (const auto& nodeId : analysis.conflictingNeutralNodeIds) {
        addIssue(
                issues,
                GraphValidationCode::ConflictingProcessingScope,
                "Routing node participates in both voice and global graphs: " + nodeId);
    }
    if (voiceOutputIds.size() != 1
            || globalInputIds.size() != 1
            || outputIds.size() != 1) {
        return;
    }

    const auto fromInput = reachableGlobalNodes(
            graph,
            analysis,
            globalInputIds.front(),
            false);
    const auto toOutput = reachableGlobalNodes(
            graph,
            analysis,
            outputIds.front(),
            true);
    for (const auto& node : graph.getNodes()) {
        if (analysis.scopeFor(node.id) != AuthoredAudioScope::Global) {
            continue;
        }
        if (fromInput.count(node.id) == 0) {
            addIssue(
                    issues,
                    GraphValidationCode::GlobalNodeUnreachable,
                    "Global node is not reachable from Global Input: " + node.id);
        }
        if (toOutput.count(node.id) == 0) {
            addIssue(
                    issues,
                    GraphValidationCode::GlobalNodeCannotReachOutput,
                    "Global node does not reach Output: " + node.id);
        }
    }

    int bypassingTerminalCount = 0;
    for (const auto& node : graph.getNodes()) {
        if (analysis.scopeFor(node.id) != AuthoredAudioScope::Voice) {
            continue;
        }
        for (const auto& output : node.outputs) {
            if (!isLinkedStereoTimeOutput(output)) {
                continue;
            }
            const bool consumedInVoiceGraph = std::any_of(
                    graph.getEdges().begin(),
                    graph.getEdges().end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment()
                                && edge.sourceNodeId == node.id
                                && edge.sourcePortId == output.id
                                && analysis.scopeFor(edge.destNodeId)
                                        == AuthoredAudioScope::Voice;
                    });
            const bool feedsVoiceOutput = std::any_of(
                    graph.getEdges().begin(),
                    graph.getEdges().end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment()
                                && edge.sourceNodeId == node.id
                                && edge.sourcePortId == output.id
                                && edge.destNodeId == voiceOutputIds.front();
                    });
            bypassingTerminalCount += consumedInVoiceGraph || feedsVoiceOutput ? 0 : 1;
        }
    }
    if (bypassingTerminalCount > 0) {
        addIssue(
                issues,
                GraphValidationCode::AmbiguousVoiceOutput,
                "Voice audio path does not terminate at Voice Output");
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
