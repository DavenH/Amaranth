#include "UI/ReferenceSurfaceCache.h"

#include "Runtime/DefaultOutputPreview.h"

namespace CycleV2 {

void ReferenceSurfaceCache::reset() {
    sample = nullptr;
    revision = 0;
    time = {};
    frequency = {};
}

bool ReferenceSurfaceCache::refresh(const ReferenceSampleAnalysis& analysis) {
    if (analysis.sample == nullptr
            || (sample == analysis.sample.get() && revision == analysis.cyclogramRevision)) {
        return false;
    }
    sample = analysis.sample.get();
    revision = analysis.cyclogramRevision;
    time = {};
    time.connected = true;
    time.domain = PortDomain::TimeSignal;
    time.gridColumns = (size_t) analysis.columns;
    time.gridRows = (size_t) analysis.rows;
    time.values = analysis.cyclogram;
    frequency = {};
    ++buildCount;
    return true;
}

const GraphPreviewResult::SignalProbePreview& ReferenceSurfaceCache::surface(bool spectral) {
    if (!spectral) {
        return time;
    }
    if (frequency.values.empty() && !time.values.empty()) {
        frequency = DefaultOutputPreview::spectrum(time);
        ++spectrumBuildCount;
    }
    return frequency;
}

}
