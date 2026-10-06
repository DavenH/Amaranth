#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "App/LibraryFavorites.h"
#include "Graph/PatternLibrary.h"
#include "UI/SidebarLibraryToolbar.h"
#include "UI/SidebarTagCloud.h"

namespace CycleV2 {

class PatternBrowser final : public juce::Component {
public:
    using SelectCallback = std::function<void(const juce::String&)>;
    using EditCallback = std::function<void(const juce::String&)>;
    using CreateCallback = std::function<void(
            const juce::String&, const juce::StringArray&)>;
    using RenameCallback = std::function<void(
            const juce::String&, const juce::String&)>;
    using DeleteCallback = std::function<void(const juce::String&)>;

    PatternBrowser(SelectCallback select, EditCallback edit, CreateCallback create,
            LibraryFavorites* favorites = nullptr,
            RenameCallback rename = {}, DeleteCallback remove = {});
    ~PatternBrowser() override;
    void setRecords(std::vector<PatternRecord> records, const juce::String& selectedId);
    void setPlaybackToggleCallback(std::function<void()> callback);
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;
    void resized() override;

private:
    class List;
    void editSelected();
    void renameSelected();
    void deleteSelected();
    const PatternRecord* selectedRecord() const;
    void updateActions();
    void applyFilter();
    void createPattern(const PatternRecord* source = nullptr);
    void toggleFavorite(const juce::String& id);

    EditCallback onEdit;
    CreateCallback onCreate;
    RenameCallback onRename;
    DeleteCallback onDelete;
    LibraryFavorites* favorites {};
    SidebarLibraryToolbar toolbar { "Search patterns..." };
    juce::Label tagHeading;
    SidebarTagCloud tagCloud;
    juce::Viewport viewport;
    std::unique_ptr<List> list;
    std::vector<PatternRecord> allRecords;
    juce::String selectedId;
};

}
