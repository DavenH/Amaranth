#include <algorithm>
#include <unordered_set>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

namespace {

using NodeIndex = std::unordered_map<
        String,
        size_t,
        GraphAudioScopeAnalysis::StringHash>;
using NodeIdSet = std::unordered_set<
        String,
        GraphAudioScopeAnalysis::StringHash>;

NodeIndex buildNodeIndex(const NodeGraph& graph) {
    NodeIndex result;
    result.reserve(graph.getNodes().size());
    for (size_t index = 0; index < graph.getNodes().size(); ++index) {
        result.emplace(graph.getNodes()[index].id, index);
    }
    return result;
}

std::vector<std::vector<size_t>> buildSignalAdjacency(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const NodeIndex& nodeIndex) {
    std::vector<std::vector<size_t>> result(graph.getNodes().size());
    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        const Edge& edge = edges[edgeIndex];
        if (edge.isAttachment()) {
            continue;
        }
        const auto source = nodeIndex.find(edge.sourceNodeId);
        const auto destination = nodeIndex.find(edge.destNodeId);
        if (source == nodeIndex.end() || destination == nodeIndex.end()) {
            continue;
        }
        result[source->second].push_back(destination->second);
        result[destination->second].push_back(source->second);
    }
    return result;
}

struct NeutralComponent {
    std::vector<size_t> nodeIndices;
    bool touchesVoice {};
    bool touchesGlobal {};
};

NeutralComponent visitNeutralComponent(
        size_t root,
        const NodeGraph& graph,
        const std::vector<std::vector<size_t>>& adjacency,
        const std::vector<AudioProcessingCapability>& capabilities,
        std::vector<bool>& visited) {
    NeutralComponent result;
    result.nodeIndices.push_back(root);
    visited[root] = true;
    for (size_t cursor = 0; cursor < result.nodeIndices.size(); ++cursor) {
        for (const size_t adjacent : adjacency[result.nodeIndices[cursor]]) {
            if (capabilities[adjacent] == AudioProcessingCapability::DomainNeutral) {
                if (!visited[adjacent]) {
                    visited[adjacent] = true;
                    result.nodeIndices.push_back(adjacent);
                }
                continue;
            }
            const auto scope = GraphAudioScopeAnalyzer::explicitScopeFor(
                    graph.getNodes()[adjacent]);
            result.touchesVoice = result.touchesVoice
                    || scope == AuthoredAudioScope::Voice;
            result.touchesGlobal = result.touchesGlobal
                    || scope == AuthoredAudioScope::Global;
        }
    }
    return result;
}

struct IndexedNeutralComponent {
    std::vector<String> nodeIds;
    bool touchesVoice {};
    bool touchesGlobal {};
};

IndexedNeutralComponent visitNeutralComponent(
        const String& root,
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphEdgeIndexOverlay& edgeIndex,
        NodeIdSet& visited) {
    IndexedNeutralComponent result;
    result.nodeIds.push_back(root);
    visited.insert(root);
    for (size_t cursor = 0; cursor < result.nodeIds.size(); ++cursor) {
        InteractionComplexityDiagnostics::recordValidationNodeVisits(1);
        const String& nodeId = result.nodeIds[cursor];
        const auto visitEdges = [&](const std::vector<size_t>& edgeIndices) {
            InteractionComplexityDiagnostics::recordValidationEdgeVisits(
                    edgeIndices.size());
            for (const size_t edgeIndexToVisit : edgeIndices) {
                const Edge& edge = edges[edgeIndexToVisit];
                if (edge.isAttachment()) {
                    continue;
                }
                const String& adjacentId = edge.sourceNodeId == nodeId
                        ? edge.destNodeId
                        : edge.sourceNodeId;
                const Node* adjacent = graph.findNode(adjacentId);
                if (adjacent == nullptr) {
                    continue;
                }
                if (GraphAudioScopeAnalyzer::capabilityFor(*adjacent)
                        == AudioProcessingCapability::DomainNeutral) {
                    if (visited.insert(adjacentId).second) {
                        result.nodeIds.push_back(adjacentId);
                    }
                    continue;
                }
                const auto scope = GraphAudioScopeAnalyzer::explicitScopeFor(*adjacent);
                result.touchesVoice = result.touchesVoice
                        || scope == AuthoredAudioScope::Voice;
                result.touchesGlobal = result.touchesGlobal
                        || scope == AuthoredAudioScope::Global;
            }
        };
        visitEdges(edgeIndex.incomingEdges(nodeId));
        visitEdges(edgeIndex.outgoingEdges(nodeId));
    }
    return result;
}

AuthoredAudioScope resolvedScope(bool touchesVoice, bool touchesGlobal) {
    return touchesGlobal && !touchesVoice
            ? AuthoredAudioScope::Global
            : AuthoredAudioScope::Voice;
}

void sortConflicts(GraphAudioScopeAnalysis& analysis) {
    std::sort(
            analysis.conflictingNeutralNodeIds.begin(),
            analysis.conflictingNeutralNodeIds.end());
}

}

AuthoredAudioScope GraphAudioScopeAnalysis::scopeFor(const String& nodeId) const {
    const auto found = nodes.find(nodeId);
    return found != nodes.end() ? found->second : AuthoredAudioScope::Voice;
}

