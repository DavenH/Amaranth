#pragma once

#include <unordered_map>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphRenderSemanticResolver.h"
#include "Runtime/GraphPresentationSnapshot.h"

namespace CycleV2 {

class GraphPresentationFacts {
public:
    GraphPresentationFacts(
            const NodeGraph& graph,
            const GraphPresentationSnapshot& snapshot,
            const GraphPresentationFacts* previous = nullptr);

    const NodePreviewResult* previewFor(
            const GraphPresentationSnapshot& snapshot,
            const String& nodeId) const;
    const RuntimeNodeTrace* runtimeTraceFor(
            const GraphPresentationSnapshot& snapshot,
            const String& nodeId) const;
    const GraphPreviewResult::SignalProbePreview* probePreviewFor(
            const GraphPresentationSnapshot& snapshot,
            const String& probeId) const;
    int executionIndexFor(const String& nodeId) const;
    PortDomain domainForEdge(const NodeGraph& graph, const Edge& edge) const;
    NodeRenderSemantic renderSemanticForNodeOutput(
            const NodeGraph& graph,
            const String& nodeId,
            const String& portId) const;

    const GraphEdgeIndex& edgeIndex() const { return structure->indexedEdges; }
    const GraphDomainResolution& domainResolution() const {
        return structure->resolvedDomains;
    }
    const GraphAudioScopeAnalysis& audioScopeAnalysis() const {
        return structure->audioScopes;
    }
    int attachmentCount() const { return structure->attachments; }

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    using Index = std::unordered_map<String, size_t, StringHash>;

    struct Structure {
        explicit Structure(const NodeGraph& graph);

        GraphEdgeIndex indexedEdges;
        GraphDomainResolution resolvedDomains;
        GraphAudioScopeAnalysis audioScopes;
        int attachments {};
    };

    std::shared_ptr<const Structure> structure;
    Index previewIndices;
    Index runtimeTraceIndices;
    Index probePreviewIndices;
    Index executionIndices;
};

}
