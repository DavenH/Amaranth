#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class PreviewPitchResolver {
public:
    static constexpr int defaultMidiNote = 48;

    static int forGraph(const NodeGraph& graph);
    static int forNode(
            const NodeGraph& graph,
            const String& nodeId,
            int fallbackMidiNote = defaultMidiNote);
    static int forProbe(
            const NodeGraph& graph,
            const String& probeId,
            int fallbackMidiNote = defaultMidiNote);
};

}
