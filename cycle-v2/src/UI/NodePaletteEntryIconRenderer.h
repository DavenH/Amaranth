#pragma once

#include <JuceHeader.h>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

class NodePaletteEntryIconRenderer {
public:
    static bool hasIcon(NodeKind kind);
    static void paint(
            Graphics& graphics,
            NodeKind kind,
            Rectangle<float> area,
            bool hover);
    static void paintNeutral(
            Graphics& graphics,
            NodeKind kind,
            Rectangle<float> area,
            float opacity = 0.96f);
};

}
