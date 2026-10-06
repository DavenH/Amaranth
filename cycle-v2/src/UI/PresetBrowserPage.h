#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "App/LibraryFavorites.h"
#include "UI/PresetBrowserComponents.h"
#include "UI/PresetBrowserLookAndFeel.h"
#include "UI/PresetThumbnailCache.h"
#include "UI/LibrarySearchField.h"

namespace CycleV2 {

class PresetBrowserPage final : public juce::Component,
                                private juce::TextEditor::Listener,
                                private juce::KeyListener {
public:
    using OpenCallback = std::function<bool(const juce::File&)>;

    PresetBrowserPage(
            std::vector<juce::File> directories,
            OpenCallback openCallback,
            std::function<void()> browseCallback,
            std::function<void()> closeCallback,
            std::function<void()> playbackToggleCallback,
            std::function<void(const juce::File&, const juce::StringArray&)>
                    tagsChangedCallback,
            LibraryFavorites* favorites = nullptr);
    ~PresetBrowserPage() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void receiveResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices);
    void updateSelection();
    void openSelected();
    void editSelectedTags();
    void applyFavoritesFilter();
    void toggleFavorite(const juce::File& file);

    OpenCallback onOpen;
    std::function<void()> onBrowse;
    std::function<void()> onClose;
    std::function<void()> onTogglePlayback;
    std::function<void(const juce::File&, const juce::StringArray&)> onTagsChanged;
    LibraryFavorites* favorites {};
    PresetBrowserLookAndFeel browserLookAndFeel;
    PresetThumbnailCache thumbnails;
    juce::Label title;
    juce::Label subtitle;
    LibrarySearchField search { "Search presets, authors, packs, or tags" };
    juce::TextButton favoritesOnly { "Favorites" };
    PresetBrowserSidebar sidebar;
    juce::Viewport viewport;
    PresetCardGrid grid;
    PresetDetailPanel detail;
    juce::Label status;
    juce::TextButton browse { "BROWSE FILES" };
    juce::TextButton open { "LOAD PRESET" };
    juce::TextButton editTags { "EDIT TAGS" };
    juce::TextButton close { "CLOSE" };
    std::unique_ptr<PresetLibraryIndex> index;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> searchResults;
};

}
