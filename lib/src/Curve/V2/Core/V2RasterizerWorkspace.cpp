#include "V2RasterizerWorkspace.h"

void V2RasterizerWorkspace::prepare(const V2CapacitySpec& capacities) {
    this->capacities = capacities;

    intercepts.clear();
    curves.clear();
    deformRegions.clear();

    intercepts.reserve(capacities.maxIntercepts);
    curves.reserve(capacities.maxCurves);
    deformRegions.reserve(capacities.maxDeformRegions);

    waveMemory.ensureSize(4 * capacities.maxWavePoints);
    waveMemory.resetPlacement();
    waveBuffers.waveX = waveMemory.place(capacities.maxWavePoints);
    waveBuffers.waveY = waveMemory.place(capacities.maxWavePoints);
    waveBuffers.slope = waveMemory.place(capacities.maxWavePoints);
    waveBuffers.diffX = waveMemory.place(capacities.maxWavePoints);
}

void V2RasterizerWorkspace::reset() {
    intercepts.clear();
    curves.clear();
    deformRegions.clear();

    waveBuffers.zeroIfAllocated();
}

bool V2RasterizerWorkspace::isPrepared() const noexcept {
    return capacities.isValid();
}

const V2CapacitySpec& V2RasterizerWorkspace::getCapacities() const noexcept {
    return capacities;
}
