#include <algorithm>
#include <deque>
#include <unordered_set>

#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
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

template<typename EdgeIndex>
class ResolutionWorklist {
public:
    ResolutionWorklist(
            const NodeGraph& graphToResolve,
            const GraphEdgeView& edgesToResolve,
            const EdgeIndex& edgeIndexToUse) :
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
            if (!edge.isAttachment()) {
                initialEdges.push_back(edgeIndex);
            }
        }
    }

    ResolutionWorklist(
            const NodeGraph& graphToResolve,
            const GraphEdgeView& edgesToResolve,
            const EdgeIndex& edgeIndexToUse,
            const GraphDomainResolution& baseline) :
            graph(graphToResolve)
        ,   edges(edgesToResolve)
        ,   indexedEdges(edgeIndexToUse) {
        jassert(baseline.domains.size() == edges.existingSize());
        jassert(baseline.channelLayouts.size() == edges.existingSize());
        resolution.domains.resize(edges.size());
        resolution.channelLayouts.resize(edges.size(), ChannelLayout::Mono);

        for (size_t existingIndex = 0;
                existingIndex < edges.existingSize();
                ++existingIndex) {
            const auto viewIndex = edges.viewIndexForExisting(existingIndex);
            if (viewIndex.has_value()) {
                resolution.domains[*viewIndex] = baseline.domains[existingIndex];
                resolution.channelLayouts[*viewIndex] = baseline.channelLayouts[existingIndex];
            }
        }
        for (size_t edgeIndex = edges.retainedSize(); edgeIndex < edges.size(); ++edgeIndex) {
            resolution.domains[edgeIndex] = edges[edgeIndex].domain;
        }

        initialEdges = dependencyClosure(
                indexedEdges.edgesAtChangedDestinations());
        resolution.affectedEdgeIndices = initialEdges;
        for (const size_t edgeIndex : initialEdges) {
            resolution.domains[edgeIndex] = edges[edgeIndex].domain;
            resolution.channelLayouts[edgeIndex] = ChannelLayout::Mono;
        }
    }

    GraphDomainResolution run() {
        const auto domainWork = resolveDomains();
        resolveChannelLayouts(domainWork);
        return std::move(resolution);
    }

private:
    std::vector<size_t> dependencyClosure(
            const std::vector<size_t>& seeds) const {
        std::vector<size_t> closure;
        std::deque<size_t> worklist;
        std::unordered_set<size_t> included;
        const auto append = [&](size_t edgeIndex) {
            if (edgeIndex < edges.size()
                    && !edges[edgeIndex].isAttachment()
                    && included.insert(edgeIndex).second) {
                closure.push_back(edgeIndex);
                worklist.push_back(edgeIndex);
            }
        };
        for (const size_t edgeIndex : seeds) {
            append(edgeIndex);
        }
        while (!worklist.empty()) {
            const size_t edgeIndex = worklist.front();
            worklist.pop_front();
            const String& affectedNodeId = edges[edgeIndex].destNodeId;
            for (const size_t affected : indexedEdges.incomingEdges(affectedNodeId)) {
                append(affected);
            }
            for (const size_t affected : indexedEdges.outgoingEdges(affectedNodeId)) {
                append(affected);
            }
        }
        return closure;
    }

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
    std::vector<size_t> propagate(
            std::vector<Value>& values,
            Transfer transfer,
            const std::vector<size_t>& seedEdges) {
        std::deque<size_t> worklist;
        std::unordered_set<size_t> queued;
        for (const size_t edgeIndex : seedEdges) {
            if (edgeIndex < edges.size()
                    && !edges[edgeIndex].isAttachment()
                    && queued.insert(edgeIndex).second) {
                worklist.push_back(edgeIndex);
            }
        }

        std::vector<size_t> processed;
        const size_t maximumTransfers = std::max<size_t>(
                1u,
                edges.size() * edges.size() * 8);
        size_t transfers = 0;
        while (!worklist.empty() && transfers++ < maximumTransfers) {
            InteractionComplexityDiagnostics::recordDomainTransfer();
            const size_t edgeIndex = worklist.front();
            worklist.pop_front();
            queued.erase(edgeIndex);
            processed.push_back(edgeIndex);

            const Value next = transfer(edgeIndex);
            if (values[edgeIndex] == next) {
                continue;
            }
            values[edgeIndex] = next;

            const String& affectedNodeId = edges[edgeIndex].destNodeId;
            for (const size_t affected : indexedEdges.incomingEdges(affectedNodeId)) {
                if (!edges[affected].isAttachment() && queued.insert(affected).second) {
                    worklist.push_back(affected);
                }
            }
            for (const size_t affected : indexedEdges.outgoingEdges(affectedNodeId)) {
                if (!edges[affected].isAttachment() && queued.insert(affected).second) {
                    worklist.push_back(affected);
                }
            }
        }

        jassert(worklist.empty());
        return processed;
    }

    std::vector<size_t> resolveDomains() {
        return propagate(
                resolution.domains,
                [this](size_t edgeIndex) {
                    return transferDomain(edgeIndex);
                },
                initialEdges);
    }

    void resolveChannelLayouts(const std::vector<size_t>& domainWork) {
        propagate(
                resolution.channelLayouts,
                [this](size_t edgeIndex) {
                    return transferChannelLayout(edgeIndex);
                },
                domainWork);
    }

    const NodeGraph& graph;
    const GraphEdgeView& edges;
    const EdgeIndex& indexedEdges;
    std::vector<size_t> initialEdges;
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

GraphDomainResolution GraphDomainResolver::resolve(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphEdgeIndexOverlay& edgeIndex,
        const GraphDomainResolution& baseline) const {
    if (baseline.domains.size() != edges.existingSize()
            || baseline.channelLayouts.size() != edges.existingSize()) {
        jassertfalse;
        return resolve(graph, edges);
    }
    return ResolutionWorklist(graph, edges, edgeIndex, baseline).run();
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
