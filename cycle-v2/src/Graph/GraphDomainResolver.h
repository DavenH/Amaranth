#pragma once

#include "NodeGraph.h"

#include <vector>

namespace CycleV2 {

struct GraphDomainResolution {
    std::vector<PortDomain> domains;
    std::vector<ChannelLayout> channelLayouts;
};

class GraphDomainResolver {
public:
    PortDomain domainFromVoiceContext(const Node& voiceNode) const;
    GraphDomainResolution resolve(const NodeGraph& graph) const;
    PortDomain resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const;
    PortDomain resolvedDomainForEdge(
            const NodeGraph& graph,
            const Edge& edge,
            const std::vector<Edge>& resolvedEdges) const;
    ChannelLayout resolvedChannelLayoutForEdge(const NodeGraph& graph, const Edge& edge) const;
    ChannelLayout resolvedChannelLayoutForEdge(
            const NodeGraph& graph,
            const Edge& edge,
            const GraphDomainResolution& resolution) const;
    std::vector<Edge> resolveSignalEdges(
            const NodeGraph& graph,
            const std::vector<String>& nodeOrder) const;
    std::vector<Edge> resolveSignalEdges(
            const NodeGraph& graph,
            const std::vector<String>& nodeOrder,
            const GraphDomainResolution& resolution) const;

    static bool isConcreteOperationDomain(PortDomain domain);
    static bool isConcreteSignalDomain(PortDomain domain);
    static bool isContextResolvedSource(const Node& node, const Port& port);

};

}
