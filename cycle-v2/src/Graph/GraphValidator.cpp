#include <unordered_set>

#include "Graph/GraphValidator.h"

#include "Graph/GraphAudioValidationFacts.h"
#include "Graph/GraphAudioScopeValidator.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeValidator.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphGuideValidator.h"
#include "Graph/GraphTopologyValidator.h"
#include "Graph/GraphValidationContext.h"

namespace CycleV2 {

namespace {

bool sameValidationIssue(
        const GraphValidationIssue& first,
        const GraphValidationIssue& second) {
    return first.code == second.code
            && first.sourceNodeId == second.sourceNodeId
            && first.sourcePortId == second.sourcePortId
            && first.destNodeId == second.destNodeId
            && first.destPortId == second.destPortId
            && first.subjectId == second.subjectId;
}

using StringSet = std::unordered_set<String, GraphAudioScopeAnalysis::StringHash>;

String edgeKey(const Edge& edge) {
    return edge.sourceNodeId + "\n" + edge.sourcePortId + "\n"
            + edge.destNodeId + "\n" + edge.destPortId;
}

String edgeKey(const GraphValidationIssue& issue) {
    return issue.sourceNodeId + "\n" + issue.sourcePortId + "\n"
            + issue.destNodeId + "\n" + issue.destPortId;
}

bool changesVoiceContextAssignment(const GraphEdgeView& edges) {
    for (const size_t removedIndex : edges.removedIndices()) {
        if (edges.existingEdge(removedIndex).destPortId == "context") {
            return true;
        }
    }
    return std::any_of(
            edges.addedEdges().begin(),
            edges.addedEdges().end(),
            [](const Edge& edge) { return edge.destPortId == "context"; });
}

bool isAudioGraphIssue(GraphValidationCode code) {
    return code == GraphValidationCode::MissingRequiredNode
            || code == GraphValidationCode::DuplicateSingletonNode
            || code == GraphValidationCode::ConflictingProcessingScope
            || code == GraphValidationCode::GlobalNodeUnreachable
            || code == GraphValidationCode::GlobalNodeCannotReachOutput
            || code == GraphValidationCode::AmbiguousVoiceOutput;
}

}

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    return validate(graph, GraphEdgeView(graph.getEdges()));
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    std::vector<GraphValidationIssue> issues;
    const GraphDomainResolution resolution = domainResolver.resolve(graph, edges);
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(graph, edges);
    return validate(graph, edges, resolution, scopeAnalysis);
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        const GraphAudioScopeAnalysis& scopeAnalysis) const {
    const GraphAudioValidationFacts audioFacts(graph, edges, scopeAnalysis);
    return validate(graph, edges, resolution, scopeAnalysis, audioFacts);
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        const GraphAudioScopeAnalysis& scopeAnalysis,
        const GraphAudioValidationFacts& audioFacts) const {
    std::vector<GraphValidationIssue> issues;
    GraphEdgeValidator edgeValidator;
    const bool explicitAudioGraph = audioFacts.usesExplicitAudioGraph();

    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        edgeValidator.validate(
                graph,
                edges[edgeIndex],
                resolution.domains[edgeIndex],
                explicitAudioGraph ? &scopeAnalysis : nullptr,
                issues);
    }

    GraphGuideValidator().validate(graph, issues);
    GraphTopologyValidator().validate(graph, edges, resolution, issues);
    GraphAudioScopeValidator().validate(scopeAnalysis, audioFacts, issues);

    return issues;
}

