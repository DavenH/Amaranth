#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "UI/NodeInputQuickCreate.h"

#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphNodeFactory.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodePortGeometry.h"

namespace CycleV2 {

namespace {

Rectangle<float> boundsInLayout(
        const Node& node,
        const NodeInputQuickCreateLayout& layout) {
    const auto moved = std::find_if(layout.moves.begin(), layout.moves.end(),
            [&](const NodeInputQuickCreateMove& move) {
                return move.nodeId == node.id;
            });
    return moved == layout.moves.end()
            ? node.bounds : node.bounds.withPosition(moved->position);
}

bool layoutIsClear(
        const NodeGraph& graph,
        const Node& prototype,
        const NodeInputQuickCreateLayout& layout) {
    const Rectangle<float> candidate = prototype.bounds
            .withPosition(layout.position).expanded(12.f);
    for (const Node& node : graph.getNodes()) {
        if (boundsInLayout(node, layout).intersects(candidate)) {
            return false;
        }
    }
    for (const auto& move : layout.moves) {
        const Node* moved = graph.findNode(move.nodeId);
        const Rectangle<float> movedBounds = boundsInLayout(*moved, layout).expanded(12.f);
        for (const Node& other : graph.getNodes()) {
            if (other.id != move.nodeId
                    && movedBounds.intersects(boundsInLayout(other, layout))) {
                return false;
            }
        }
    }
    return true;
}

NodeInputQuickCreateLayout vacantLayoutFor(
        const NodeGraph& graph,
        const Node& prototype,
        float firstX,
        float firstY) {
    constexpr std::array<int, 9> rowOffsets { 0, 1, -1, 2, -2, 3, -3, 4, -4 };
    for (int column = 0; column < 24; ++column) {
        const float x = firstX - (float) column * (prototype.bounds.getWidth() + 32.f);
        for (const int rowOffset : rowOffsets) {
            const float y = firstY + (float) rowOffset * (prototype.bounds.getHeight() + 24.f);
            const Rectangle<float> candidate = prototype.bounds.withPosition(x, y).expanded(12.f);
            const bool occupied = std::any_of(
                    graph.getNodes().begin(), graph.getNodes().end(),
                    [&](const Node& node) {
                        return node.bounds.intersects(candidate);
                    });
            if (!occupied) {
                return { { x, y }, {} };
            }
        }
    }
    return { { firstX, firstY }, {} };
}

std::optional<NodeInputQuickCreateLayout> stackedVoiceLayoutFor(
        const NodeGraph& graph,
        const Node& voice,
        const Port& destination,
        const Node& prototype,
        float firstX) {
    const int destinationIndex = (int) std::distance(
            voice.inputs.begin(),
            std::find_if(voice.inputs.begin(), voice.inputs.end(),
                    [&](const Port& port) { return port.id == destination.id; }));
    std::vector<std::pair<const Node*, int>> existingSources;
    for (const Edge& edge : graph.getEdges()) {
        if (edge.destNodeId != voice.id) {
            continue;
        }
        const auto input = std::find_if(voice.inputs.begin(), voice.inputs.end(),
                [&](const Port& port) { return port.id == edge.destPortId; });
        const Node* connected = graph.findNode(edge.sourceNodeId);
        if (input != voice.inputs.end() && connected != nullptr
                && connected->bounds.getRight() <= voice.bounds.getX()) {
            existingSources.emplace_back(connected,
                    (int) std::distance(voice.inputs.begin(), input));
        }
    }

    if (existingSources.empty()) {
        return std::nullopt;
    }
    constexpr float gap = 24.f;
    float upperBottom = -std::numeric_limits<float>::infinity();
    float lowerTop = std::numeric_limits<float>::infinity();
    for (const auto& [node, index] : existingSources) {
        if (index < destinationIndex) {
            upperBottom = jmax(upperBottom, node->bounds.getBottom());
        } else if (index > destinationIndex) {
            lowerTop = jmin(lowerTop, node->bounds.getY());
        }
    }
    const bool hasUpper = std::isfinite(upperBottom);
    const bool hasLower = std::isfinite(lowerTop);
    const float required = prototype.bounds.getHeight() + 2.f * gap;
    const float deficit = hasUpper && hasLower
            ? jmax(0.f, required - (lowerTop - upperBottom)) : 0.f;
    const float upperShift = hasUpper && hasLower ? -deficit * 0.5f : 0.f;
    const float lowerShift = hasUpper && hasLower ? deficit * 0.5f : 0.f;
    const float insertedY = hasUpper
            ? upperBottom + upperShift + gap
            : lowerTop - prototype.bounds.getHeight() - gap;
    NodeInputQuickCreateLayout layout { { firstX, insertedY }, {} };
    for (const auto& [node, index] : existingSources) {
        const float shift = index < destinationIndex ? upperShift : lowerShift;
        if (shift != 0.f) {
            layout.moves.push_back({ node->id,
                    node->bounds.getPosition().translated(0.f, shift) });
        }
    }
    return layoutIsClear(graph, prototype, layout)
            ? std::optional<NodeInputQuickCreateLayout>(std::move(layout))
            : std::nullopt;
}

const Port* inputFor(const Node& node, const String& portId) {
    const auto found = std::find_if(node.inputs.begin(), node.inputs.end(),
            [&](const Port& port) { return port.id == portId; });
    return found != node.inputs.end() ? &*found : nullptr;
}

const Port* outputFor(const Node& node, const String& portId) {
    const auto found = std::find_if(node.outputs.begin(), node.outputs.end(),
            [&](const Port& port) { return port.id == portId; });
    return found != node.outputs.end() ? &*found : nullptr;
}

}

std::optional<NodeInputQuickCreateSource> NodeInputQuickCreate::sourceFor(
        const Node& node,
        const PortAddress& destination) {
    if (!destination.input || destination.nodeId != node.id) {
        return std::nullopt;
    }
    if (node.kind == NodeKind::VoiceContext) {
        if (inputFor(node, destination.portId) == nullptr) {
            return std::nullopt;
        }
        if (destination.portId == "modulation") {
            return NodeInputQuickCreateSource {
                    NodeKind::ModulationTriple, "modulation", {}, "Modulation Triple" };
        }
        if (destination.portId == "unison") {
            return NodeInputQuickCreateSource { NodeKind::Unison, "unison", {}, "Unison" };
        }
        if (destination.portId == "pitch") {
            return NodeInputQuickCreateSource {
                    NodeKind::Envelope, "env", "pitch", "Pitch Envelope" };
        }
        if (destination.portId == "scratch") {
            return NodeInputQuickCreateSource {
                    NodeKind::Envelope, "env", "scratch", "Scratch Envelope" };
        }
    }
    if (node.kind == NodeKind::TrilinearMesh) {
        if (destination.portId == "scratch" && inputFor(node, "scratch") != nullptr) {
            return NodeInputQuickCreateSource {
                    NodeKind::Envelope, "env", "scratch", "Scratch Envelope" };
        }
        if (ModulationCableBundle::isAddress(destination)) {
            return NodeInputQuickCreateSource {
                    NodeKind::ModulationTriple,
                    ModulationCableBundle::portId(), {}, "Modulation Triple" };
        }
    }
    return std::nullopt;
}

bool NodeInputQuickCreate::canCreate(
        const Node* node,
        const PortAddress& destination,
        const GraphEdgeIndex& edgeIndex) {
    if (node == nullptr || !sourceFor(*node, destination).has_value()) {
        return false;
    }
    if (node->kind == NodeKind::TrilinearMesh
            && ModulationCableBundle::isAddress(destination)) {
        constexpr std::array<const char*, 3> axes { "yellow", "red", "blue" };
        return std::all_of(axes.begin(), axes.end(), [&](const char* axis) {
            return edgeIndex.edgesToInput(node->id, axis).empty();
        });
    }
    return edgeIndex.edgesToInput(node->id, destination.portId).empty();
}

NodeInputQuickCreateLayout NodeInputQuickCreate::layoutFor(
        const NodeGraph& graph,
        const Node& destinationNode,
        const PortAddress& destination,
        const NodeInputQuickCreateSource& source) {
    const Node prototype = GraphNodeFactory().createNode(source.kind, {}, {});
    const bool bundle = ModulationCableBundle::isAddress(destination);
    const Port* output = bundle ? nullptr : outputFor(prototype, source.outputPortId);
    jassert(bundle || output != nullptr);
    const float outputY = source.kind == NodeKind::ModulationTriple
            ? ModulationCableBundle::worldCentre(prototype, false).y
            : NodeCanvasScene::portWorldCentre(prototype, *output).y;
    const float destinationY = socketWorldCentre(destinationNode, destination).y;
    const float firstX = destinationNode.bounds.getX()
            - prototype.bounds.getWidth() - 52.f;
    const float firstY = destinationY - outputY;
    if (destinationNode.kind == NodeKind::VoiceContext) {
        const Port* input = inputFor(destinationNode, destination.portId);
        jassert(input != nullptr);
        if (const auto stacked = stackedVoiceLayoutFor(
                    graph, destinationNode, *input, prototype, firstX)) {
            return *stacked;
        }
    }
    return vacantLayoutFor(graph, prototype, firstX, firstY);
}

Point<float> NodeInputQuickCreate::socketWorldCentre(
        const Node& node,
        const PortAddress& destination) {
    if (ModulationCableBundle::isAddress(destination)) {
        return ModulationCableBundle::worldCentre(node, true);
    }
    const Port* input = inputFor(node, destination.portId);
    jassert(input != nullptr);
    return input != nullptr
            ? NodeCanvasScene::portWorldCentre(node, *input)
            : node.bounds.getCentre();
}

Rectangle<float> NodeInputQuickCreate::badgeBounds(
        Point<float> socket,
        float zoom) {
    const float rowHeight = NodePortGeometry::sidePortSpacing * zoom;
    const float diameter = jmax(4.f, jmin(15.6f, rowHeight));
    return Rectangle<float>(diameter, diameter).withCentre(
            { socket.x - 13.f, socket.y });
}

}
