#include "UI/ReferenceSurfaceCache.h"

#include <Algo/FFT.h>
#include <Audio/WavePeriodFolding.h>
#include <Audio/WavePeriodSpectrum.h>

namespace CycleV2 {

void ReferenceSurfaceCache::reset() {
    sample = nullptr;
    revision = 0;
    positions.clear();
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
    positions = analysis.cyclePositions;
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
        buildSpectrum();
        ++spectrumBuildCount;
    }
    return frequency;
}

void ReferenceSurfaceCache::buildSpectrum() {
    const int cycleRows = WavePeriodSpectrum::cycleSize(sample->fundNote, sample->samplerate);
    frequency.connected = true;
    frequency.domain = PortDomain::SpectralMagnitudeSignal;
    frequency.frequencyMidiNote = sample->fundNote;
    frequency.gridColumns = time.gridColumns;
    frequency.gridRows = (size_t) WavePeriodSpectrum::harmonicCount(sample->fundNote, sample->samplerate);
    frequency.values.resize(frequency.gridColumns * frequency.gridRows);
    std::vector<float> folded(time.gridColumns * (size_t) cycleRows);
    std::vector<Column> cycles;
    std::vector<Column> harmonics;
    cycles.reserve(time.gridColumns);
    harmonics.reserve(time.gridColumns);
    for (size_t index = 0; index < time.gridColumns; ++index) {
        const float position = positions.empty()
                ? (float) index / (float) (time.gridColumns - 1) : positions[index];
        cycles.emplace_back(folded.data() + index * (size_t) cycleRows,
                cycleRows, position, (char) sample->fundNote);
        harmonics.emplace_back(frequency.values.data() + index * frequency.gridRows,
                (int) frequency.gridRows, position, (char) sample->fundNote);
    }
    WavePeriodFolding::render(*sample, cycles, sample->size(), true, true, 0.5,
            false, !positions.empty());
    Transform transform;
    WavePeriodSpectrum::render(transform, cycles, harmonics);
}

}
