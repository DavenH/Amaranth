#include <algorithm>

#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/TrimeshSignalSemantics.h"

namespace CycleV2 {

bool GraphRenderSemanticResolver::isBipolarMagnitudeSource(
        const NodeGraph& graph,
        const String& nodeId,
        const GraphEdgeIndex* edgeIndex) const {
    String currentId = nodeId;
    StringArray visited;
    while (currentId.isNotEmpty() && !visited.contains(currentId)) {
        visited.add(currentId);
        const Node* node = graph.findNode(currentId);
        if (node == nullptr) {
            return false;
        }
        if (node->kind == NodeKind::TrilinearMesh) {
            return TrimeshSignalSemantics::isBipolar(*node);
        }
        if (node->kind != NodeKind::SpectralLayer) {
            return false;
        }

        const Edge* input = nullptr;
        if (edgeIndex != nullptr) {
            for (const size_t candidate : edgeIndex->edgesToInput(currentId, "in")) {
                const Edge& edge = graph.getEdges()[candidate];
                if (!edge.isAttachment()) {
                    input = &edge;
                    break;
                }
            }
        } else {
            const auto found = std::find_if(
                    graph.getEdges().begin(),
                    graph.getEdges().end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment()
                                && edge.destNodeId == currentId
                                && edge.destPortId == "in";
                    });
            if (found != graph.getEdges().end()) {
                input = &*found;
            }
        }
        if (input == nullptr) {
            return false;
        }
        currentId = input->sourceNodeId;
    }

    return false;
}

NodeRenderSemantic GraphRenderSemanticResolver::semanticForNodeOutput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId) const {
    const GraphDomainResolution resolution = domainResolver.resolve(graph);
    const GraphEdgeIndex edgeIndex(graph.getEdges());
    return semanticForNodeOutput(graph, nodeId, portId, edgeIndex, resolution);
}

NodeRenderSemantic GraphRenderSemanticResolver::semanticForNodeOutput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId,
        const GraphEdgeIndex& edgeIndex,
        const GraphDomainResolution& resolution) const {
    NodeRenderSemantic semantic;
    bool foundSignalEdge {};

    for (const size_t edgeIndexToVisit : edgeIndex.outgoingEdges(nodeId)) {
        const Edge& edge = graph.getEdges()[edgeIndexToVisit];
        if (edge.isAttachment() || edge.sourcePortId != portId) {
            continue;
        }

        const PortDomain domain = resolution.domains[edgeIndexToVisit];
        const NodeRenderSemantic edgeSemantic = semanticForEdge(
                graph, edge, domain, &edgeIndex);

        if (!foundSignalEdge || edgeSemantic.role != RenderSemanticRole::Generic) {
            semantic = edgeSemantic;
        }

        foundSignalEdge = true;

        if (edgeSemantic.scalePolicy == RenderScalePolicy::Bipolar) {
            return edgeSemantic;
        }
    }

    if (foundSignalEdge) {
        return semantic;
    }

    if (const Node* node = graph.findNode(nodeId)) {
        if (node->kind == NodeKind::TrilinearMesh && portId == "out") {
            NodeRenderSemantic semantic = defaultSemanticForDomain(
                    TrimeshSignalSemantics::domain(*node));
            if (semantic.domain == PortDomain::SpectralMagnitudeSignal
                    && TrimeshSignalSemantics::isBipolar(*node)) {
                semantic.scalePolicy = RenderScalePolicy::Bipolar;
                semantic.role = RenderSemanticRole::SpectralMagnitudeBipolar;
            }
            return semantic;
        }

        for (const auto& port : node->outputs) {
            if (port.id == portId) {
                return defaultSemanticForDomain(port.domain);
            }
        }
    }

    return semantic;
}

NodeRenderSemantic GraphRenderSemanticResolver::semanticForEdge(
        const NodeGraph& graph,
        const Edge& edge,
        PortDomain domain,
        const GraphEdgeIndex* edgeIndex) const {
    NodeRenderSemantic semantic = defaultSemanticForDomain(domain);
    const Node* destNode = graph.findNode(edge.destNodeId);

    if (domain == PortDomain::SpectralMagnitudeSignal) {
        if (isBipolarMagnitudeSource(graph, edge.sourceNodeId, edgeIndex)) {
            semantic.scalePolicy = RenderScalePolicy::Bipolar;
            semantic.role = RenderSemanticRole::SpectralMagnitudeBipolar;
        }
    }

    if (domain == PortDomain::EnvelopeSignal && destNode != nullptr) {
        if (edge.destPortId.containsIgnoreCase("pitch")) {
            semantic.scalePolicy = RenderScalePolicy::Bipolar;
            semantic.role = RenderSemanticRole::EnvelopeBipolar;
        }
    }

    return semantic;
}

NodeRenderSemantic GraphRenderSemanticResolver::defaultSemanticForDomain(PortDomain domain) const {
    switch (domain) {
        case PortDomain::TimeSignal:
            return { domain, RenderScalePolicy::Bipolar, RenderSemanticRole::TimeWaveform };

        case PortDomain::SpectralMagnitudeSignal:
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::SpectralMagnitudeUnipolar };

        case PortDomain::SpectralPhaseSignal:
            return { domain, RenderScalePolicy::Bipolar, RenderSemanticRole::SpectralPhase };

        case PortDomain::EnvelopeSignal:
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::EnvelopeUnipolar };

        default:
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::Generic };
    }
}

}
