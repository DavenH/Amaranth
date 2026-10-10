#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "App/LibraryFavorites.h"
#include "UI/PresetBrowserLookAndFeel.h"
#include "UI/PresetLibraryIndex.h"
#include "UI/PresetThumbnailCache.h"
#include "UI/PatternBrowser.h"
#include "UI/SidebarLibraryToolbar.h"
#include "UI/SidebarTagCloud.h"

namespace CycleV2 {

enum class WorkspaceSidebarTab {
    Curves,
    Presets,
    Patterns,
    Nodes
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
    using PatternSelectCallback = PatternBrowser::SelectCallback;
    using PatternEditCallback = PatternBrowser::EditCallback;
    using PatternCreateCallback = PatternBrowser::CreateCallback;
    using PatternRenameCallback = PatternBrowser::RenameCallback;
    using PatternDeleteCallback = PatternBrowser::DeleteCallback;
    using TitleChangedCallback = std::function<void(
            const juce::File&, const juce::String&)>;
    using TagsChangedCallback = std::function<void(
            const juce::File&, const juce::StringArray&)>;

    InlinePresetBrowser(
            std::vector<juce::File> directories,
            OpenCallback openCallback,
            ActionCallback browseCallback,
            TabCallback tabCallback,
            DeleteCallback deleteCallback = {},
            ConfirmDeleteCallback confirmDeleteCallback = {},
            ActionCallback createCallback = {},
            LibraryFavorites* favorites = nullptr);
    ~InlinePresetBrowser() override;

    void setActiveTab(WorkspaceSidebarTab tab);
    void configurePatterns(PatternSelectCallback select,
            PatternEditCallback edit, PatternCreateCallback create,
            PatternRenameCallback rename, PatternDeleteCallback remove);
    void setPatterns(std::vector<PatternRecord> records,
            const juce::String& selectedId);
    WorkspaceSidebarTab activeTab() const { return tab; }
    int visiblePresetCount() const;
    void refreshIndex();
    void refreshRecord(const juce::File& file);
    void refreshFavorites();
    void setPlaybackToggleCallback(ActionCallback callback);
    void setMetadataChangedCallbacks(
            TitleChangedCallback titleCallback,
            TagsChangedCallback tagsCallback);
    static juce::String deleteConfirmationMessage(const juce::String& presetName);
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;

    std::function<bool(juce::Point<int>)> nodePaletteHitTest;

    bool hitTest(int x, int y) override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class CompactList;

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void receiveResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices);
    void applyTagFilter();
    void toggleFavorite(const juce::File& file);
    juce::StringArray tagsFor(const PresetLibraryRecord& record) const;
    void updateAvailableTags();
    void openSelected();
    void editSelectedTags();
    void toggleSelectedTag(const juce::String& tag);
    void updateSelectedTags();
    void renameSelected();
    void requestDeleteSelected();
    void deletePreset(const juce::File& file);
    void updateVisibility();
    void styleTabButton(juce::TextButton& button);

    OpenCallback onOpen;
    ActionCallback onBrowse;
    ActionCallback onCreate;
    ActionCallback onTogglePlayback;
    TitleChangedCallback onTitleChanged;
    TagsChangedCallback onTagsChanged;
    TabCallback onTabChanged;
    DeleteCallback onDelete;
    ConfirmDeleteCallback onConfirmDelete;
    LibraryFavorites* favorites {};
    PresetBrowserLookAndFeel lookAndFeel;
    PresetThumbnailCache thumbnails;
    juce::TextButton curves { "CURVES" };
    juce::TextButton presets { "PRESETS" };
    juce::TextButton patterns { "PATTERNS" };
    juce::TextButton nodes { "NODES" };
    std::unique_ptr<PatternBrowser> patternBrowser;
    SidebarLibraryToolbar toolbar { "Search presets..." };
    juce::Label tagHeading;
    SidebarTagCloud tagCloud;
    juce::Viewport viewport;
    std::unique_ptr<CompactList> list;
    juce::TextButton browse { "BROWSE FILES..." };
    std::unique_ptr<PresetLibraryIndex> index;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> searchResults;
    std::map<std::string, juce::StringArray> patternTags;
    juce::StringArray knownTags;
    WorkspaceSidebarTab tab { WorkspaceSidebarTab::Presets };
};

}
