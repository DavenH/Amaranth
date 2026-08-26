#pragma once

#include "Graph/NodeGraph.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>

namespace CycleV2 {

class FlatCurvePreparation {
public:
    FlatCurvePreparation(
            const String& name,
            NodeModelStatePtr model,
            FXRasterizer::ScalingType scaling);
    ~FlatCurvePreparation();

    bool prepare();
    Rasterization::SamplerView sampler() const { return rasterizer.sampler(); }

private:
    NodeModelStatePtr model;
    Mesh mesh;
    FXRasterizer rasterizer;
};

}