std::vector<GraphValidationIssue> GraphValidator::validateProposal(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphEdgeIndexOverlay& edgeIndex,
        const GraphValidationContext& baseline) const {
    const auto resolution = domainResolver.resolve(
            graph,
            edges,
            edgeIndex,
            baseline.domainResolution());
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(
            graph,
            edges,
            edgeIndex,
            baseline.audioScopeAnalysis());
    const GraphAudioValidationFacts audioFacts(
            graph,
            edges,
            edgeIndex,
            baseline.audioScopeAnalysis(),
            scopeAnalysis,
            baseline.audioValidationFacts());
    const bool voiceContextChanged = changesVoiceContextAssignment(edges);

    std::vector<size_t> edgesToValidate;
    std::unordered_set<size_t> includedEdges;
    StringSet invalidatedEdgeKeys;
    StringSet affectedOperationNodes;
    const auto invalidateEdge = [&](size_t proposedIndex) {
        if (proposedIndex >= edges.size()
                || !includedEdges.insert(proposedIndex).second) {
            return;
        }
        const Edge& edge = edges[proposedIndex];
        edgesToValidate.push_back(proposedIndex);
        invalidatedEdgeKeys.emplace(edgeKey(edge));
        affectedOperationNodes.emplace(edge.destNodeId);
    };
    for (const size_t removedIndex : edges.removedIndices()) {
        const Edge& removed = edges.existingEdge(removedIndex);
        invalidatedEdgeKeys.emplace(edgeKey(removed));
        affectedOperationNodes.emplace(removed.destNodeId);
    }
    for (const size_t affectedEdge : resolution.affectedEdgeIndices) {
        invalidateEdge(affectedEdge);
    }
    for (size_t proposedIndex = edges.retainedSize();
            proposedIndex < edges.size();
            ++proposedIndex) {
        invalidateEdge(proposedIndex);
    }
    for (const String& nodeId : scopeAnalysis.affectedNodeIds) {
        for (const size_t incoming : edgeIndex.incomingEdges(nodeId)) {
            invalidateEdge(incoming);
        }
        for (const size_t outgoing : edgeIndex.outgoingEdges(nodeId)) {
            invalidateEdge(outgoing);
        }
    }

    std::vector<GraphValidationIssue> issues;
    for (const auto& issue : baseline.validationIssues()) {
        const bool invalidatedEdge = issue.sourceNodeId.isNotEmpty()
                && invalidatedEdgeKeys.count(edgeKey(issue)) > 0;
        const bool operationIssue = issue.sourceNodeId.isEmpty()
                && (issue.code == GraphValidationCode::DomainMismatch
                        || issue.code == GraphValidationCode::MixedOperationDomains);
        const bool invalidatedOperation = operationIssue
                && issue.subjectId.isNotEmpty()
                && affectedOperationNodes.count(issue.subjectId) > 0;
        const bool voiceContextIssue = voiceContextChanged
                && (issue.code == GraphValidationCode::MissingVoiceContextAssignment
                        || issue.code == GraphValidationCode::MultipleActiveVoiceContexts);
        if (!invalidatedEdge
                && !invalidatedOperation
                && !voiceContextIssue
                && !isAudioGraphIssue(issue.code)) {
            issues.push_back(issue);
        }
    }

    GraphEdgeValidator edgeValidator;
    const GraphAudioScopeAnalysis* edgeScopes = audioFacts.usesExplicitAudioGraph()
            ? &scopeAnalysis
            : nullptr;
    for (const size_t edgeIndexToValidate : edgesToValidate) {
        edgeValidator.validate(
                graph,
                edges[edgeIndexToValidate],
                resolution.domains[edgeIndexToValidate],
                edgeScopes,
                issues);
    }

    GraphTopologyValidator topologyValidator;
    for (const String& nodeId : affectedOperationNodes) {
        const Node* node = graph.findNode(nodeId);
        if (node != nullptr) {
            topologyValidator.validateOperationNode(
                    *node,
                    edges,
                    edgeIndex,
                    resolution,
                    issues);
        }
    }
    if (voiceContextChanged) {
        const GraphVoiceContextAssignments assignments(
                baseline.voiceContextAssignments(),
                edges);
        topologyValidator.validateVoiceContextAssignments(
                graph,
                assignments,
                issues);
    }
    GraphAudioScopeValidator().validate(scopeAnalysis, audioFacts, issues);
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
    const auto analysis = GraphAudioScopeAnalyzer().analyze(graph);
    const bool explicitAudioGraph = GraphAudioValidationFacts::usesExplicitAudioGraph(graph);
    return GraphEdgeValidator().firstIssue(
            graph,
            edge,
            domainResolver.resolvedDomainForEdge(graph, edge),
            explicitAudioGraph ? &analysis : nullptr);
}

PortDomain GraphValidator::resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const {
    return domainResolver.resolvedDomainForEdge(graph, edge);
}

}
