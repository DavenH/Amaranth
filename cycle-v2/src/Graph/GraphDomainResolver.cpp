#include "GraphDomainResolver.h"

#include <algorithm>
#include <deque>
#include <unordered_map>

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

bool isInputResolvedPassthrough(const Node& node) {
    return node.kind == NodeKind::Spy;
}

PortDomain voiceContextDomain(const Node& voiceNode) {
    const String domain = parameterValueForNode(voiceNode, "domain", "waveform");

    if (domain == "spectral" || domain == "spectralMagnitude") {
        return PortDomain::SpectralMagnitudeSignal;
    }

    if (domain == "spectralPhase") {
        return PortDomain::SpectralPhaseSignal;
    }

    return PortDomain::TimeSignal;
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
    explicit ResolutionWorklist(const NodeGraph& graphToResolve) :
            graph(graphToResolve)
        ,   edges(graph.getEdges()) {
        resolution.domains.reserve(edges.size());
        resolution.channelLayouts.resize(edges.size(), ChannelLayout::Mono);
        incoming.resize(graph.getNodes().size());
        outgoing.resize(graph.getNodes().size());

        for (size_t nodeIndex = 0; nodeIndex < graph.getNodes().size(); ++nodeIndex) {
            nodeIndices.emplace(graph.getNodes()[nodeIndex].id, nodeIndex);
        }

        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const Edge& edge = edges[edgeIndex];
            resolution.domains.push_back(edge.domain);
            if (edge.attachment) {
                continue;
            }

            const auto source = nodeIndices.find(edge.sourceNodeId);
            if (source != nodeIndices.end()) {
                outgoing[source->second].push_back(edgeIndex);
            }

            const auto dest = nodeIndices.find(edge.destNodeId);
            if (dest != nodeIndices.end()) {
                incoming[dest->second].push_back(edgeIndex);
            }
        }
    }

    GraphDomainResolution run() {
        resolveDomains();
        resolveChannelLayouts();
        return std::move(resolution);
    }

private:
    const Node* node(const String& id) const {
        const auto found = nodeIndices.find(id);
        return found != nodeIndices.end()
                ? &graph.getNodes()[found->second]
                : nullptr;
    }

    size_t nodeIndex(const String& id) const {
        const auto found = nodeIndices.find(id);
        return found != nodeIndices.end() ? found->second : graph.getNodes().size();
    }

    PortDomain contextDomain(const Node& nodeToResolve) const {
        const size_t index = nodeIndex(nodeToResolve.id);
        if (index >= incoming.size()) {
            return PortDomain::ControlSignal;
        }

        for (const size_t edgeIndex : incoming[index]) {
            const Edge& edge = edges[edgeIndex];
            if (edge.destPortId != "context") {
                continue;
            }

            const Node* source = node(edge.sourceNodeId);
            if (source != nullptr && source->kind == NodeKind::VoiceContext) {
                return voiceContextDomain(*source);
            }
        }

        return PortDomain::ControlSignal;
    }

    PortDomain firstInputDomain(
            const Node& nodeToResolve,
            bool operationDomain) const {
        const size_t index = nodeIndex(nodeToResolve.id);
        if (index >= incoming.size()) {
            return PortDomain::ControlSignal;
        }

        for (const size_t edgeIndex : incoming[index]) {
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
        if (edge.attachment) {
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
            sourceDomain = contextDomain(*sourceNode);
        }
        if (sourceDomain == PortDomain::ControlSignal
                && (sourceNode->kind == NodeKind::Add
                        || sourceNode->kind == NodeKind::Multiply)) {
            sourceDomain = firstInputDomain(*sourceNode, true);
        }
        if (sourceDomain == PortDomain::ControlSignal
                && isInputResolvedPassthrough(*sourceNode)) {
            sourceDomain = firstInputDomain(*sourceNode, false);
        }
        if (sourceDomain != PortDomain::ControlSignal) {
            return sourceDomain;
        }

        if (dest->domain != PortDomain::ControlSignal) {
            return dest->domain;
        }
        if (destNode->kind == NodeKind::Add || destNode->kind == NodeKind::Multiply) {
            const PortDomain operationDomain = firstInputDomain(*destNode, true);
            if (operationDomain != PortDomain::ControlSignal) {
                return operationDomain;
            }
        }
        if (isInputResolvedPassthrough(*destNode)) {
            const PortDomain passthroughDomain = firstInputDomain(*destNode, false);
            if (passthroughDomain != PortDomain::ControlSignal) {
                return passthroughDomain;
            }
        }

        return edge.domain;
    }

    ChannelLayout transferChannelLayout(size_t edgeIndex) const {
        const Edge& edge = edges[edgeIndex];
        if (edge.attachment) {
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

        if (isInputResolvedPassthrough(*sourceNode)
                && source->domain == PortDomain::ControlSignal) {
            const size_t index = nodeIndex(sourceNode->id);
            if (index < incoming.size() && !incoming[index].empty()) {
                return resolution.channelLayouts[incoming[index].front()];
            }
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
            if (!edges[edgeIndex].attachment) {
                worklist.push_back(edgeIndex);
                queued[edgeIndex] = true;
            }
        }

        const size_t maximumTransfers = std::max<size_t>(
                1u,
                edges.size() * edges.size() * 8);
        size_t transfers = 0;
        while (!worklist.empty() && transfers++ < maximumTransfers) {
            const size_t edgeIndex = worklist.front();
            worklist.pop_front();
            queued[edgeIndex] = false;

            const Value next = transfer(edgeIndex);
            if (values[edgeIndex] == next) {
                continue;
            }
            values[edgeIndex] = next;

            const size_t affectedNodeIndex = nodeIndex(edges[edgeIndex].destNodeId);
            if (affectedNodeIndex >= incoming.size()) {
                continue;
            }

            for (const size_t affected : incoming[affectedNodeIndex]) {
                if (!queued[affected]) {
                    worklist.push_back(affected);
                    queued[affected] = true;
                }
            }
            for (const size_t affected : outgoing[affectedNodeIndex]) {
                if (!queued[affected]) {
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
    const std::vector<Edge>& edges;
    std::unordered_map<String, size_t, StringHash> nodeIndices;
    std::vector<std::vector<size_t>> incoming;
    std::vector<std::vector<size_t>> outgoing;
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
                && candidate.attachment == edge.attachment) {
            return edgeIndex;
        }
    }

    return edges.size();
}

}

PortDomain GraphDomainResolver::domainFromVoiceContext(const Node& voiceNode) const {
    return voiceContextDomain(voiceNode);
}

GraphDomainResolution GraphDomainResolver::resolve(const NodeGraph& graph) const {
    return ResolutionWorklist(graph).run();
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
            if (edge.attachment || edge.sourceNodeId != nodeId) {
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
