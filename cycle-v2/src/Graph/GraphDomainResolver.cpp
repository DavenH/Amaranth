#include "Graph/GraphDomainResolver.h"

#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"

#include <algorithm>
#include <deque>
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/TrimeshSignalSemantics.h"

namespace CycleV2 {

namespace {

bool propagatesUniversalDomain(NodeKind kind) {
    return kind == NodeKind::Add
            || kind == NodeKind::Multiply
            || kind == NodeKind::SpectralLayer;
}

const Port* findPort(const Node& node, const String& id, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

class ResolutionWorklist {
public:
    ResolutionWorklist(
            const NodeGraph& graphToResolve,
            const GraphEdgeView& edgesToResolve,
            const GraphEdgeIndex& edgeIndexToUse) :
            graph(graphToResolve)
        ,   edges(edgesToResolve)
        ,   indexedEdges(edgeIndexToUse) {
        InteractionComplexityDiagnostics::recordValidationNodeVisits(
                graph.getNodes().size());
        resolution.domains.reserve(edges.size());
        resolution.channelLayouts.resize(edges.size(), ChannelLayout::Mono);

        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const Edge& edge = edges[edgeIndex];
            resolution.domains.push_back(edge.domain);
        }
    }

    GraphDomainResolution run() {
        resolveDomains();
        resolveChannelLayouts();
        return std::move(resolution);
    }

private:
    const Node* node(const String& id) const {
        return graph.findNode(id);
    }

    PortDomain firstInputDomain(
            const Node& nodeToResolve,
            bool operationDomain) const {
        for (const size_t edgeIndex : indexedEdges.incomingEdges(nodeToResolve.id)) {
            if (edges[edgeIndex].isAttachment()) {
                continue;
            }
            const PortDomain domain = resolution.domains[edgeIndex];
            const bool accepted = operationDomain
                    ? GraphDomainResolver::isConcreteOperationDomain(domain)
                    : GraphDomainResolver::isConcreteSignalDomain(domain);
            if (accepted) {
                return domain;
            }
        }

        return PortDomain::ControlSignal;
    }

    PortDomain transferDomain(size_t edgeIndex) const {
        const Edge& edge = edges[edgeIndex];
        if (edge.isAttachment()) {
            return edge.domain;
        }

        const Node* sourceNode = node(edge.sourceNodeId);
        const Node* destNode = node(edge.destNodeId);
        if (sourceNode == nullptr || destNode == nullptr) {
            return edge.domain;
        }

        const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* dest = findPort(*destNode, edge.destPortId, true);
        if (source == nullptr || dest == nullptr) {
            return edge.domain;
        }

        PortDomain sourceDomain = source->domain;
        if (sourceDomain == PortDomain::ControlSignal
                && GraphDomainResolver::isContextResolvedSource(*sourceNode, *source)) {
            sourceDomain = TrimeshSignalSemantics::domain(*sourceNode);
        }
        if (sourceDomain == PortDomain::ControlSignal
                && propagatesUniversalDomain(sourceNode->kind)) {
            sourceDomain = firstInputDomain(*sourceNode, true);
        }
        if (sourceDomain != PortDomain::ControlSignal) {
            return sourceDomain;
        }

        if (dest->domain != PortDomain::ControlSignal) {
            return dest->domain;
        }
        if (propagatesUniversalDomain(destNode->kind)) {
            const PortDomain operationDomain = firstInputDomain(*destNode, true);
            if (operationDomain != PortDomain::ControlSignal) {
                return operationDomain;
            }
        }
        return edge.domain;
    }

    ChannelLayout transferChannelLayout(size_t edgeIndex) const {
        const Edge& edge = edges[edgeIndex];
        if (edge.isAttachment()) {
            return ChannelLayout::Mono;
        }

        const Node* sourceNode = node(edge.sourceNodeId);
        const Node* destNode = node(edge.destNodeId);
        if (sourceNode == nullptr || destNode == nullptr) {
            return ChannelLayout::Mono;
        }

        const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* dest = findPort(*destNode, edge.destPortId, true);
        if (source == nullptr || dest == nullptr) {
            return ChannelLayout::Mono;
        }

        const PortDomain domain = resolution.domains[edgeIndex];
        if (domain == PortDomain::SpectralMagnitudeSignal
                || domain == PortDomain::SpectralPhaseSignal) {
            return ChannelLayout::StereoPair;
        }

        if (dest->domain != PortDomain::ControlSignal) {
            return dest->channelLayout;
        }

        return source->channelLayout;
    }

    template<typename Transfer, typename Value>
    void propagate(
            std::vector<Value>& values,
            Transfer transfer) {
        std::deque<size_t> worklist;
        std::vector<bool> queued(edges.size(), false);
        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            if (!edges[edgeIndex].isAttachment()) {
                worklist.push_back(edgeIndex);
                queued[edgeIndex] = true;
            }
        }

        const size_t maximumTransfers = std::max<size_t>(
                1u,
                edges.size() * edges.size() * 8);
        size_t transfers = 0;
        while (!worklist.empty() && transfers++ < maximumTransfers) {
            InteractionComplexityDiagnostics::recordDomainTransfer();
            const size_t edgeIndex = worklist.front();
            worklist.pop_front();
            queued[edgeIndex] = false;

            const Value next = transfer(edgeIndex);
            if (values[edgeIndex] == next) {
                continue;
            }
            values[edgeIndex] = next;

            const String& affectedNodeId = edges[edgeIndex].destNodeId;
            for (const size_t affected : indexedEdges.incomingEdges(affectedNodeId)) {
                if (!edges[affected].isAttachment() && !queued[affected]) {
                    worklist.push_back(affected);
                    queued[affected] = true;
                }
            }
            for (const size_t affected : indexedEdges.outgoingEdges(affectedNodeId)) {
                if (!edges[affected].isAttachment() && !queued[affected]) {
                    worklist.push_back(affected);
                    queued[affected] = true;
                }
            }
        }

        jassert(worklist.empty());
    }

