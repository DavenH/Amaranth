#pragma once

#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>
#include "Nodes/Trimesh/Rendering/TrimeshGridRenderService.h"

namespace CycleV2 {

class EnvelopeMorphSurfacePreview {
public:
    void paint(juce::Graphics& graphics, juce::Rectangle<float> bounds,
            Mesh& mesh, GuideCurveProvider* guides, uint64_t revision,
            const MorphPosition& morph, int axis, int dimensions);

private:
    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    TrimeshRenderData data;
    juce::Image image;
    uint64_t cachedRevision {};
    MorphPosition cachedMorph;
    int cachedAxis { -1 };
    int cachedDimensions { -1 };
    juce::Point<int> cachedSize;
};

}
