#pragma once

#include <optional>

#include "Graph/GraphEditTypes.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct VoiceContextInputSource {
    NodeKind kind;
    String outputPortId;
    String envelopePurpose;
    String label;
};

class VoiceContextInputCreation {
public:
    static std::optional<VoiceContextInputSource> sourceFor(const String& portId);
    static bool canCreate(
            const Node* node,
            const PortAddress& destination,
            bool connected);
    static Point<float> positionFor(
            const NodeGraph& graph,
            const Node& voice,
            const Port& destination,
            const VoiceContextInputSource& source);
};

}
