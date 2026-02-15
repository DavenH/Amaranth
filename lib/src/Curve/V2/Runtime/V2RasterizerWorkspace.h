#pragma once

#include <vector>

#include "../../../Array/ScopedAlloc.h"
#include "../../Curve.h"
#include "../../Intercept.h"
#include "V2RenderTypes.h"

class V2RasterizerWorkspace {
public:
    V2RasterizerWorkspace() = default;

    void prepare(const V2CapacitySpec& capacities);
    void reset();
    bool isPrepared() const noexcept;
    const V2CapacitySpec& getCapacities() const noexcept;

    std::vector<Intercept> intercepts;
    std::vector<Curve> curves;
    std::vector<Intercept> deformRegionStarts;
    std::vector<Intercept> deformRegionEnds;

    ScopedAlloc<float> waveMemory;
    Buffer<float> waveX;
    Buffer<float> waveY;
    Buffer<float> slope;
    Buffer<float> diffX;

private:
    V2CapacitySpec capacities;
};

