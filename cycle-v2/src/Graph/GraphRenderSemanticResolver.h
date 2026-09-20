#pragma once

#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"

namespace CycleV2 {

enum class RenderScalePolicy {
    Unipolar,
    Bipolar
};

enum class RenderSemanticRole {
    Generic,
    TimeWaveform,
    SpectralMagnitudeUnipolar,
    SpectralMagnitudeBipolar,
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
    NodeRenderSemantic semanticForNodeOutput(
            const NodeGraph& graph,
            const String& nodeId,
            const String& portId,
            const GraphEdgeIndex& edgeIndex,
            const GraphDomainResolution& resolution) const;

private:
    GraphDomainResolver domainResolver;

    bool isBipolarMagnitudeSource(
            const NodeGraph& graph,
            const String& nodeId,
            const GraphEdgeIndex* edgeIndex = nullptr) const;
    NodeRenderSemantic semanticForEdge(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain domain,
            const GraphEdgeIndex* edgeIndex = nullptr) const;
    NodeRenderSemantic defaultSemanticForDomain(PortDomain domain) const;
};

}