bool GraphAudioScopeAnalysis::hasConflict(const String& nodeId) const {
    return std::find(
            conflictingNeutralNodeIds.begin(),
            conflictingNeutralNodeIds.end(),
            nodeId) != conflictingNeutralNodeIds.end();
}

GraphAudioScopeAnalysis GraphAudioScopeAnalyzer::analyze(const NodeGraph& graph) const {
    return analyze(graph, GraphEdgeView(graph.getEdges()));
}

GraphAudioScopeAnalysis GraphAudioScopeAnalyzer::analyze(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    InteractionComplexityDiagnostics::recordValidationNodeVisits(
            graph.getNodes().size());
    InteractionComplexityDiagnostics::recordValidationEdgeVisits(edges.size());
    GraphAudioScopeAnalysis result;
    result.nodes.reserve(graph.getNodes().size());
    std::vector<AudioProcessingCapability> capabilities;
    capabilities.reserve(graph.getNodes().size());
    for (const auto& node : graph.getNodes()) {
        const auto capability = capabilityFor(node);
        capabilities.push_back(capability);
        if (capability != AudioProcessingCapability::DomainNeutral) {
            result.nodes.emplace(node.id, explicitScopeFor(node));
        }
    }

    const auto adjacency = buildSignalAdjacency(graph, edges, buildNodeIndex(graph));
    std::vector<bool> visited(graph.getNodes().size());
    for (size_t index = 0; index < graph.getNodes().size(); ++index) {
        if (visited[index]
                || capabilities[index] != AudioProcessingCapability::DomainNeutral) {
            continue;
        }
        const auto component = visitNeutralComponent(
                index,
                graph,
                adjacency,
                capabilities,
                visited);
        const auto scope = resolvedScope(
                component.touchesVoice,
                component.touchesGlobal);
        for (const size_t nodeIndex : component.nodeIndices) {
            const String& nodeId = graph.getNodes()[nodeIndex].id;
            result.nodes.emplace(nodeId, scope);
            if (component.touchesVoice && component.touchesGlobal) {
                result.conflictingNeutralNodeIds.push_back(nodeId);
            }
        }
    }
    sortConflicts(result);
    return result;
}

GraphAudioScopeAnalysis GraphAudioScopeAnalyzer::analyze(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphEdgeIndexOverlay& edgeIndex,
        const GraphAudioScopeAnalysis& baseline) const {
    GraphAudioScopeAnalysis result = baseline;
    std::vector<String> roots;
    NodeIdSet changedNodes;
    const auto appendChangedNode = [&](const String& nodeId) {
        if (!changedNodes.insert(nodeId).second) {
            return;
        }
        InteractionComplexityDiagnostics::recordValidationNodeVisits(1);
        const Node* node = graph.findNode(nodeId);
        if (node != nullptr
                && capabilityFor(*node) == AudioProcessingCapability::DomainNeutral) {
            roots.push_back(nodeId);
        }
    };
    const auto appendChangedEdge = [&](const Edge& edge) {
        if (!edge.isAttachment()) {
            appendChangedNode(edge.sourceNodeId);
            appendChangedNode(edge.destNodeId);
        }
    };
    for (const size_t removedIndex : edges.removedIndices()) {
        appendChangedEdge(edges.existingEdge(removedIndex));
    }
    for (const Edge& edge : edges.addedEdges()) {
        appendChangedEdge(edge);
    }

    NodeIdSet affected;
    std::vector<String> affectedConflicts;
    for (const String& root : roots) {
        if (affected.count(root) > 0) {
            continue;
        }
        const auto component = visitNeutralComponent(
                root,
                graph,
                edges,
                edgeIndex,
                affected);
        const auto scope = resolvedScope(
                component.touchesVoice,
                component.touchesGlobal);
        for (const String& nodeId : component.nodeIds) {
            result.nodes[nodeId] = scope;
            if (component.touchesVoice && component.touchesGlobal) {
                affectedConflicts.push_back(nodeId);
            }
        }
    }

    result.conflictingNeutralNodeIds.erase(
            std::remove_if(
                    result.conflictingNeutralNodeIds.begin(),
                    result.conflictingNeutralNodeIds.end(),
                    [&](const String& nodeId) {
                        return affected.count(nodeId) > 0;
                    }),
            result.conflictingNeutralNodeIds.end());
    result.conflictingNeutralNodeIds.insert(
            result.conflictingNeutralNodeIds.end(),
            affectedConflicts.begin(),
            affectedConflicts.end());
    sortConflicts(result);
    return result;
}

AudioProcessingCapability GraphAudioScopeAnalyzer::capabilityFor(const Node& node) {
    const auto* definition = NodeDefinitionRegistry::instance().find(node.kind);
    return definition != nullptr
            ? definition->processingCapability
            : AudioProcessingCapability::VoiceOnly;
}

AuthoredAudioScope GraphAudioScopeAnalyzer::explicitScopeFor(const Node& node) {
    const auto capability = capabilityFor(node);
    if (capability == AudioProcessingCapability::GlobalOnly) {
        return AuthoredAudioScope::Global;
    }
    if (capability == AudioProcessingCapability::Selectable
            && NodeParameterMap(node).stringValue("processingScope", "voice") == "global") {
        return AuthoredAudioScope::Global;
    }
    return AuthoredAudioScope::Voice;
}

}
