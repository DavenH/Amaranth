#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "UI/VoiceContextInputCreation.h"

#include "Graph/GraphNodeFactory.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodePortGeometry.h"

namespace CycleV2 {

namespace {

Rectangle<float> boundsInLayout(
        const Node& node,
        const VoiceContextInputLayout& layout) {
    const auto moved = std::find_if(layout.moves.begin(), layout.moves.end(),
            [&](const VoiceContextInputMove& move) {
                return move.nodeId == node.id;
            });
    return moved == layout.moves.end()
            ? node.bounds : node.bounds.withPosition(moved->position);
}

bool layoutIsClear(
        const NodeGraph& graph,
        const Node& prototype,
        const VoiceContextInputLayout& layout) {
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

}

std::optional<VoiceContextInputSource> VoiceContextInputCreation::sourceFor(
        const String& portId) {
    if (portId == "modulation") {
        return VoiceContextInputSource {
                NodeKind::ModulationTriple, "modulation", {}, "Modulation Triple" };
    }
    if (portId == "unison") {
        return VoiceContextInputSource { NodeKind::Unison, "unison", {}, "Unison" };
    }
    if (portId == "pitch") {
        return VoiceContextInputSource { NodeKind::Envelope, "env", "pitch", "Pitch Envelope" };
    }
    if (portId == "scratch") {
        return VoiceContextInputSource { NodeKind::Envelope, "env", "scratch", "Scratch Envelope" };
    }
    return std::nullopt;
}

bool VoiceContextInputCreation::canCreate(
        const Node* node,
        const PortAddress& destination,
        bool connected) {
    return node != nullptr
            && node->kind == NodeKind::VoiceContext
            && destination.input
            && !connected
            && sourceFor(destination.portId).has_value();
}

VoiceContextInputLayout VoiceContextInputCreation::layoutFor(
        const NodeGraph& graph,
        const Node& voice,
        const Port& destination,
        const VoiceContextInputSource& source) {
    const Node prototype = GraphNodeFactory().createNode(source.kind, {}, {});
    const auto output = std::find_if(
            prototype.outputs.begin(), prototype.outputs.end(),
            [&](const Port& port) { return port.id == source.outputPortId; });
    jassert(output != prototype.outputs.end());
    const float outputOffsetY = source.kind == NodeKind::ModulationTriple
            ? ModulationCableBundle::worldCentre(prototype, false).y
            : NodeCanvasScene::portWorldCentre(prototype, *output).y;
    const float destinationY = NodeCanvasScene::portWorldCentre(voice, destination).y;
    const float firstX = voice.bounds.getX() - prototype.bounds.getWidth() - 52.f;
    const float firstY = destinationY - outputOffsetY;
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

    if (!existingSources.empty()) {
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
        VoiceContextInputLayout layout { { firstX, insertedY }, {} };
        for (const auto& [node, index] : existingSources) {
            const float shift = index < destinationIndex ? upperShift : lowerShift;
            if (shift != 0.f) {
                layout.moves.push_back({ node->id,
                        node->bounds.getPosition().translated(0.f, shift) });
            }
        }
        if (layoutIsClear(graph, prototype, layout)) {
            return layout;
        }
    }
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

Rectangle<float> VoiceContextInputCreation::badgeBounds(
        Point<float> socket,
        float zoom) {
    const float rowHeight = NodePortGeometry::sidePortSpacing * zoom;
    const float diameter = jmax(4.f, jmin(15.6f, rowHeight));
    return Rectangle<float>(diameter, diameter).withCentre(
            { socket.x - 13.f, socket.y });
}

}
