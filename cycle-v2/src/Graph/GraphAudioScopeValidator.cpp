#include "Graph/GraphAudioScopeValidator.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidationTypes.h"

namespace CycleV2 {

namespace {

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

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message) {
    issues.push_back({ code, message });
}

}

bool GraphAudioScopeValidator::usesExplicitAudioGraph(const NodeGraph& graph) {
    return std::any_of(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::GlobalInput
                        || node.kind == NodeKind::VoiceOutput;
            });
}

void GraphAudioScopeValidator::validate(
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

}
