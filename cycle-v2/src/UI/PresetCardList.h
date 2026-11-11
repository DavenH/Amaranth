#pragma once

#include <JuceHeader.h>

#include "App/LibraryFavorites.h"
#include "UI/PresetLibraryIndex.h"
#include "UI/PresetThumbnailCache.h"

namespace CycleV2 {

class PresetCardList final : public juce::Component {
public:
    using Callback = std::function<void()>;
    explicit PresetCardList(PresetThumbnailCache& thumbnails);
    void setResults(const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices, std::vector<juce::StringArray> visibleTags);
    void setCallbacks(Callback openCallback, std::function<void(const juce::File&)> favoriteCallback);
    void setFavorites(LibraryFavorites* store);
    const PresetLibraryRecord* selectedRecord() const;
    int count() const;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> pointerTargetsForAutomation(
            const juce::Rectangle<int>& viewportBounds, int viewPositionY) const;
    void moveSelection(int delta);
    void paint(juce::Graphics& graphics) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    Callback onSelectionChanged;
    std::function<void(const juce::File&, const juce::String&)> onDelete;

private:
    juce::Rectangle<int> rowBounds(int index) const;
    int indexAt(juce::Point<int> position) const;
    void select(int index);
    void updateHeight();
    void paintRow(juce::Graphics& graphics, int visibleIndex, juce::Rectangle<float> bounds);

    PresetThumbnailCache& thumbnails;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> indices;
    std::vector<juce::StringArray> tags;
    Callback onOpen;
    std::function<void(const juce::File&)> onFavorite;
    LibraryFavorites* favorites {};
    int selected { -1 };
    int hovered { -1 };
};

}
