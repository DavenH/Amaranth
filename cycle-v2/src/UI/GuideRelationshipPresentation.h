#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct NodeCanvasPresentationFrame;

class GuideRelationshipPresentation {
public:
    static void paintTether(
            juce::Graphics& graphics,
            const NodeCanvasPresentationFrame& frame);
    static void paintHighlights(
            juce::Graphics& graphics,
            const NodeCanvasPresentationFrame& frame);
};

}
