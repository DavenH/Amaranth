#pragma once

#include <JuceHeader.h>

#include "Nodes/Trimesh/Rendering/TrimeshGridRenderService.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

namespace CycleV2 {

class TrimeshSurfaceRenderer {
public:
    static juce::Colour colourForProfile(float value, const TrimeshRenderProfile& profile);
    static juce::Image createTrimeshImage(
            const TrimeshRenderData& renderData,
            const TrimeshRenderProfile& profile,
            float surfaceAspectRatio,
            int displayHeight,
            bool opaque = false);
    static juce::Image createHeatmapImage(
            const TrimeshRenderData& renderData,
            const TrimeshRenderProfile& profile,
            bool opaque = false,
            float surfaceAspectRatio = 0.f);
    static void drawHeatmap(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const TrimeshRenderData& renderData,
            const TrimeshRenderProfile& profile,
            bool drawGrid);
};

}
