#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "UI/PresetBrowserLookAndFeel.h"
#include "UI/PresetLibraryIndex.h"
#include "UI/PresetThumbnailCache.h"

namespace CycleV2 {

enum class WorkspaceSidebarTab {
    Curves,
    Presets
};

class InlinePresetBrowser final :
        public juce::Component
    ,   private juce::TextEditor::Listener
    ,   private juce::KeyListener {
public:
    using OpenCallback = std::function<bool(const juce::File&)>;
    using ActionCallback = std::function<void()>;
    using TabCallback = std::function<void(WorkspaceSidebarTab)>;
    using DeleteCallback = std::function<bool(const juce::File&)>;
    using ConfirmDeleteCallback = std::function<void(
            const juce::String&,
            std::function<void(bool)>)>;

    InlinePresetBrowser(
            std::vector<juce::File> directories,
            OpenCallback openCallback,
            ActionCallback browseCallback,
            ActionCallback newGuideCallback,
            TabCallback tabCallback,
            DeleteCallback deleteCallback = {},
            ConfirmDeleteCallback confirmDeleteCallback = {});
    ~InlinePresetBrowser() override;

    void setActiveTab(WorkspaceSidebarTab tab);
    WorkspaceSidebarTab activeTab() const { return tab; }
    int visiblePresetCount() const;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;

    bool hitTest(int x, int y) override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class CompactList;
    class SelectedPresetCard;

    enum class PackFilter {
        All,
        Factory,
        User
    };

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void receiveResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices);
    void applyPackFilter();
    void setPackFilter(PackFilter filter);
    void updateSelectedPreview();
    void openSelected();
    void requestDeleteSelected();
    void deletePreset(const juce::File& file);
    void updateVisibility();
    void styleTabButton(juce::TextButton& button);
    void styleFilterButton(juce::TextButton& button);

    OpenCallback onOpen;
    ActionCallback onBrowse;
    ActionCallback onNewGuide;
    TabCallback onTabChanged;
    DeleteCallback onDelete;
    ConfirmDeleteCallback onConfirmDelete;
    PresetBrowserLookAndFeel lookAndFeel;
    PresetThumbnailCache thumbnails;
    juce::TextButton curves { "CURVES" };
    juce::TextButton presets { "PRESETS" };
    juce::TextButton addGuide { "+" };
    juce::TextEditor search;
    juce::TextButton all { "ALL" };
    juce::TextButton factory { "FACTORY" };
    juce::TextButton user { "USER" };
    juce::Viewport viewport;
    std::unique_ptr<SelectedPresetCard> selectedPreview;
    std::unique_ptr<CompactList> list;
    juce::Label status;
    juce::TextButton browse { "BROWSE FILES..." };
    std::unique_ptr<PresetLibraryIndex> index;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> searchResults;
    WorkspaceSidebarTab tab { WorkspaceSidebarTab::Presets };
    PackFilter packFilter { PackFilter::All };
};

}
