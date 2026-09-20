#include "UI/PresetLibraryIndex.h"

#include <algorithm>

namespace CycleV2 {

namespace {

PresetLibraryRecord skeletonRecord(
        const juce::File& file,
        const juce::String& fallbackPack) {
    PresetLibraryRecord record;
    record.file = file;
    record.name = file.getFileNameWithoutExtension();
    record.modificationTime = file.getLastModificationTime().toMilliseconds();
    record.presentation.pack = fallbackPack;
    record.searchText = (record.name + " " + fallbackPack).toLowerCase();
    return record;
}

PresetLibraryRecord readRecord(const PresetLibraryRecord& skeleton) {
    PresetLibraryRecord record = skeleton;

    juce::var root;
    if (juce::JSON::parse(record.file.loadFileAsString(), root).wasOk()) {
        if (const auto* object = root.getDynamicObject()) {
            record.presentation = PresetPresentationCodec::readMetadataJSON(
                    object->getProperty("presetPresentation")).presentation;
        }
    }
    if (record.presentation.pack.isEmpty()) {
        record.presentation.pack = skeleton.presentation.pack;
    }

    juce::StringArray searchTerms {
            record.name,
            record.presentation.author,
            record.presentation.pack
    };
    searchTerms.addArray(record.presentation.tags);
    record.searchText = searchTerms.joinIntoString(" ").toLowerCase();
    record.metadataReady = true;
    return record;
}

std::vector<int> matchingIndices(
        const std::vector<juce::String>& searchTexts,
        const juce::String& query) {
    std::vector<int> indices;
    const juce::String normalized = query.trim().toLowerCase();
    for (int index = 0; index < (int) searchTexts.size(); ++index) {
        if (normalized.isEmpty()
                || searchTexts[(size_t) index].contains(normalized)) {
            indices.push_back(index);
        }
    }
    return indices;
}

std::vector<juce::String> searchTextsFor(
        const std::vector<PresetLibraryRecord>& records) {
    std::vector<juce::String> searchTexts;
    searchTexts.reserve(records.size());
    for (const auto& record : records) {
        searchTexts.push_back(record.searchText);
    }
    return searchTexts;
}

}

class PresetLibraryIndex::ScanJob final : public juce::ThreadPoolJob {
public:
    ScanJob(
            PresetLibraryIndex& ownerToUse,
            std::vector<juce::File> directoriesToScan,
            uint64_t generationToPublish) :
            ThreadPoolJob("Cycle V2 preset scan")
        ,   owner(&ownerToUse)
        ,   directories(std::move(directoriesToScan))
        ,   generation(generationToPublish) {
    }

    JobStatus runJob() override {
        std::vector<PresetLibraryRecord> found;
        for (int directoryIndex = 0; directoryIndex < (int) directories.size(); ++directoryIndex) {
            if (shouldExit()) {
                return jobHasFinished;
            }
            juce::Array<juce::File> files;
            directories[(size_t) directoryIndex].findChildFiles(
                    files, juce::File::findFiles, false, "*.cyclegraph");
            const juce::String fallbackPack = directoryIndex == 0 ? "Factory" : "User";
            for (const auto& file : files) {
                const auto duplicate = std::find_if(found.begin(), found.end(), [&](const auto& record) {
                    return record.file == file;
                });
                if (duplicate == found.end()) {
                    found.push_back(skeletonRecord(file, fallbackPack));
                }
            }
        }
        std::sort(found.begin(), found.end(), [](const auto& left, const auto& right) {
            return left.name.compareIgnoreCase(right.name) < 0;
        });

        publish(found);
        for (auto& record : found) {
            if (shouldExit()) {
                return jobHasFinished;
            }
            record = readRecord(record);
        }
        publish(std::move(found));
        return jobHasFinished;
    }

private:
    void publish(std::vector<PresetLibraryRecord> found) {
        const juce::WeakReference<PresetLibraryIndex> safeOwner = owner;
        juce::MessageManager::callAsync([
                safeOwner,
                generationToPublish = generation,
                recordsToPublish = std::move(found)]() mutable {
            if (safeOwner != nullptr) {
                safeOwner->publishScan(
                        generationToPublish,
                        std::move(recordsToPublish));
            }
        });
    }

    juce::WeakReference<PresetLibraryIndex> owner;
    std::vector<juce::File> directories;
    uint64_t generation {};
};

class PresetLibraryIndex::FilterJob final : public juce::ThreadPoolJob {
public:
    FilterJob(
            PresetLibraryIndex& ownerToUse,
            std::vector<juce::String> searchTextsToFilter,
            juce::String queryToUse,
            uint64_t generationToPublish) :
            ThreadPoolJob("Cycle V2 preset filter")
        ,   owner(&ownerToUse)
        ,   searchTexts(std::move(searchTextsToFilter))
        ,   query(std::move(queryToUse))
        ,   generation(generationToPublish) {
    }

    JobStatus runJob() override {
        auto indices = matchingIndices(searchTexts, query);
        const juce::WeakReference<PresetLibraryIndex> safeOwner = owner;
        juce::MessageManager::callAsync([
                safeOwner,
                generationToPublish = generation,
                indicesToPublish = std::move(indices)]() mutable {
            if (safeOwner != nullptr) {
                safeOwner->publishFilter(
                        generationToPublish,
                        std::move(indicesToPublish));
            }
        });
        return jobHasFinished;
    }

private:
    juce::WeakReference<PresetLibraryIndex> owner;
    std::vector<juce::String> searchTexts;
    juce::String query;
    uint64_t generation {};
};

PresetLibraryIndex::PresetLibraryIndex(
        std::vector<juce::File> directoriesToScan,
        ResultsCallback resultsCallback) :
        directories(std::move(directoriesToScan))
    ,   callback(std::move(resultsCallback)) {
}

PresetLibraryIndex::~PresetLibraryIndex() {
    stopTimer();
    worker.removeAllJobs(true, 2000);
}

void PresetLibraryIndex::start() {
    const uint64_t generation = ++requested;
    worker.addJob(new ScanJob(*this, directories, generation), true);
}

void PresetLibraryIndex::setQuery(const juce::String& query) {
    pendingQuery = query;
    ++requested;
    startTimer(70);
}

void PresetLibraryIndex::timerCallback() {
    stopTimer();
    scheduleFilter();
}

void PresetLibraryIndex::publishScan(
        uint64_t generation,
        std::vector<PresetLibraryRecord> scannedRecords) {
    records = std::move(scannedRecords);
    scanComplete = true;
    if (generation > requested.load()) {
        requested = generation;
    }
    const uint64_t currentGeneration = requested.load();
    published = currentGeneration;
    if (callback) {
        callback(records, matchingIndices(searchTextsFor(records), pendingQuery));
    }
}

void PresetLibraryIndex::publishFilter(
        uint64_t generation,
        std::vector<int> indices) {
    if (generation != requested.load() || generation <= published) {
        return;
    }
    published = generation;
    if (callback) {
        callback(records, indices);
    }
}

void PresetLibraryIndex::scheduleFilter() {
    if (!scanComplete) {
        return;
    }
    const uint64_t generation = requested.load();
    worker.addJob(new FilterJob(
            *this,
            searchTextsFor(records),
            pendingQuery,
            generation), true);
}

}
