#include <algorithm>
#include <array>

#include "UI/VoiceContextInputCreation.h"

#include "Graph/GraphNodeFactory.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodeCanvasScene.h"

namespace CycleV2 {

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

Point<float> VoiceContextInputCreation::positionFor(
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
                return { x, y };
            }
        }
    }
    return { firstX, firstY };
}

}
