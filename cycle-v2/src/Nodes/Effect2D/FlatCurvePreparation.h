#pragma once

#include "../../Graph/NodeGraph.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>

namespace CycleV2 {

class FlatCurvePreparation {
public:
    FlatCurvePreparation(
            const String& name,
            NodeKind kind,
            const std::vector<NodeParameter>& parameters,
            FXRasterizer::ScalingType scaling);
    ~FlatCurvePreparation();

    bool prepare();
    Rasterization::SamplerView sampler() const { return rasterizer.sampler(); }

private:
    NodeKind kind;
    const std::vector<NodeParameter>& parameters;
    Mesh mesh;
    FXRasterizer rasterizer;
};

}
