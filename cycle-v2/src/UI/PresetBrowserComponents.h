#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>
#include <vector>

#include "UI/PresetLibraryIndex.h"

namespace CycleV2 {

class PresetCardGrid final : public juce::Component {
public:
    using SelectionCallback = std::function<void()>;
    using OpenCallback = std::function<void()>;

    void setResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices);
    void setCallbacks(SelectionCallback selectionCallback, OpenCallback openCallback);
    void moveSelection(int columnDelta, int rowDelta);
    const PresetLibraryRecord* selectedRecord() const;
    int visibleCount() const { return (int) indices.size(); }
    int selectedVisibleIndex() const { return selected; }
    int contentHeightForWidth(int width) const;

    void paint(juce::Graphics& graphics) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    struct CachedPreview {
        juce::int64 modificationTime {};
        juce::Image image;
    };

    int columnCount(int width) const;
    int indexAt(juce::Point<int> position) const;
    juce::Rectangle<int> cardBounds(int visibleIndex) const;
    const juce::Image& previewFor(int recordIndex);
    void select(int visibleIndex);

    std::vector<PresetLibraryRecord> library;
    std::vector<int> indices;
    std::map<juce::String, CachedPreview> previewCache;
    SelectionCallback onSelection;
    OpenCallback onOpen;
    int selected {};
};

class PresetDetailPanel final : public juce::Component {
public:
    void setRecord(const PresetLibraryRecord* record);
    void paint(juce::Graphics& graphics) override;

private:
    PresetLibraryRecord record;
    juce::Image preview;
    bool hasRecord {};
};

class PresetBrowserSidebar final : public juce::Component {
public:
    void setRecords(const std::vector<PresetLibraryRecord>& records);
    void paint(juce::Graphics& graphics) override;

private:
    int factoryCount {};
    int userCount {};
    juce::StringArray tags;
};

}
