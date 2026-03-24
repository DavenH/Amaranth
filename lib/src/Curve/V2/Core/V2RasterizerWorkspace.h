#pragma once

#include <vector>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Intercept.h>
#include <Curve/V2/Core/V2RenderTypes.h>

class V2RasterizerWorkspace {
public:
    V2RasterizerWorkspace() = default;

    void prepare(const V2CapacitySpec& capacities);
    void reset();
    bool isPrepared() const noexcept;
    const V2CapacitySpec& getCapacities() const noexcept;

    std::vector<Intercept> intercepts;
    std::vector<Curve> curves;
    std::vector<V2DeformRegion> deformRegions;

    ScopedAlloc<float> waveMemory;
    V2WaveBuffers waveBuffers;

private:
    V2CapacitySpec capacities;
};
