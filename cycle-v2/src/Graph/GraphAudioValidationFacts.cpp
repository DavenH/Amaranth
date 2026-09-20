#include <algorithm>
#include <deque>

#include "Graph/GraphAudioValidationFacts.h"

#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

namespace {

using NodeIdSet = std::unordered_set<
        String,
        GraphAudioScopeAnalysis::StringHash>;

bool isLinkedStereoTimeOutput(const Port& port) {
    return port.domain == PortDomain::TimeSignal
            && port.channelLayout == ChannelLayout::LinkedStereo;
}

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message,
        const String& subjectId) {
    GraphValidationIssue issue { code, message };
    issue.subjectId = subjectId;
    issues.push_back(std::move(issue));
}

template<typename EdgeIndex>
NodeIdSet reachableNodes(
        const GraphEdgeView& edges,
        const EdgeIndex& edgeIndex,
        const GraphAudioScopeAnalysis& scopes,
        const String& root,
        bool reverse) {
    NodeIdSet reachable { root };
    std::vector<String> pending { root };
    for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
        InteractionComplexityDiagnostics::recordValidationNodeVisits(1);
        const auto adjacentEdges = reverse
                ? edgeIndex.incomingEdges(pending[cursor])
                : edgeIndex.outgoingEdges(pending[cursor]);
        InteractionComplexityDiagnostics::recordValidationEdgeVisits(
                adjacentEdges.size());
        for (const size_t edgeIndexToVisit : adjacentEdges) {
            const Edge& edge = edges[edgeIndexToVisit];
            if (edge.isAttachment()) {
                continue;
            }
            const String& nodeId = reverse ? edge.sourceNodeId : edge.destNodeId;
            if (scopes.scopeFor(nodeId) == AuthoredAudioScope::Global
                    && reachable.insert(nodeId).second) {
                pending.push_back(nodeId);
            }
        }
    }
    return reachable;
}

template<typename EdgeIndex>
int bypassingOutputsFor(
        const Node& node,
        const GraphEdgeView& edges,
        const EdgeIndex& edgeIndex,
        const GraphAudioScopeAnalysis& scopes,
        const String& voiceOutputId) {
    if (scopes.scopeFor(node.id) != AuthoredAudioScope::Voice) {
        return 0;
    }
    const auto outgoing = edgeIndex.outgoingEdges(node.id);
    InteractionComplexityDiagnostics::recordValidationEdgeVisits(outgoing.size());
    int result = 0;
    for (const auto& output : node.outputs) {
        if (!isLinkedStereoTimeOutput(output)) {
            continue;
        }
        const bool consumed = std::any_of(
                outgoing.begin(),
                outgoing.end(),
                [&](size_t edgeIndexToVisit) {
                    const Edge& edge = edges[edgeIndexToVisit];
                    return !edge.isAttachment()
                            && edge.sourcePortId == output.id
                            && (scopes.scopeFor(edge.destNodeId)
                                        == AuthoredAudioScope::Voice
                                    || edge.destNodeId == voiceOutputId);
                });
        result += consumed ? 0 : 1;
    }
    return result;
}

}

GraphAudioValidationFacts::GraphAudioValidationFacts(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphAudioScopeAnalysis& scopes) {
    const GraphEdgeIndex edgeIndex(edges);
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::VoiceOutput) {
            voiceOutputs.push_back(node.id);
        } else if (node.kind == NodeKind::GlobalInput) {
            globalInputs.push_back(node.id);
        } else if (node.kind == NodeKind::Output) {
            outputs.push_back(node.id);
        }
        if (scopes.scopeFor(node.id) == AuthoredAudioScope::Global) {
            globalNodes.emplace(node.id);
        }
    }
    if (globalInputs.size() == 1 && outputs.size() == 1) {
        reachableFromInput = reachableNodes(
                edges, edgeIndex, scopes, globalInputs.front(), false);
        reachingOutput = reachableNodes(
                edges, edgeIndex, scopes, outputs.front(), true);
    }
    const String voiceOutputId = voiceOutputs.size() == 1
            ? voiceOutputs.front()
            : String {};
    for (const auto& node : graph.getNodes()) {
        InteractionComplexityDiagnostics::recordValidationNodeVisits(1);
        const int count = bypassingOutputsFor(
                node, edges, edgeIndex, scopes, voiceOutputId);
        if (count > 0) {
            bypassingTerminals.emplace(node.id, count);
            bypassingTerminalCount += count;
        }
    }
}

bool GraphAudioValidationFacts::usesExplicitAudioGraph() const {
    return !globalInputs.empty() || !voiceOutputs.empty();
}

void GraphAudioValidationFacts::appendIssues(
        const GraphAudioScopeAnalysis& scopes,
        std::vector<GraphValidationIssue>& issues) const {
    if (!usesExplicitAudioGraph()) {
        return;
    }
    const auto singletonIssue = [&](const std::vector<String>& ids,
                                    const String& label,
                                    const String& subjectId) {
        if (ids.size() != 1) {
            addIssue(
                    issues,
                    ids.empty()
                            ? GraphValidationCode::MissingRequiredNode
                            : GraphValidationCode::DuplicateSingletonNode,
                    "Audio graph requires exactly one " + label,
                    subjectId);
        }
    };
    singletonIssue(voiceOutputs, "Voice Output", "voiceOutput");
    singletonIssue(globalInputs, "Global Input", "globalInput");
    singletonIssue(outputs, "Output", "output");
    for (const String& nodeId : scopes.conflictingNeutralNodeIds) {
        addIssue(
                issues,
                GraphValidationCode::ConflictingProcessingScope,
                "Routing node participates in both voice and global graphs: " + nodeId,
                nodeId);
    }
    if (voiceOutputs.size() != 1 || globalInputs.size() != 1 || outputs.size() != 1) {
        return;
    }
    for (const String& nodeId : globalNodes) {
        if (reachableFromInput.count(nodeId) == 0) {
            addIssue(
                    issues,
                    GraphValidationCode::GlobalNodeUnreachable,
                    "Global node is not reachable from Global Input: " + nodeId,
                    nodeId);
        }
        if (reachingOutput.count(nodeId) == 0) {
            addIssue(
                    issues,
                    GraphValidationCode::GlobalNodeCannotReachOutput,
                    "Global node does not reach Output: " + nodeId,
                    nodeId);
        }
    }
    if (bypassingTerminalCount > 0) {
        addIssue(
                issues,
                GraphValidationCode::AmbiguousVoiceOutput,
                "Voice audio path does not terminate at Voice Output",
                "voiceOutput");
    }
}

}
