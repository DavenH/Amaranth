#pragma once

#include "NodeGraph.h"

#include <vector>

namespace CycleV2 {

class GraphDomainResolver {
public:
    PortDomain domainFromVoiceContext(const Node& voiceNode) const;
    PortDomain resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const;
    PortDomain resolvedDomainForEdge(
            const NodeGraph& graph,
            const Edge& edge,
            const std::vector<Edge>& resolvedEdges) const;
    ChannelLayout resolvedChannelLayoutForEdge(const NodeGraph& graph, const Edge& edge) const;
    std::vector<Edge> resolveSignalEdges(
            const NodeGraph& graph,
            const std::vector<String>& nodeOrder) const;

    static bool isConcreteOperationDomain(PortDomain domain);
    static bool isConcreteSignalDomain(PortDomain domain);
    static bool isContextResolvedSource(const Node& node, const Port& port);

private:
    const Node* findNode(const NodeGraph& graph, const String& id) const;
    const Port* findPort(const Node& node, const String& id, bool input) const;
    PortDomain domainFromContextInput(const NodeGraph& graph, const Node& node) const;
    PortDomain firstResolvedInputDomain(const NodeGraph& graph, const String& nodeId, int depth) const;
    PortDomain firstResolvedSignalInputDomain(const NodeGraph& graph, const String& nodeId, int depth) const;
    PortDomain firstResolvedInputDomain(const std::vector<Edge>& resolvedEdges, const String& nodeId) const;
    PortDomain resolvedControlOutputDomain(
            const NodeGraph& graph,
            const Node& sourceNode,
            const Port& sourcePort,
            const std::vector<Edge>& resolvedEdges) const;
    PortDomain resolvedEdgeDomain(const NodeGraph& graph, const Edge& edge, int depth) const;
};

}
