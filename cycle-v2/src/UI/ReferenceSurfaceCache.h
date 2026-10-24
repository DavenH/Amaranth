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

private:
    const PitchedSample* sample {};
    uint64_t revision {};
    GraphPreviewResult::SignalProbePreview time;
    GraphPreviewResult::SignalProbePreview frequency;
    int buildCount {};
    int spectrumBuildCount {};
};

}
