#include "V2RasterizerWorkspace.h"

void V2RasterizerWorkspace::prepare(const V2CapacitySpec& capacities) {
    this->capacities = capacities;

    intercepts.clear();
    curves.clear();
    deformRegionStarts.clear();
    deformRegionEnds.clear();

    intercepts.reserve(capacities.maxIntercepts);
    curves.reserve(capacities.maxCurves);
    deformRegionStarts.reserve(capacities.maxDeformRegions);
    deformRegionEnds.reserve(capacities.maxDeformRegions);

    waveMemory.ensureSize(4 * capacities.maxWavePoints);
    waveMemory.resetPlacement();
    waveX = waveMemory.place(capacities.maxWavePoints);
    waveY = waveMemory.place(capacities.maxWavePoints);
    slope = waveMemory.place(capacities.maxWavePoints);
    diffX = waveMemory.place(capacities.maxWavePoints);
}

void V2RasterizerWorkspace::reset() {
    intercepts.clear();
    curves.clear();
    deformRegionStarts.clear();
    deformRegionEnds.clear();

    if (! waveX.empty()) {
        waveX.zero();
        waveY.zero();
        slope.zero();
        diffX.zero();
    }
}

bool V2RasterizerWorkspace::isPrepared() const noexcept {
    return capacities.isValid();
}

const V2CapacitySpec& V2RasterizerWorkspace::getCapacities() const noexcept {
    return capacities;
}

