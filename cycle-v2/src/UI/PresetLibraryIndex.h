#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

struct PresetLibraryRecord {
    juce::File file;
    juce::String name;
    PresetPresentation presentation;
    juce::String searchText;
    juce::int64 modificationTime {};
};

class PresetLibraryIndex final : private juce::Timer {
public:
    using ResultsCallback = std::function<void(
            const std::vector<PresetLibraryRecord>&,
            const std::vector<int>&)>;

    PresetLibraryIndex(
            std::vector<juce::File> directories,
            ResultsCallback resultsCallback);
    ~PresetLibraryIndex() override;

    void start();
    void setQuery(const juce::String& query);
    uint64_t publishedGeneration() const { return published; }

private:
    class ScanJob;
    class FilterJob;

    void timerCallback() override;
    void publishScan(uint64_t generation, std::vector<PresetLibraryRecord> records);
    void publishFilter(uint64_t generation, std::vector<int> indices);
    void scheduleFilter();

    std::vector<juce::File> directories;
    std::vector<PresetLibraryRecord> records;
    ResultsCallback callback;
    juce::ThreadPool worker { 1 };
    juce::String pendingQuery;
    std::atomic<uint64_t> requested { 0 };
    uint64_t published {};
    bool scanComplete {};

    JUCE_DECLARE_WEAK_REFERENCEABLE(PresetLibraryIndex)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryIndex)
};

}
