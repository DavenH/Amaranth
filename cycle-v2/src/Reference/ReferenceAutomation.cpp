#include <string>
#include <unordered_set>

#include <Util/NumberUtils.h>

#include "Reference/ReferenceAutomation.h"

namespace CycleV2::ReferenceAutomation {

namespace {

juce::var recordValue(const ReferenceRecord& record) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("referenceId", record.id);
    object->setProperty("name", record.name);
    object->setProperty("audioPath", record.audioFile.getFullPathName());
    object->setProperty("factory", record.factory);
    return { object.release() };
}

}

juce::var catalogue(const ReferenceLibrary& library) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("schemaVersion", 1);
    object->setProperty("selectedReferenceId", library.selectedId());
    juce::Array<juce::var> records;
    std::unordered_set<std::string> openIds;
    for (const auto& id : library.openReferenceIds()) {
        openIds.insert(id.toStdString());
    }
    for (const auto& record : library.records()) {
        auto value = recordValue(record);
        value.getDynamicObject()->setProperty("open",
                openIds.find(record.id.toStdString()) != openIds.end());
        records.add(std::move(value));
    }
    object->setProperty("references", std::move(records));
    return { object.release() };
}

juce::var selectedAnalysis(const ReferenceLibrary& library, bool includeGrid) {
    const auto* record = library.selectedRecord();
    const auto* analysis = library.selectedAnalysis();
    if (record == nullptr || analysis == nullptr || !analysis->succeeded()) {
        return {};
    }

    auto result = recordValue(*record);
    auto* object = result.getDynamicObject();
    const auto& sample = *analysis->sample;
    const double sampleRate = sample.samplerate;
    const double duration = sample.size() / sampleRate;
    object->setProperty("schemaVersion", 1);
    object->setProperty("sampleRate", sample.samplerate);
    object->setProperty("sampleCount", sample.size());
    object->setProperty("channels", sample.audio.numChannels);
    object->setProperty("durationSeconds", duration);
    object->setProperty("effectiveMidiNote", sample.fundNote);
    object->setProperty("trackingInsecurity", analysis->pitchInsecurity);
    object->setProperty("lowConfidence", analysis->lowConfidence);
    object->setProperty("trackingUsedFallback", analysis->pitchInsecurity > 50.f);
    object->setProperty("amplitudeNormalization", "PitchedSample peak normalized");
    object->setProperty("pitchState", "effective path including library edits");

    juce::Array<juce::var> path;
    for (const auto& point : analysis->pitchPath) {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("time", point.x);
        item->setProperty("pitch", point.y);
        item->setProperty("timeSeconds", point.x * duration);
        item->setProperty("midiNote",
                sample.fundNote + NumberUtils::unitPitchToSemis(point.y));
        path.add(juce::var(item.release()));
    }
    object->setProperty("pitchPath", std::move(path));

    juce::Array<juce::var> frames;
    for (const auto& frame : sample.periods) {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("sampleOffset", frame.sampleOffset);
        item->setProperty("timeSeconds", frame.sampleOffset / sampleRate);
        item->setProperty("periodSamples", frame.period);
        item->setProperty("frequencyHz", frame.period > 0.f ? sampleRate / frame.period : 0.0);
        frames.add(juce::var(item.release()));
    }
    object->setProperty("pitchFrames", std::move(frames));

    auto grid = std::make_unique<juce::DynamicObject>();
    grid->setProperty("columns", analysis->columns);
    grid->setProperty("rows", analysis->rows);
    grid->setProperty("layout", "column-major");
    grid->setProperty("timeAxis", "linear normalized recording duration [0, 1]");
    grid->setProperty("phaseAxis", "normalized cycle [0, 1)");
    grid->setProperty("foldingGain", 0.5);
    grid->setProperty("revision", (juce::int64) analysis->cyclogramRevision);
    if (includeGrid) {
        juce::Array<juce::var> values;
        values.ensureStorageAllocated((int) analysis->cyclogram.size());
        for (const float value : analysis->cyclogram) {
            values.add(value);
        }
        grid->setProperty("values", std::move(values));
    }
    object->setProperty("cyclogram", juce::var(grid.release()));
    return result;
}

}
