#include "GraphRenderSemanticResolver.h"

namespace CycleV2 {

const Node* GraphRenderSemanticResolver::findNode(const NodeGraph& graph, const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

PortDomain GraphRenderSemanticResolver::contextDomainForNode(
        const NodeGraph& graph,
        const Node& node) const {
    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment || edge.destNodeId != node.id || edge.destPortId != "context") {
            continue;
        }

        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        if (sourceNode != nullptr && sourceNode->kind == NodeKind::VoiceContext) {
            return domainResolver.domainFromVoiceContext(*sourceNode);
        }
    }

    return PortDomain::ControlSignal;
}

NodeRenderSemantic GraphRenderSemanticResolver::semanticForNodeOutput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId) const {
    NodeRenderSemantic semantic;
    bool foundSignalEdge {};

    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment || edge.sourceNodeId != nodeId || edge.sourcePortId != portId) {
            continue;
        }

        const PortDomain domain = domainResolver.resolvedDomainForEdge(graph, edge);
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
            const PortDomain contextDomain = contextDomainForNode(graph, *node);
            if (contextDomain != PortDomain::ControlSignal) {
                return defaultSemanticForDomain(contextDomain);
            }
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

    if (domain == PortDomain::SpectralMagnitudeSignal && destNode != nullptr) {
        if (destNode->kind == NodeKind::Multiply) {
            semantic.scalePolicy = RenderScalePolicy::Bipolar;
            semantic.role = RenderSemanticRole::SpectralMagnitudeMultiplicative;
        } else if (destNode->kind == NodeKind::Add) {
            semantic.scalePolicy = RenderScalePolicy::Unipolar;
            semantic.role = RenderSemanticRole::SpectralMagnitudeAdditive;
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
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::SpectralMagnitudeAdditive };

        case PortDomain::SpectralPhaseSignal:
            return { domain, RenderScalePolicy::Bipolar, RenderSemanticRole::SpectralPhase };

        case PortDomain::EnvelopeSignal:
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::EnvelopeUnipolar };

        default:
            return { domain, RenderScalePolicy::Unipolar, RenderSemanticRole::Generic };
    }
}

}
