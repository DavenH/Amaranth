#include <algorithm>

#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/TrimeshSignalSemantics.h"

namespace CycleV2 {

const Node* GraphRenderSemanticResolver::findNode(const NodeGraph& graph, const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

bool GraphRenderSemanticResolver::isBipolarMagnitudeSource(
        const NodeGraph& graph,
        const String& nodeId) const {
    String currentId = nodeId;
    StringArray visited;
    while (currentId.isNotEmpty() && !visited.contains(currentId)) {
        visited.add(currentId);
        const Node* node = findNode(graph, currentId);
        if (node == nullptr) {
            return false;
        }
        if (node->kind == NodeKind::TrilinearMesh) {
            return TrimeshSignalSemantics::isBipolar(*node);
        }
        if (node->kind != NodeKind::SpectralLayer) {
            return false;
        }

        const auto input = std::find_if(
                graph.getEdges().begin(),
                graph.getEdges().end(),
                [&](const Edge& edge) {
                    return !edge.isAttachment()
                            && edge.destNodeId == currentId
                            && edge.destPortId == "in";
                });
        if (input == graph.getEdges().end()) {
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
    NodeRenderSemantic semantic;
    bool foundSignalEdge {};
    const GraphDomainResolution resolution = domainResolver.resolve(graph);

    for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
        const Edge& edge = graph.getEdges()[edgeIndex];
        if (edge.isAttachment() || edge.sourceNodeId != nodeId || edge.sourcePortId != portId) {
            continue;
        }

        const PortDomain domain = resolution.domains[edgeIndex];
        const NodeRenderSemantic edgeSemantic = semanticForEdge(graph, edge, domain);

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

    if (const Node* node = findNode(graph, nodeId)) {
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
        PortDomain domain) const {
    NodeRenderSemantic semantic = defaultSemanticForDomain(domain);
    const Node* destNode = findNode(graph, edge.destNodeId);

    if (domain == PortDomain::SpectralMagnitudeSignal) {
        if (isBipolarMagnitudeSource(graph, edge.sourceNodeId)) {
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
