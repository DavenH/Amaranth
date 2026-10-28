#include "WavePeriodSpectrum.h"

#include "../Algo/FFT.h"
#include "../Util/LogRegionMapping.h"
#include "../Util/NumberUtils.h"

int WavePeriodSpectrum::harmonicCount(int legacyMidiNote, double sampleRate) {
    return LogRegionMapping(legacyMidiNote, sampleRate).regionSize();
}

int WavePeriodSpectrum::cycleSize(int legacyMidiNote, double sampleRate) {
    return NumberUtils::nextPower2(2 * harmonicCount(legacyMidiNote, sampleRate));
}

void WavePeriodSpectrum::render(
        Transform& transform,
        const std::vector<Column>& cycles,
        std::vector<Column>& magnitudes,
        std::vector<Column>* phases) {
    jassert(cycles.size() == magnitudes.size());
    jassert(phases == nullptr || phases->size() == cycles.size());
    if (cycles.empty()) {
        return;
    }
    transform.allocate(cycles.front().size(), Transform::DivFwdByN, true);
    for (size_t index = 0; index < cycles.size(); ++index) {
        transform.forward(cycles[index]);
        auto magnitude = magnitudes[index];
        transform.getMagnitudes().withSize(magnitude.size()).copyTo(magnitude);
        magnitude[magnitude.size() - 1] = 0.f;
        if (phases != nullptr) {
            auto phase = (*phases)[index];
            transform.getPhases().withSize(phase.size()).copyTo(phase);
            phase[phase.size() - 1] = 0.f;
        }
    }
}
