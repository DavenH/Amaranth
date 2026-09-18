#include "Graph/GraphValidator.h"

#include <unordered_map>
#include <unordered_set>

#include "Graph/GraphEdgeView.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Guide/GuideAttachmentTarget.h"

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
        const GraphEdgeView& edges,
        const GraphAudioScopeAnalysis& analysis,
        const String& root,
        bool reverse) {
    NodeAdjacency adjacency;
    for (const auto& edge : edges) {
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

bool usesExplicitAudioGraph(const NodeGraph& graph) {
    return std::any_of(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::GlobalInput
                        || node.kind == NodeKind::VoiceOutput;
            });
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
    return validate(graph, GraphEdgeView(graph.getEdges()));
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    std::vector<GraphValidationIssue> issues;
    EdgeIssueReporter reporter(issues);
    const GraphDomainResolution resolution = domainResolver.resolve(graph, edges);
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(graph, edges);
    const bool explicitAudioGraph = usesExplicitAudioGraph(graph);

    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        validateEdge(
                graph,
                edges[edgeIndex],
                resolution.domains[edgeIndex],
                explicitAudioGraph ? &scopeAnalysis : nullptr,
                reporter);
    }

    for (const auto& assignment : graph.getGuideAssignments()) {
        const GuideCurveResource* guide = graph.findGuideCurve(assignment.guideId);
        const Node* target = graph.findNode(assignment.targetNodeId);
        const bool validTarget = target != nullptr
                && (assignment.targetKind == GuideCurveTargetKind::EnvelopeCubeComponent
                        ? target->kind == NodeKind::Envelope
                        : target->kind == NodeKind::TrilinearMesh)
                && GuideAttachmentTarget::isValid(*target, assignment.target);
        if (guide == nullptr || target == nullptr
                || !validTarget) {
            addIssue(
                    issues,
                    GraphValidationCode::InvalidAttachmentDestination,
                    "Guide assignment references an invalid resource or cube component");
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

    validateOperationInputs(graph, edges, resolution, issues);
    validateAudioScopes(graph, edges, scopeAnalysis, issues);
    validateVoiceContextAssignments(graph, edges, issues);

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
    const bool explicitAudioGraph = usesExplicitAudioGraph(graph);
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
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::SpectralLayer) {
            const auto found = std::find_if(
                    edges.begin(),
                    edges.end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment() && edge.destNodeId == node.id;
                    });
            if (found != edges.end()) {
                const size_t edgeIndex = (size_t) std::distance(
                        edges.begin(),
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

        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const Edge& edge = edges[edgeIndex];
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

void GraphValidator::validateVoiceContextAssignments(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        std::vector<GraphValidationIssue>& issues) const {
    const int voiceContextCount = static_cast<int>(std::count_if(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::VoiceContext;
            }));
    if (voiceContextCount <= 1) {
        return;
    }

    std::unordered_map<String, String, GraphAudioScopeAnalysis::StringHash>
            explicitAssignments;
    for (const auto& edge : edges) {
        if (!edge.isAttachment() && edge.destPortId == "context") {
            explicitAssignments.emplace(edge.destNodeId, edge.sourceNodeId);
        }
    }

    NodeIdSet activeContexts;
    for (const auto& node : graph.getNodes()) {
        const bool acceptsContext = std::any_of(
                node.inputs.begin(),
                node.inputs.end(),
                [](const Port& port) {
                    return port.id == "context"
                            && port.domain == PortDomain::DomainContext;
                });
        if (!acceptsContext) {
            continue;
        }
        const auto assignment = explicitAssignments.find(node.id);
        if (assignment == explicitAssignments.end()) {
            addIssue(
                    issues,
                    GraphValidationCode::MissingVoiceContextAssignment,
                    "Multiple Voice Contexts require an explicit context for " + node.id);
            continue;
        }
        const Node* source = graph.findNode(assignment->second);
        if (source != nullptr && source->kind == NodeKind::VoiceContext) {
            activeContexts.emplace(source->id);
        }
    }

    if (activeContexts.size() > 1) {
        addIssue(
                issues,
                GraphValidationCode::MultipleActiveVoiceContexts,
                "Only one Voice Context can participate until multi-oscillator semantics are defined");
    }
}

void GraphValidator::validateAudioScopes(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
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
    // NodeGraph does not retain its serialized format version. Durable graphs are
    // migrated before validation, while tests and domain clients also compile
    // boundary-free graph fragments. The presence of either explicit boundary
    // opts the graph into the format-six audio grammar.
    if (voiceOutputIds.empty() && globalInputIds.empty()) {
        return;
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
            edges,
            analysis,
            globalInputIds.front(),
            false);
    const auto toOutput = reachableGlobalNodes(
            edges,
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
                    edges.begin(),
                    edges.end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment()
                                && edge.sourceNodeId == node.id
                                && edge.sourcePortId == output.id
                                && analysis.scopeFor(edge.destNodeId)
                                        == AuthoredAudioScope::Voice;
                    });
            const bool feedsVoiceOutput = std::any_of(
                    edges.begin(),
                    edges.end(),
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
