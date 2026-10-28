#pragma once

#include "Reference/ReferenceSampleAnalysis.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

class ReferenceSurfaceCache {
public:
    void reset();
    bool refresh(const ReferenceSampleAnalysis& analysis);
    const GraphPreviewResult::SignalProbePreview& surface(bool spectral);
    int generation() const { return buildCount; }
    int spectrumGeneration() const { return spectrumBuildCount; }
    const std::vector<float>& columnPositions() const { return positions; }

private:
    void buildSpectrum();

    const PitchedSample* sample {};
    uint64_t revision {};
    std::vector<float> positions;
    GraphPreviewResult::SignalProbePreview time;
    GraphPreviewResult::SignalProbePreview frequency;
    int buildCount {};
    int spectrumBuildCount {};
};

}
