#pragma once

#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <JuceHeader.h>

namespace CycleV2 {

class TrimeshSurfaceRenderer {
public:
    static juce::Colour colourForProfile(float value, const TrimeshRenderProfile& profile);
    static juce::Image createHeatmapImage(
            const TrimeshRenderData& renderData,
            const TrimeshRenderProfile& profile,
            bool opaque = false);
    static void drawHeatmap(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const TrimeshRenderData& renderData,
            const TrimeshRenderProfile& profile,
            bool drawGrid);
};

}
