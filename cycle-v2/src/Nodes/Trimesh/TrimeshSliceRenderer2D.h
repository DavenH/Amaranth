#pragma once

#include "TrimeshNodeModel.h"
#include "TrimeshRenderProfile.h"

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

class TrimeshSliceRenderer2D {
public:
    static void drawTrace(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<float>& values,
            juce::Colour colour);
    static void drawGrid(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const TrimeshRenderProfile& profile);
    static void drawTraceFill(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<float>& values,
            const TrimeshRenderProfile& profile);
    static void drawVertexMarkers(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexMarker>& markers);
};

}
