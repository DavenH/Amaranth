#pragma once

#include <optional>
#include <vector>

#include "Graph/GraphEditTypes.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct VoiceContextInputSource {
    NodeKind kind;
    String outputPortId;
    String envelopePurpose;
    String label;
};

struct VoiceContextInputMove {
    String nodeId;
    Point<float> position;
};

struct VoiceContextInputLayout {
    Point<float> position;
    std::vector<VoiceContextInputMove> moves;
};

class VoiceContextInputCreation {
public:
    static std::optional<VoiceContextInputSource> sourceFor(const String& portId);
    static bool canCreate(
            const Node* node,
            const PortAddress& destination,
            bool connected);
    static VoiceContextInputLayout layoutFor(
            const NodeGraph& graph,
            const Node& voice,
            const Port& destination,
            const VoiceContextInputSource& source);
    static Rectangle<float> badgeBounds(Point<float> socket, float zoom);
};

}
