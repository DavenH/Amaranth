#pragma once

#include <JuceHeader.h>

#include "UI/GuideCurveShelf.h"

namespace CycleV2 {

struct NodeCanvasPresentationFrame;

class GuideRelationshipPresentation {
public:
    static juce::String highlightGuideId(const GuideCurveShelfState& state);
    static juce::String tetherGuideId(const GuideCurveShelfState& state);
    static void paintTether(
            juce::Graphics& graphics,
            const NodeCanvasPresentationFrame& frame);
    static void paintTetherTerminal(
            juce::Graphics& graphics,
            const NodeCanvasPresentationFrame& frame);
    static void paintHighlights(
            juce::Graphics& graphics,
            const NodeCanvasPresentationFrame& frame);
};

}
