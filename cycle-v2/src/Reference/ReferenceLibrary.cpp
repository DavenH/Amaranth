#include <algorithm>

#include "Reference/ReferenceLibrary.h"

namespace CycleV2 {

namespace {

constexpr int kMaximumHistory = 64;

juce::File selectionFile(const juce::File& directory) {
    return directory.getChildFile("library.json");
}

}

ReferenceLibrary::ReferenceLibrary(
        juce::File factoryDirectoryToUse,
        juce::File userDirectoryToUse) :
        factoryDirectory(std::move(factoryDirectoryToUse))
    ,   userDirectory(std::move(userDirectoryToUse)) {
    reload();
}

bool ReferenceLibrary::supportedAudioFile(const juce::File& file) {
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
            || extension == ".mp3" || extension == ".flac" || extension == ".ogg";
}

std::optional<ReferenceRecord> ReferenceLibrary::readRecord(
        const juce::File& metadata,
        const juce::File& audioFile,
        bool factory) {
    if (!metadata.existsAsFile()) {
        return std::nullopt;
    }
    const juce::var data = juce::JSON::parse(metadata.loadFileAsString());
    const auto* object = data.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != 1) {
        return std::nullopt;
    }

    ReferenceRecord record;
    record.id = object->getProperty("id").toString();
    record.name = object->getProperty("name").toString();
    record.audioFile = audioFile;
    record.factory = factory;
    record.editedFundamental = (int) object->getProperty("fundamental");
    const juce::var pathValue = object->getProperty("pitchPath");
    if (const auto* path = pathValue.getArray()) {
        for (const auto& item : *path) {
            const auto* point = item.getDynamicObject();
            if (point != nullptr) {
                record.editedPitchPath.emplace_back(
                        (float) point->getProperty("time"),
                        (float) point->getProperty("pitch"));
            }
        }
    }
    if (record.id.isEmpty() || !record.audioFile.existsAsFile()) {
        return std::nullopt;
    }
    return record;
}

juce::var ReferenceLibrary::writeRecord(const ReferenceRecord& record) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("version", 1);
    object->setProperty("id", record.id);
    object->setProperty("name", record.name);
    object->setProperty("file", record.audioFile.getFileName());
    object->setProperty("fundamental", record.editedFundamental);
    juce::Array<juce::var> path;
    for (const auto& point : record.editedPitchPath) {
        auto encoded = std::make_unique<juce::DynamicObject>();
        encoded->setProperty("time", point.x);
        encoded->setProperty("pitch", point.y);
        path.add(juce::var(encoded.release()));
    }
    object->setProperty("pitchPath", std::move(path));
    return { object.release() };
}

juce::File ReferenceLibrary::metadataFile(const ReferenceRecord& record) const {
    return userDirectory.getChildFile(record.id.replaceCharacter(':', '_') + ".ref.json");
}

void ReferenceLibrary::saveRecord(const ReferenceRecord& record) const {
    userDirectory.createDirectory();
    metadataFile(record).replaceWithText(juce::JSON::toString(writeRecord(record)));
}

void ReferenceLibrary::saveSelection() const {
    userDirectory.createDirectory();
    auto data = std::make_unique<juce::DynamicObject>();
    data->setProperty("selected", currentId);
    juce::Array<juce::var> open;
    for (const auto& reference : openReferences) {
        open.add(reference.id);
    }
    data->setProperty("open", std::move(open));
    selectionFile(userDirectory).replaceWithText(
            juce::JSON::toString(juce::var(data.release())));
}

