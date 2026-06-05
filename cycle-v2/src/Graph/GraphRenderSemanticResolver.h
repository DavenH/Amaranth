#pragma once

#include "GraphDomainResolver.h"

namespace CycleV2 {

enum class RenderScalePolicy {
    Unipolar,
    Bipolar
};

enum class RenderSemanticRole {
    Generic,
    TimeWaveform,
    SpectralMagnitudeAdditive,
    SpectralMagnitudeMultiplicative,
    SpectralPhase,
    EnvelopeUnipolar,
    EnvelopeBipolar
};

struct NodeRenderSemantic {
    PortDomain domain { PortDomain::ControlSignal };
    RenderScalePolicy scalePolicy { RenderScalePolicy::Unipolar };
    RenderSemanticRole role { RenderSemanticRole::Generic };
};

class GraphRenderSemanticResolver {
public:
    NodeRenderSemantic semanticForNodeOutput(
            const NodeGraph& graph,
            const String& nodeId,
            const String& portId) const;

private:
    GraphDomainResolver domainResolver;

    const Node* findNode(const NodeGraph& graph, const String& id) const;
    PortDomain contextDomainForNode(const NodeGraph& graph, const Node& node) const;
    NodeRenderSemantic semanticForEdge(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain domain) const;
    NodeRenderSemantic defaultSemanticForDomain(PortDomain domain) const;
};

}
