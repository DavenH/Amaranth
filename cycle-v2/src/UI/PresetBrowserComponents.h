#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

#include "UI/PresetLibraryIndex.h"
#include "UI/PresetThumbnailCache.h"

namespace CycleV2 {

namespace PresetBrowserPainting {

void drawPreview(
        juce::Graphics& graphics,
        const PresetLibraryRecord& record,
        PresetThumbnailCache& thumbnails,
        juce::Rectangle<float> bounds);

}

class PresetCardGrid final : public juce::Component {
public:
    using SelectionCallback = std::function<void()>;
    using OpenCallback = std::function<void()>;

    explicit PresetCardGrid(PresetThumbnailCache& thumbnailsToUse) :
            thumbnails(thumbnailsToUse) {
    }

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
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    int columnCount(int width) const;
    int indexAt(juce::Point<int> position) const;
    juce::Rectangle<int> cardBounds(int visibleIndex) const;
    void select(int visibleIndex);

    PresetThumbnailCache& thumbnails;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> indices;
    SelectionCallback onSelection;
    OpenCallback onOpen;
    int selected {};
    int hovered { -1 };
};

class PresetDetailPanel final : public juce::Component {
public:
    explicit PresetDetailPanel(PresetThumbnailCache& thumbnailsToUse) :
            thumbnails(thumbnailsToUse) {
    }

    void setRecord(const PresetLibraryRecord* record);
    void paint(juce::Graphics& graphics) override;

private:
    PresetThumbnailCache& thumbnails;
    PresetLibraryRecord record;
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
