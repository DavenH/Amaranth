#pragma once

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class VoiceContextCompactEditor {
public:
    static Rectangle<float> summaryBounds(Rectangle<float> nodeBounds, float zoom);
    static Rectangle<float> scratchIndicatorBounds(Rectangle<float> nodeBounds, float zoom);
    static Rectangle<float> scratchLabelBounds(Rectangle<float> nodeBounds, float zoom);

    static String summaryLabel(const Node& node);

    static void paintNodeSummary(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom,
            const Node& node);
    static void paintScratchIndicator(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom);
};

}
