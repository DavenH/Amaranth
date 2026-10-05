#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "App/LibraryFavorites.h"
#include "Graph/PatternLibrary.h"
#include "UI/LibrarySearchField.h"
#include "UI/SidebarTagCloud.h"

namespace CycleV2 {

class PatternBrowser final : public juce::Component {
public:
    using SelectCallback = std::function<void(const juce::String&)>;
    using EditCallback = std::function<void(const juce::String&)>;
    using CreateCallback = std::function<void(
            const juce::String&, const juce::StringArray&)>;

    PatternBrowser(SelectCallback select, EditCallback edit, CreateCallback create,
            LibraryFavorites* favorites = nullptr);
    ~PatternBrowser() override;
    void setRecords(std::vector<PatternRecord> records, const juce::String& selectedId);
    void setPlaybackToggleCallback(std::function<void()> callback);
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;
    void resized() override;

private:
    class List;
    void editSelected();
    void applyFilter();
    void createPattern();
    void toggleFavorite(const juce::String& id);

    EditCallback onEdit;
    CreateCallback onCreate;
    LibraryFavorites* favorites {};
    LibrarySearchField search { "Search patterns..." };
    juce::TextButton createButton { "+ NEW" };
    juce::TextButton editButton { "EDIT" };
    juce::Label tagHeading;
    SidebarTagCloud tagCloud;
    juce::TextButton favoritesOnly { "Favorites" };
    juce::Viewport viewport;
    std::unique_ptr<List> list;
    std::vector<PatternRecord> allRecords;
    juce::String selectedId;
};

}