void ReferenceLibrary::reload() {
    entries.clear();
    openReferences.clear();
    undoStack.clear();
    redoStack.clear();

    for (const auto& file : factoryDirectory.findChildFiles(
            juce::File::findFiles, false)) {
        if (!supportedAudioFile(file)) {
            continue;
        }
        ReferenceRecord record;
        record.id = "factory:" + file.getFileNameWithoutExtension();
        record.name = file.getFileNameWithoutExtension();
        record.audioFile = file;
        record.factory = true;
        if (auto override = readRecord(metadataFile(record), file, true)) {
            record = std::move(*override);
        }
        entries.push_back(std::move(record));
    }

    for (const auto& metadata : userDirectory.findChildFiles(
            juce::File::findFiles, false, "*.ref.json")) {
        if (metadata.getFileName().startsWith("factory_")) {
            continue;
        }
        const juce::var data = juce::JSON::parse(metadata.loadFileAsString());
        const auto* object = data.getDynamicObject();
        if (object == nullptr) {
            continue;
        }
        const auto file = userDirectory.getChildFile(object->getProperty("file").toString());
        if (auto record = readRecord(metadata, file, false)) {
            entries.push_back(std::move(*record));
        }
    }

    const juce::var state = juce::JSON::parse(selectionFile(userDirectory).loadFileAsString());
    const juce::String selected = state.getProperty("selected", {}).toString();
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        if (left.factory != right.factory) {
            return left.factory;
        }
        return left.name.compareIgnoreCase(right.name) < 0;
    });
    const juce::var openState = state.getProperty("open", {});
    if (const auto* open = openState.getArray()) {
        for (const auto& item : *open) {
            juce::String error;
            select(item.toString(), error);
        }
    } else if (selected.isNotEmpty()) {
        juce::String error;
        select(selected, error);
    }
    if (analysisFor(selected) != nullptr) {
        currentId = selected;
    }
    saveSelection();
}

const ReferenceRecord* ReferenceLibrary::selectedRecord() const {
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.id == currentId;
    });
    return found != entries.end() ? &*found : nullptr;
}

const ReferenceSampleAnalysis* ReferenceLibrary::selectedAnalysis() const {
    return analysisFor(currentId);
}

const ReferenceSampleAnalysis* ReferenceLibrary::analysisFor(const juce::String& id) const {
    const auto found = std::find_if(openReferences.begin(), openReferences.end(),
            [&](const auto& reference) { return reference.id == id; });
    return found != openReferences.end() ? &found->analysis : nullptr;
}

std::vector<juce::String> ReferenceLibrary::openReferenceIds() const {
    std::vector<juce::String> ids;
    ids.reserve(openReferences.size());
    for (const auto& reference : openReferences) {
        ids.push_back(reference.id);
    }
    return ids;
}

bool ReferenceLibrary::select(const juce::String& id, juce::String& error) {
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.id == id;
    });
    if (found == entries.end()) {
        error = "Reference is unavailable";
        return false;
    }
    if (analysisFor(id) != nullptr) {
        if (currentId != id) {
            currentId = id;
            undoStack.clear();
            redoStack.clear();
            saveSelection();
        }
        return true;
    }

    auto analysis = ReferenceSampleAnalyzer::analyze(found->audioFile);
    if (!analysis.succeeded()) {
        error = analysis.error;
        return false;
    }
    if (found->editedFundamental >= 0) {
        analysis.sample->fundNote = found->editedFundamental;
        if (found->editedPitchPath.size() >= 2) {
            analysis.pitchPath = found->editedPitchPath;
        }
        ReferenceSampleAnalyzer::refreshCyclogram(analysis);
    }

    currentId = id;
    openReferences.push_back({ id, std::move(analysis) });
    undoStack.clear();
    redoStack.clear();
    saveSelection();
    return true;
}

bool ReferenceLibrary::close(const juce::String& id) {
    const auto found = std::find_if(openReferences.begin(), openReferences.end(),
            [&](const auto& reference) { return reference.id == id; });
    if (found == openReferences.end()) {
        return false;
    }
    openReferences.erase(found);
    if (currentId == id) {
        currentId = openReferences.empty() ? juce::String() : openReferences.front().id;
        undoStack.clear();
        redoStack.clear();
    }
    saveSelection();
    return true;
}

