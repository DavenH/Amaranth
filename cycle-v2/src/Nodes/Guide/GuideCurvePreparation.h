#pragma once

#include <Array/Buffer.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GuideHeatmapAsset;

class GuideCurvePreparation {
public:
    static bool prepare(
            const GuideCurveResource& resource,
            const GuideHeatmapAsset* heatmap,
            Buffer<float> path,
            Buffer<float> bipolarOutput);
};

}
