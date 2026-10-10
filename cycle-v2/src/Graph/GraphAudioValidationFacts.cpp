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

template<typename EdgeIndex>
NodeIdSet directedClosure(
        const GraphEdgeView& edges,
        const EdgeIndex& edgeIndex,
        const GraphAudioScopeAnalysis& baselineScopes,
        const GraphAudioScopeAnalysis& scopes,
        const std::vector<String>& seeds,
        bool reverse) {
    NodeIdSet closure;
    std::vector<String> pending;
    for (const String& seed : seeds) {
        if (closure.insert(seed).second) {
            pending.push_back(seed);
        }
    }
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
            const bool participates = baselineScopes.scopeFor(nodeId)
                            == AuthoredAudioScope::Global
                    || scopes.scopeFor(nodeId) == AuthoredAudioScope::Global;
            if (participates && closure.insert(nodeId).second) {
                pending.push_back(nodeId);
            }
        }
    }
    return closure;
}

template<typename EdgeIndex>
void updateReachability(
        NodeIdSet& reachable,
        const NodeIdSet& closure,
        const GraphEdgeView& edges,
        const EdgeIndex& edgeIndex,
        const GraphAudioScopeAnalysis& scopes,
        const String& root,
        bool reverse) {
    for (const String& nodeId : closure) {
        reachable.erase(nodeId);
    }

    std::vector<String> pending;
    for (const String& nodeId : closure) {
        if (scopes.scopeFor(nodeId) != AuthoredAudioScope::Global) {
            continue;
        }
        bool hasReachableBoundary = nodeId == root;
        const auto boundaryEdges = reverse
                ? edgeIndex.outgoingEdges(nodeId)
                : edgeIndex.incomingEdges(nodeId);
        InteractionComplexityDiagnostics::recordValidationEdgeVisits(
                boundaryEdges.size());
        for (const size_t edgeIndexToVisit : boundaryEdges) {
            const Edge& edge = edges[edgeIndexToVisit];
            if (edge.isAttachment()) {
                continue;
            }
            const String& adjacent = reverse ? edge.destNodeId : edge.sourceNodeId;
            if (closure.count(adjacent) == 0 && reachable.count(adjacent) > 0) {
                hasReachableBoundary = true;
                break;
            }
        }
        if (hasReachableBoundary && reachable.insert(nodeId).second) {
            pending.push_back(nodeId);
        }
    }

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
            if (closure.count(nodeId) > 0
                    && scopes.scopeFor(nodeId) == AuthoredAudioScope::Global
                    && reachable.insert(nodeId).second) {
                pending.push_back(nodeId);
            }
        }
    }
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

GraphAudioValidationFacts::GraphAudioValidationFacts(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphEdgeIndexOverlay& edgeIndex,
        const GraphAudioScopeAnalysis& baselineScopes,
        const GraphAudioScopeAnalysis& scopes,
        const GraphAudioValidationFacts& baseline) :
        globalInputs(baseline.globalInputs)
    ,   voiceOutputs(baseline.voiceOutputs)
    ,   outputs(baseline.outputs)
    ,   globalNodes(baseline.globalNodes)
    ,   reachableFromInput(baseline.reachableFromInput)
    ,   reachingOutput(baseline.reachingOutput)
    ,   bypassingTerminals(baseline.bypassingTerminals)
    ,   bypassingTerminalCount(baseline.bypassingTerminalCount) {
    std::vector<String> forwardSeeds;
    std::vector<String> reverseSeeds;
    NodeIdSet terminalSources;
    const auto appendChangedEdge = [&](const Edge& edge) {
        if (edge.isAttachment()) {
            return;
        }
        forwardSeeds.push_back(edge.destNodeId);
        reverseSeeds.push_back(edge.sourceNodeId);
        terminalSources.emplace(edge.sourceNodeId);
    };
    for (const size_t removedIndex : edges.removedIndices()) {
        appendChangedEdge(edges.existingEdge(removedIndex));
    }
    for (const Edge& edge : edges.addedEdges()) {
        appendChangedEdge(edge);
    }
    for (const String& nodeId : scopes.affectedNodeIds) {
        forwardSeeds.push_back(nodeId);
        reverseSeeds.push_back(nodeId);
        terminalSources.emplace(nodeId);
        if (scopes.scopeFor(nodeId) == AuthoredAudioScope::Global) {
            globalNodes.emplace(nodeId);
        } else {
            globalNodes.erase(nodeId);
        }
        for (const size_t incoming : edgeIndex.incomingEdges(nodeId)) {
            terminalSources.emplace(edges[incoming].sourceNodeId);
        }
    }

    if (globalInputs.size() == 1 && outputs.size() == 1) {
        const auto forward = directedClosure(
                edges, edgeIndex, baselineScopes, scopes, forwardSeeds, false);
        const auto reverse = directedClosure(
                edges, edgeIndex, baselineScopes, scopes, reverseSeeds, true);
        updateReachability(
                reachableFromInput,
                forward,
                edges,
                edgeIndex,
                scopes,
                globalInputs.front(),
                false);
        updateReachability(
                reachingOutput,
                reverse,
                edges,
                edgeIndex,
                scopes,
                outputs.front(),
                true);
    }

    const String voiceOutputId = voiceOutputs.size() == 1
            ? voiceOutputs.front()
            : String {};
    for (const String& nodeId : terminalSources) {
        InteractionComplexityDiagnostics::recordValidationNodeVisits(1);
        const auto previous = bypassingTerminals.find(nodeId);
        if (previous != bypassingTerminals.end()) {
            bypassingTerminalCount -= previous->second;
            bypassingTerminals.erase(previous);
        }
        const Node* node = graph.findNode(nodeId);
        if (node == nullptr) {
            continue;
        }
        const int count = bypassingOutputsFor(
                *node, edges, edgeIndex, scopes, voiceOutputId);
        if (count > 0) {
            bypassingTerminals.emplace(nodeId, count);
            bypassingTerminalCount += count;
        }
    }
}

bool GraphAudioValidationFacts::usesExplicitAudioGraph() const {
    return !globalInputs.empty() || !voiceOutputs.empty();
}

bool GraphAudioValidationFacts::usesExplicitAudioGraph(const NodeGraph& graph) {
    return std::any_of(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::GlobalInput
                        || node.kind == NodeKind::VoiceOutput;
            });
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
