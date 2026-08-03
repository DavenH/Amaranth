#include "ModulationCableBundle.h"

#include <algorithm>
#include <array>

namespace CycleV2 {

namespace {

constexpr const char* kBundlePortId = "__modulationBundle";
constexpr const char* kAttachmentPortId = "modulation";
const std::array<const char*, 3> kAxes { "yellow", "red", "blue" };

const Node* nodeFor(const NodeGraph& graph, const PortAddress& address) {
    return graph.findNode(address.nodeId);
}

bool isAxisEdge(const Edge& edge) {
    return edge.sourcePortId == edge.destPortId
            && (edge.sourcePortId == "yellow"
                || edge.sourcePortId == "red"
                || edge.sourcePortId == "blue");
}

bool destinationSupportsAxis(const Node& node, const String& axis) {
    if (node.kind == NodeKind::TrilinearMesh) {
        return axis == "yellow" || axis == "red" || axis == "blue";
    }
    return node.kind == NodeKind::Envelope
            && (axis == "red" || axis == "blue");
}

bool isVoiceContextDestination(const Node& node, const PortAddress& address) {
    return node.kind == NodeKind::VoiceContext
            && address.input
            && address.portId == kAttachmentPortId;
}

}

String ModulationCableBundle::portId() {
    return kBundlePortId;
}

PortAddress ModulationCableBundle::sourceAddress(const Node& node) {
    return { node.id, portId(), false };
}

PortAddress ModulationCableBundle::destinationAddress(const Node& node) {
    return { node.id, portId(), true };
}

bool ModulationCableBundle::isAddress(const PortAddress& address) {
    return address.portId == kBundlePortId;
}

bool ModulationCableBundle::isSource(
        const NodeGraph& graph,
        const PortAddress& address) {
    const Node* node = nodeFor(graph, address);
    return node != nullptr
            && !address.input
            && isAddress(address)
            && node->kind == NodeKind::ModulationTriple;
}

bool ModulationCableBundle::isDestination(
        const NodeGraph& graph,
        const PortAddress& address) {
    const Node* node = nodeFor(graph, address);
    return node != nullptr
            && address.input
            && ((isAddress(address) && supportsDestination(*node))
                || isVoiceContextDestination(*node, address));
}

bool ModulationCableBundle::supportsDestination(const Node& node) {
    return node.kind == NodeKind::TrilinearMesh
            || node.kind == NodeKind::Envelope;
}

bool ModulationCableBundle::destinationIncludesYellow(const Node& node) {
    return node.kind == NodeKind::TrilinearMesh;
}

std::vector<ModulationCableBundleRoute> ModulationCableBundle::routes(
        const NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) {
    const PortAddress& source = isSource(graph, first) ? first : second;
    const PortAddress& destination = isDestination(graph, first) ? first : second;
    if (!isSource(graph, source) || !isDestination(graph, destination)) {
        return {};
    }
    const Node* destinationNode = nodeFor(graph, destination);
    jassert(destinationNode != nullptr);

    if (isVoiceContextDestination(*destinationNode, destination)) {
        return {{
                { source.nodeId, kAttachmentPortId, false },
                { destination.nodeId, kAttachmentPortId, true }
        }};
    }

    std::vector<ModulationCableBundleRoute> result;
    result.reserve(kAxes.size());
    for (const char* axis : kAxes) {
        if (!destinationSupportsAxis(*destinationNode, axis)) {
            continue;
        }
        result.push_back({
                { source.nodeId, axis, false },
                { destination.nodeId, axis, true }
        });
    }
    return result;
}

bool ModulationCableBundle::canConnect(
        const NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) {
    NodeGraph candidate = graph;
    const auto bundleRoutes = routes(graph, first, second);
    if (bundleRoutes.empty()) {
        return false;
    }

    for (const auto& route : bundleRoutes) {
        if (!GraphEditor().connect(candidate, route.source, route.destination).succeeded()) {
            return false;
        }
    }
    return true;
}

std::vector<int> ModulationCableBundle::edgeIndices(
        const NodeGraph& graph,
        int edgeIndex) {
    const auto& edges = graph.getEdges();
    if (!isPositiveAndBelow(edgeIndex, (int) edges.size())) {
        return {};
    }

    const Edge& selected = edges[(size_t) edgeIndex];
    const Node* source = graph.findNode(selected.sourceNodeId);
    const Node* destination = graph.findNode(selected.destNodeId);
    if (source == nullptr
            || destination == nullptr
            || source->kind != NodeKind::ModulationTriple
            || !supportsDestination(*destination)
            || !isAxisEdge(selected)
            || !destinationSupportsAxis(*destination, selected.destPortId)) {
        return { edgeIndex };
    }

    std::vector<int> result;
    for (const char* axis : kAxes) {
        if (!destinationSupportsAxis(*destination, axis)) {
            continue;
        }
        const auto found = std::find_if(
                edges.begin(),
                edges.end(),
                [&](const Edge& edge) {
                    return edge.sourceNodeId == selected.sourceNodeId
                            && edge.destNodeId == selected.destNodeId
                            && edge.sourcePortId == axis
                            && edge.destPortId == axis
                            && !edge.isAttachment();
                });
        if (found == edges.end()) {
            return { edgeIndex };
        }
        result.push_back((int) std::distance(edges.begin(), found));
    }
    return result;
}

std::optional<std::vector<int>> ModulationCableBundle::bundleBeginningAt(
        const NodeGraph& graph,
        int edgeIndex) {
    auto indices = edgeIndices(graph, edgeIndex);
    if (indices.size() < 2
            || *std::min_element(indices.begin(), indices.end()) != edgeIndex) {
        return std::nullopt;
    }
    return indices;
}

bool ModulationCableBundle::hidesIndividualPort(const Node& node, const Port& port) {
    return port.input && destinationSupportsAxis(node, port.id);
}

bool ModulationCableBundle::usesSharedSourceSocket(const Node& node, const Edge& edge) {
    return node.kind == NodeKind::ModulationTriple
            && edge.sourcePortId == kAttachmentPortId;
}

Point<float> ModulationCableBundle::worldCentre(const Node& node, bool input) {
    const Point<float> boundary {
            input ? node.bounds.getX() : node.bounds.getRight(),
            input
                    ? node.bounds.getCentreY()
                    : node.bounds.getY() + NodePortGeometry::firstSidePortOffset
    };
    return input
            ? NodePortGeometry::socketCentreForAttachedIcon(boundary, PortSide::Left)
            : boundary;
}

}