    void resolveDomains() {
        propagate(
                resolution.domains,
                [this](size_t edgeIndex) {
                    return transferDomain(edgeIndex);
                });
    }

    void resolveChannelLayouts() {
        propagate(
                resolution.channelLayouts,
                [this](size_t edgeIndex) {
                    return transferChannelLayout(edgeIndex);
                });
    }

    const NodeGraph& graph;
    const GraphEdgeView& edges;
    const GraphEdgeIndex& indexedEdges;
    GraphDomainResolution resolution;
};

size_t edgeIndexInGraph(const NodeGraph& graph, const Edge& edge) {
    const auto& edges = graph.getEdges();
    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        if (&edges[edgeIndex] == &edge) {
            return edgeIndex;
        }
    }

    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        const Edge& candidate = edges[edgeIndex];
        if (candidate.sourceNodeId == edge.sourceNodeId
                && candidate.sourcePortId == edge.sourcePortId
                && candidate.destNodeId == edge.destNodeId
                && candidate.destPortId == edge.destPortId
                && candidate.connectionKind == edge.connectionKind
                && candidate.attachmentType == edge.attachmentType) {
            return edgeIndex;
        }
    }

    return edges.size();
}

}

GraphDomainResolution GraphDomainResolver::resolve(const NodeGraph& graph) const {
    return resolve(graph, GraphEdgeView(graph.getEdges()));
}

GraphDomainResolution GraphDomainResolver::resolve(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    const GraphEdgeIndex edgeIndex(edges);
    return ResolutionWorklist(graph, edges, edgeIndex).run();
}

bool GraphDomainResolver::isConcreteOperationDomain(PortDomain domain) {
    return domain == PortDomain::TimeSignal
            || domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
}

bool GraphDomainResolver::isConcreteSignalDomain(PortDomain domain) {
    return isConcreteOperationDomain(domain)
            || domain == PortDomain::EnvelopeSignal
            || domain == PortDomain::MeshField;
}

bool GraphDomainResolver::isContextResolvedSource(const Node& node, const Port& port) {
    return node.kind == NodeKind::TrilinearMesh && port.id == "out";
}

PortDomain GraphDomainResolver::resolvedDomainForEdge(
        const NodeGraph& graph,
        const Edge& edge) const {
    const auto resolution = resolve(graph);
    const size_t edgeIndex = edgeIndexInGraph(graph, edge);
    return edgeIndex < resolution.domains.size()
            ? resolution.domains[edgeIndex]
            : edge.domain;
}

PortDomain GraphDomainResolver::resolvedDomainForEdge(
        const NodeGraph& graph,
        const Edge& edge,
        const std::vector<Edge>&) const {
    return resolvedDomainForEdge(graph, edge);
}

ChannelLayout GraphDomainResolver::resolvedChannelLayoutForEdge(
        const NodeGraph& graph,
        const Edge& edge) const {
    return resolvedChannelLayoutForEdge(graph, edge, resolve(graph));
}

ChannelLayout GraphDomainResolver::resolvedChannelLayoutForEdge(
        const NodeGraph& graph,
        const Edge& edge,
        const GraphDomainResolution& resolution) const {
    const size_t edgeIndex = edgeIndexInGraph(graph, edge);
    return edgeIndex < resolution.channelLayouts.size()
            ? resolution.channelLayouts[edgeIndex]
            : ChannelLayout::Mono;
}

std::vector<Edge> GraphDomainResolver::resolveSignalEdges(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder) const {
    return resolveSignalEdges(graph, nodeOrder, resolve(graph));
}

std::vector<Edge> GraphDomainResolver::resolveSignalEdges(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder,
        const GraphDomainResolution& resolution) const {
    std::vector<Edge> resolvedEdges;

    for (const auto& nodeId : nodeOrder) {
        for (size_t edgeIndex = 0; edgeIndex < graph.getEdges().size(); ++edgeIndex) {
            const Edge& edge = graph.getEdges()[edgeIndex];
            if (edge.isAttachment() || edge.sourceNodeId != nodeId) {
                continue;
            }

            Edge resolved = edge;
            resolved.domain = resolution.domains[edgeIndex];
            resolvedEdges.push_back(std::move(resolved));
        }
    }

    return resolvedEdges;
}

}
