#pragma once

#include <Array/Buffer.h>

namespace CycleV2 {

class GuideHeatmapAsset;

class GuideHeatmapSampler {
public:
    static float sampleBicubic(const GuideHeatmapAsset& heatmap, float x, float y);
    static bool samplePath(
            const GuideHeatmapAsset& heatmap,
            Buffer<float> path,
            Buffer<float> destination);
};

}
