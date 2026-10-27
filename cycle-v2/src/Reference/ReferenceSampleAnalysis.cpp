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
        int maximumColumns,
        int maximumRows) {
    ReferenceSampleAnalysis result;
    result.maximumColumns = juce::jmin(1024, maximumColumns);
    result.maximumRows = juce::jmin(1024, maximumRows);
    if (maximumColumns < 2 || maximumRows < 4 || !file.existsAsFile()) {
        result.error = "Reference file is missing or the surface size is invalid";
        return result;
    }

    auto sample = std::make_unique<PitchedSample>();
    if (sample->load(file.getFullPathName(), 5.0) != 0) {
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
    if (analysis.sample == nullptr) {
        return;
    }

    analysis.sample->createPeriodsFromPitchPath(analysis.pitchPath);
    const auto& sample = *analysis.sample;
    std::vector<const PitchFrame*> cycles;
    int longestCycle = 4;
    for (const auto& frame : sample.periods) {
        if (frame.period >= 10.f
                && frame.sampleOffset + juce::roundToInt(frame.period) <= sample.size()) {
            cycles.push_back(&frame);
            longestCycle = juce::jmax(longestCycle, juce::roundToInt(frame.period));
        }
    }
    if (cycles.size() < 2) {
        return;
    }
    analysis.columns = juce::jmin(analysis.maximumColumns, (int) cycles.size());
    analysis.rows = juce::jmin(analysis.maximumRows, NumberUtils::nextPower2(longestCycle));
    analysis.cyclePositions.resize((size_t) analysis.columns);
    analysis.cyclogram.resize((size_t) analysis.columns * (size_t) analysis.rows);
    std::vector<Column> columns;
    columns.reserve((size_t) analysis.columns);
    for (int index = 0; index < analysis.columns; ++index) {
        const size_t cycle = (size_t) index * cycles.size() / (size_t) analysis.columns;
        const float position = (float) cycles[cycle]->sampleOffset / (float) sample.size();
        analysis.cyclePositions[(size_t) index] = position;
        columns.emplace_back(
                analysis.cyclogram.data() + (size_t) index * (size_t) analysis.rows,
                analysis.rows,
                position,
                (char) analysis.sample->fundNote);
    }
    WavePeriodFolding::render(*analysis.sample, columns, analysis.sample->size(),
            true, true, 0.5, false, true);
    ++analysis.cyclogramRevision;
}

}
