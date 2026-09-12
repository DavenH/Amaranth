#include "Graph/GraphAudioScope.h"

#include <algorithm>

#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

namespace {

using NodeIndex = std::unordered_map<
        String,
        size_t,
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
        const NodeIndex& nodeIndex) {
    std::vector<std::vector<size_t>> result(graph.getNodes().size());
    for (const auto& edge : graph.getEdges()) {
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

    const auto adjacency = buildSignalAdjacency(graph, buildNodeIndex(graph));
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
        const auto scope = component.touchesGlobal && !component.touchesVoice
                ? AuthoredAudioScope::Global
                : AuthoredAudioScope::Voice;
        for (const size_t nodeIndex : component.nodeIndices) {
            const String& nodeId = graph.getNodes()[nodeIndex].id;
            result.nodes.emplace(nodeId, scope);
            if (component.touchesVoice && component.touchesGlobal) {
                result.conflictingNeutralNodeIds.push_back(nodeId);
            }
        }
    }
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
