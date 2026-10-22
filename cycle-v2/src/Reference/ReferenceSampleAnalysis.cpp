#include <Algo/Pitch/PitchTracker.h>
#include <Array/Buffer.h>
#include <Audio/WavePeriodFolding.h>
#include <Util/LogRegionMapping.h>
#include <Util/NumberUtils.h>

#include "Reference/ReferenceSampleAnalysis.h"

namespace CycleV2 {

int ReferenceSampleAnalysis::standardMidiNote() const {
    return sample != nullptr ? sample->fundNote - LogRegionMapping::legacyMidiNoteBias : -1;
}

ReferenceSampleAnalysis ReferenceSampleAnalyzer::analyze(
        const juce::File& file,
        int columns,
        int rows) {
    ReferenceSampleAnalysis result;
    result.columns = columns;
    result.rows = rows;
    if (columns < 2 || rows < 4 || !file.existsAsFile()) {
        result.error = "Reference file is missing or the surface size is invalid";
        return result;
    }

    auto sample = std::make_unique<PitchedSample>();
    if (sample->load(file.getFullPathName()) != 0) {
        result.error = "This audio format could not be decoded";
        return result;
    }
    if (sample->size() < 2048) {
        result.error = "Reference is too short for pitch analysis";
        return result;
    }
    if (sample->audio.left.max() <= 0.f
            && sample->audio.left.min() >= 0.f) {
        result.error = "Reference is silent";
        return result;
    }

    PitchTracker tracker;
    tracker.setSample(sample.get());
    tracker.trackPitch(false);
    if (sample->periods.size() < 3) {
        result.error = "No periodic pitch could be identified";
        return result;
    }

    result.pitchInsecurity = tracker.getConfidence();
    result.lowConfidence = result.pitchInsecurity > 30.f;
    if (result.pitchInsecurity > 50.f) {
        sample->fundNote = 41;
        sample->createDefaultPeriods();
    } else {
        const float frequency = (float) sample->samplerate / sample->getAveragePeriod();
        sample->fundNote = juce::roundToInt(NumberUtils::frequencyToNote(frequency));
    }
    sample->fundNote = juce::jlimit(0, 127, sample->fundNote);
    result.pitchPath = sample->simplifiedPitchPath();
    result.sample = std::move(sample);
    refreshCyclogram(result);
    return result;
}

void ReferenceSampleAnalyzer::refreshCyclogram(ReferenceSampleAnalysis& analysis) {
    if (analysis.sample == nullptr || analysis.columns < 2 || analysis.rows < 4) {
        return;
    }

    analysis.sample->createPeriodsFromPitchPath(analysis.pitchPath);
    analysis.cyclogram.resize((size_t) analysis.columns * (size_t) analysis.rows);
    std::vector<Column> columns;
    columns.reserve((size_t) analysis.columns);
    for (int index = 0; index < analysis.columns; ++index) {
        columns.emplace_back(
                analysis.cyclogram.data() + (size_t) index * (size_t) analysis.rows,
                analysis.rows,
                (float) index / (float) (analysis.columns - 1),
                (char) analysis.sample->fundNote);
    }
    WavePeriodFolding::render(*analysis.sample, columns, analysis.sample->size(),
            true, true, 0.5);
    ++analysis.cyclogramRevision;
}

}