bool ReferenceLibrary::importFile(const juce::File& source, juce::String& error) {
    if (!supportedAudioFile(source)) {
        error = "Choose WAV, AIFF, MP3, FLAC, or Ogg audio";
        return false;
    }
    auto analysis = ReferenceSampleAnalyzer::analyze(source);
    if (!analysis.succeeded()) {
        error = analysis.error;
        return false;
    }

    userDirectory.createDirectory();
    const juce::String id = juce::Uuid().toString();
    const juce::File destination = userDirectory.getChildFile(id + source.getFileExtension());
    if (!source.copyFileTo(destination)) {
        error = "Reference could not be copied to the library";
        return false;
    }
    ReferenceRecord record;
    record.id = id;
    record.name = source.getFileNameWithoutExtension();
    record.audioFile = destination;
    record.editedFundamental = analysis.sample->fundNote;
    record.editedPitchPath = analysis.pitchPath;
    analysis.sample->lastLoadedFilePath = destination.getFullPathName();
    saveRecord(record);
    entries.push_back(std::move(record));
    currentId = id;
    openReferences.push_back({ id, std::move(analysis) });
    undoStack.clear();
    redoStack.clear();
    saveSelection();
    return true;
}

ReferenceLibrary::PitchEdit ReferenceLibrary::currentPitchEdit() const {
    const auto* analysis = selectedAnalysis();
    return analysis != nullptr
            ? PitchEdit { analysis->sample->fundNote, analysis->pitchPath }
            : PitchEdit {};
}

bool ReferenceLibrary::applyPitchEdit(PitchEdit edit, bool recordUndo) {
    const auto found = std::find_if(openReferences.begin(), openReferences.end(),
            [&](const auto& reference) { return reference.id == currentId; });
    if (found == openReferences.end() || edit.note < 0 || edit.note > 127
            || edit.path.size() < 2) {
        return false;
    }
    if (recordUndo) {
        undoStack.push_back(currentPitchEdit());
        if (undoStack.size() > kMaximumHistory) {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear();
    }
    found->analysis.sample->fundNote = edit.note;
    found->analysis.pitchPath = std::move(edit.path);
    ReferenceSampleAnalyzer::refreshCyclogram(found->analysis);
    const auto record = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.id == currentId;
    });
    if (record != entries.end()) {
        record->editedFundamental = found->analysis.sample->fundNote;
        record->editedPitchPath = found->analysis.pitchPath;
        saveRecord(*record);
    }
    return true;
}

bool ReferenceLibrary::setFundamental(int midiNote) {
    const auto* analysis = selectedAnalysis();
    if (analysis == nullptr || midiNote == analysis->sample->fundNote) {
        return false;
    }
    PitchEdit edit = currentPitchEdit();
    edit.note = midiNote;
    return applyPitchEdit(std::move(edit), true);
}

bool ReferenceLibrary::movePitchPoint(size_t index, juce::Point<float> position) {
    PitchEdit edit = currentPitchEdit();
    if (index >= edit.path.size()) {
        return false;
    }
    const float left = index == 0 ? 0.f : edit.path[index - 1].x + 0.0001f;
    const float right = index + 1 == edit.path.size()
            ? 1.f : edit.path[index + 1].x - 0.0001f;
    edit.path[index].x = juce::jlimit(left, right, position.x);
    edit.path[index].y = juce::jlimit(0.f, 1.f, position.y);
    return applyPitchEdit(std::move(edit), true);
}

bool ReferenceLibrary::replacePitchPath(std::vector<Vertex2> points) {
    if (points.size() < 2) {
        return false;
    }
    for (size_t index = 0; index < points.size(); ++index) {
        if (points[index].x < 0.f || points[index].x > 1.f
                || points[index].y < 0.f || points[index].y > 1.f
                || (index > 0 && points[index].x < points[index - 1].x)) {
            return false;
        }
    }
    PitchEdit edit = currentPitchEdit();
    if (edit.path == points) {
        return false;
    }
    edit.path = std::move(points);
    return applyPitchEdit(std::move(edit), true);
}

bool ReferenceLibrary::undoPitchEdit() {
    if (undoStack.empty()) {
        return false;
    }
    redoStack.push_back(currentPitchEdit());
    PitchEdit edit = std::move(undoStack.back());
    undoStack.pop_back();
    return applyPitchEdit(std::move(edit), false);
}

bool ReferenceLibrary::redoPitchEdit() {
    if (redoStack.empty()) {
        return false;
    }
    undoStack.push_back(currentPitchEdit());
    PitchEdit edit = std::move(redoStack.back());
    redoStack.pop_back();
    return applyPitchEdit(std::move(edit), false);
}

}
