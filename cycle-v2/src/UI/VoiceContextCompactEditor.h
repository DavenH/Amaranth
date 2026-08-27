#pragma once

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class VoiceContextCompactEditor {
public:
    static Rectangle<float> nodeSelectorBounds(Rectangle<float> nodeBounds, float zoom);

    static String domain(const Node& node);
    static String domainLabel(const Node& node);
    static String nextDomain(const Node& node);
    static String summaryLabel(const Node& node, double voiceDurationSeconds);

    static void paintNodeSelector(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom,
            const Node& node);
    static void paintNodeSummary(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom,
            const Node& node,
            double voiceDurationSeconds);
    static bool hitNodeSelector(
            Rectangle<float> nodeBounds,
            float zoom,
            Point<float> position);
};

}
