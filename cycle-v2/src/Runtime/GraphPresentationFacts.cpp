#include "Runtime/GraphPresentationFacts.h"

namespace CycleV2 {

namespace {

template<typename Item, typename Index, typename Id>
void indexItems(
        const std::vector<Item>& items,
        Index& index,
        Id id) {
    index.reserve(items.size());
    for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
        index.emplace(id(items[itemIndex]), itemIndex);
    }
}

}

GraphPresentationFacts::Structure::Structure(const NodeGraph& graph) :
        indexedEdges(graph.getEdges())
    ,   resolvedDomains(GraphDomainResolver().resolve(graph))
    ,   audioScopes(GraphAudioScopeAnalyzer().analyze(graph)) {
    for (const auto& edge : graph.getEdges()) {
        attachments += edge.isAttachment() ? 1 : 0;
    }
}

GraphPresentationFacts::GraphPresentationFacts(
        const NodeGraph& graph,
        const GraphPresentationSnapshot& snapshot,
        const GraphPresentationFacts* previous) :
        structure(previous != nullptr
                ? previous->structure
                : std::make_shared<const Structure>(graph)) {
    indexItems(snapshot.previewResult.nodes, previewIndices, [](const auto& preview) {
        return preview.nodeId;
    });
    indexItems(snapshot.runtimeTrace.nodes, runtimeTraceIndices, [](const auto& trace) {
        return trace.nodeId;
    });
    indexItems(snapshot.previewResult.probes, probePreviewIndices, [](const auto& preview) {
        return preview.probeId;
    });
    indexItems(snapshot.compileResult.plan.nodeOrder, executionIndices, [](const auto& nodeId) {
        return nodeId;
    });
}

const NodePreviewResult* GraphPresentationFacts::previewFor(
        const GraphPresentationSnapshot& snapshot,
        const String& nodeId) const {
    const auto found = previewIndices.find(nodeId);
    return found == previewIndices.end()
            ? nullptr
            : &snapshot.previewResult.nodes[found->second];
}

const RuntimeNodeTrace* GraphPresentationFacts::runtimeTraceFor(
        const GraphPresentationSnapshot& snapshot,
        const String& nodeId) const {
    const auto found = runtimeTraceIndices.find(nodeId);
    return found == runtimeTraceIndices.end()
            ? nullptr
            : &snapshot.runtimeTrace.nodes[found->second];
}

const GraphPreviewResult::SignalProbePreview* GraphPresentationFacts::probePreviewFor(
        const GraphPresentationSnapshot& snapshot,
        const String& probeId) const {
    const auto found = probePreviewIndices.find(probeId);
    return found == probePreviewIndices.end()
            ? nullptr
            : &snapshot.previewResult.probes[found->second];
}

int GraphPresentationFacts::executionIndexFor(const String& nodeId) const {
    const auto found = executionIndices.find(nodeId);
    return found == executionIndices.end() ? -1 : (int) found->second;
}

PortDomain GraphPresentationFacts::domainForEdge(
        const NodeGraph& graph,
        const Edge& edge) const {
    if (edge.isAttachment()) {
        return edge.domain;
    }
    for (const size_t edgeIndex : structure->indexedEdges.outgoingEdges(
            edge.sourceNodeId)) {
        const Edge& candidate = graph.getEdges()[edgeIndex];
        if (candidate.sourcePortId == edge.sourcePortId
                && candidate.destNodeId == edge.destNodeId
                && candidate.destPortId == edge.destPortId
                && candidate.connectionKind == edge.connectionKind
                && candidate.attachmentType == edge.attachmentType) {
            return structure->resolvedDomains.domains[edgeIndex];
        }
    }
    return GraphDomainResolver().resolvedDomainForEdge(graph, edge);
}

NodeRenderSemantic GraphPresentationFacts::renderSemanticForNodeOutput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId) const {
    return GraphRenderSemanticResolver().semanticForNodeOutput(
            graph,
            nodeId,
            portId,
            structure->indexedEdges,
            structure->resolvedDomains);
}

}
