#include <algorithm>

#include "UI/PatternBrowser.h"
#include "UI/MidiPatternMiniMap.h"
#include "UI/SidebarMediaRow.h"

namespace CycleV2 {

namespace {

juce::StringArray tagsFor(const PatternRecord& record) {
    if (!record.tags.isEmpty()) {
        return record.tags;
    }
    return { record.tag.isNotEmpty() ? record.tag : "Other" };
}

}

class PatternBrowser::List final : public juce::Component {
public:
    List(SelectCallback select, EditCallback edit,
            std::function<void(const juce::String&)> favoriteCallback,
            LibraryFavorites* favoriteStore) :
            onSelect(std::move(select))
        ,   onEdit(std::move(edit))
        ,   onFavorite(std::move(favoriteCallback))
        ,   favorites(favoriteStore) {
        setComponentID("workspace.sidebar.patternList");
    }

    void setRecords(std::vector<PatternRecord> next, const juce::String& id) {
        records = std::move(next);
        selectedId = id;
        setSize(getWidth(), (int) records.size() * SidebarMediaRow::height);
        repaint();
    }

    const juce::String& selection() const { return selectedId; }

    const PatternRecord* selectedRecord() const {
        const auto found = std::find_if(records.begin(), records.end(),
                [this](const PatternRecord& record) {
                    return record.id == selectedId;
                });
        return found == records.end() ? nullptr : &*found;
    }

    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation(
                    const juce::Rectangle<int>& viewportBounds,
                    int viewPositionY) const {
        std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
        for (int index = 0; index < (int) records.size(); ++index) {
            const auto row = juce::Rectangle<float>(
                    (float) viewportBounds.getX(),
                    (float) viewportBounds.getY()
                            + index * SidebarMediaRow::height - viewPositionY,
                    (float) viewportBounds.getWidth(),
                    (float) SidebarMediaRow::height);
            if (row.intersects(viewportBounds.toFloat())) {
                targets.push_back({ "workspace.sidebar.pattern." + records[(size_t) index].id,
                        row.getIntersection(viewportBounds.toFloat()) });
                targets.push_back({ "workspace.sidebar.patternFavorite."
                                + records[(size_t) index].id,
                        SidebarMediaRow::patternFavoriteBounds(row) });
            }
        }
        return targets;
    }

    void paint(juce::Graphics& graphics) override {
        for (int index = 0; index < (int) records.size(); ++index) {
            const auto& record = records[(size_t) index];
            const auto row = getLocalBounds()
                    .withY(index * SidebarMediaRow::height)
                    .withHeight(SidebarMediaRow::height).toFloat();
            const bool selected = record.id == selectedId;
            const auto preview = SidebarMediaRow::paintPatternFrame(
                    graphics, row, selected);
            {
                juce::Graphics::ScopedSaveState save(graphics);
                juce::Path clip;
                clip.addRoundedRectangle(preview, 3.f);
                graphics.reduceClipRegion(clip);
                MidiPatternMiniMap::paintNotes(graphics, record.sequence,
                        preview,
                        MidiPatternMiniMap::pitchRange(record.sequence, 12, 0));
            }
            SidebarMediaRow::paintPatternLabels(graphics, row, record.name,
                    tagsFor(record),
                    favorites != nullptr && favorites->isPatternFavorite(record.id));
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        const int index = event.y / SidebarMediaRow::height;
        if (index < 0 || index >= (int) records.size()) {
            return;
        }
        const auto row = getLocalBounds()
                .withY(index * SidebarMediaRow::height)
                .withHeight(SidebarMediaRow::height).toFloat();
        if (SidebarMediaRow::patternFavoriteBounds(row).contains(event.position)
                && onFavorite) {
            onFavorite(records[(size_t) index].id);
            return;
        }
        selectedId = records[(size_t) index].id;
        repaint();
        onSelect(selectedId);
        if (event.getNumberOfClicks() > 1) {
            onEdit(selectedId);
        }
    }

private:
    SelectCallback onSelect;
    EditCallback onEdit;
    std::function<void(const juce::String&)> onFavorite;
    LibraryFavorites* favorites {};
    std::vector<PatternRecord> records;
    juce::String selectedId;
};

PatternBrowser::PatternBrowser(
        SelectCallback select,
        EditCallback edit,
        CreateCallback create,
        LibraryFavorites* favoriteStore,
        RenameCallback rename,
        DeleteCallback remove) :
        onEdit(std::move(edit))
    ,   onCreate(std::move(create))
    ,   onRename(std::move(rename))
    ,   onDelete(std::move(remove))
    ,   favorites(favoriteStore)
    ,   list(std::make_unique<List>(
                [this, select = std::move(select)](const juce::String& id) {
                    selectedId = id;
                    if (select) {
                        select(id);
                    }
                    updateActions();
                },
                [this](const juce::String&) { editSelected(); },
                [this](const juce::String& id) { toggleFavorite(id); },
                favorites)) {
    setComponentID("workspace.sidebar.patternBrowser");
    auto& search = toolbar.searchField();
    auto& createButton = toolbar.newButton();
    auto& editButton = toolbar.editButton();
    auto& renameButton = toolbar.renameButton();
    auto& deleteButton = toolbar.deleteButton();
    search.setComponentID("workspace.sidebar.patternSearch");
    search.onTextChange = [this] { applyFilter(); };
    createButton.setComponentID("workspace.sidebar.patternNew");
    editButton.setComponentID("workspace.sidebar.patternEdit");
    createButton.setTooltip("Save the current MIDI phrase as a new pattern");
    createButton.onClick = [this] { createPattern(); };
    editButton.onClick = [this] { editSelected(); };
    renameButton.setComponentID("workspace.sidebar.patternRename");
    renameButton.setTooltip("Rename the selected user pattern");
    renameButton.onClick = [this] { renameSelected(); };
    deleteButton.setComponentID("workspace.sidebar.patternDelete");
    deleteButton.setTooltip("Move the selected user pattern to Trash");
    deleteButton.onClick = [this] { deleteSelected(); };
    SidebarTagCloud::styleHeading(tagHeading);
    addAndMakeVisible(tagHeading);
    tagCloud.setComponentID("workspace.sidebar.patternTags");
    tagCloud.setFavoritesAvailable(favorites != nullptr);
    tagCloud.setChangeCallback([this] { applyFilter(); });
    addAndMakeVisible(tagCloud);
    addAndMakeVisible(toolbar);
    viewport.setViewedComponent(list.get(), false);
    viewport.setComponentID("workspace.sidebar.patternViewport");
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    updateActions();
}

PatternBrowser::~PatternBrowser() = default;

void PatternBrowser::setRecords(
        std::vector<PatternRecord> records,
        const juce::String& selectedId) {
    allRecords = std::move(records);
    this->selectedId = selectedId;
    juce::StringArray available;
    for (const auto& record : allRecords) {
        for (const auto& tag : tagsFor(record)) {
            available.addIfNotAlreadyThere(tag);
        }
    }
    tagCloud.setTags(std::move(available));
    applyFilter();
    updateActions();
}

void PatternBrowser::setPlaybackToggleCallback(std::function<void()> callback) {
    toolbar.searchField().setPlaybackToggleCallback(std::move(callback));
}

void PatternBrowser::createPattern(const PatternRecord* source) {
    const juce::String sourceName = source == nullptr
            ? juce::String() : source->name;
    const juce::StringArray initialTags = source == nullptr
            ? tagCloud.selectedTags() : tagsFor(*source);
    auto* prompt = new juce::AlertWindow(
            source == nullptr ? "New pattern" : "Edit a copy",
            source == nullptr
                    ? "Name the MIDI phrase to save in the pattern library."
                    : "Factory patterns are read-only. Name a user copy to edit.",
            juce::MessageBoxIconType::QuestionIcon,
            getTopLevelComponent());
    prompt->addTextEditor("name",
            sourceName.isEmpty() ? juce::String() : sourceName + " Variation",
            "Pattern name");
    prompt->addTextEditor("tags",
            initialTags.isEmpty() ? juce::String("Other")
                    : initialTags.joinIntoString(", "),
            "Tags (comma separated)");
    prompt->addButton(source == nullptr ? "Create" : "Create copy", 1,
            juce::KeyPress(juce::KeyPress::returnKey));
    prompt->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    prompt->enterModalState(true,
            juce::ModalCallbackFunction::create([
                    safeThis = juce::Component::SafePointer<PatternBrowser>(this),
                    prompt](int result) {
                if (result != 1 || safeThis == nullptr) {
                    return;
                }
                const auto name = prompt->getTextEditorContents("name").trim();
                if (name.isEmpty()) {
                    return;
                }
                juce::StringArray tags;
                tags.addTokens(prompt->getTextEditorContents("tags"), ",", "\"");
                tags.trim();
                tags.removeEmptyStrings();
                tags.removeDuplicates(true);
                if (tags.isEmpty()) {
                    tags.add("Other");
                }
                safeThis->onCreate(name, tags);
            }), true);
}

const PatternRecord* PatternBrowser::selectedRecord() const {
    return list->selectedRecord();
}

void PatternBrowser::updateActions() {
    const auto* record = selectedRecord();
    toolbar.editButton().setEnabled(record != nullptr);
    const bool userPattern = record != nullptr && !record->factory;
    toolbar.renameButton().setEnabled(userPattern);
    toolbar.deleteButton().setEnabled(userPattern);
}

void PatternBrowser::renameSelected() {
    const auto* record = selectedRecord();
    if (record == nullptr || record->factory || !onRename) {
        return;
    }
    const juce::String id = record->id;
    const juce::String oldName = record->name;
    auto* prompt = new juce::AlertWindow(
            "Rename pattern", "Choose a new title for this pattern.",
            juce::MessageBoxIconType::QuestionIcon, getTopLevelComponent());
    prompt->addTextEditor("name", oldName, "Pattern name");
    prompt->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    prompt->enterModalState(true,
            juce::ModalCallbackFunction::create([
                    safeThis = juce::Component::SafePointer<PatternBrowser>(this),
                    prompt, id, oldName](int result) {
                if (result != 1 || safeThis == nullptr) {
                    return;
                }
                const auto name = prompt->getTextEditorContents("name").trim();
                if (name.isNotEmpty() && name != oldName) {
                    safeThis->onRename(id, name);
                }
            }), true);
}

void PatternBrowser::deleteSelected() {
    const auto* record = selectedRecord();
    if (record == nullptr || record->factory || !onDelete) {
        return;
    }
    const juce::String id = record->id;
    const juce::String name = record->name;
    juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Move pattern to Trash?",
            "Move '" + name + "' to Trash? Presets referencing it will use "
                    "their embedded MIDI sequence or audition note until "
                    "another pattern is assigned.",
            "Move to Trash", "Cancel", getTopLevelComponent(),
            juce::ModalCallbackFunction::create([
                    safeThis = juce::Component::SafePointer<PatternBrowser>(this),
                    id](int result) {
                if (result != 0 && safeThis != nullptr) {
                    safeThis->onDelete(id);
                }
            }));
}

void PatternBrowser::applyFilter() {
    const int previousScroll = viewport.getViewPositionY();
    std::vector<PatternRecord> visible;
    const auto query = toolbar.searchField().getText().trim();
    for (const auto& record : allRecords) {
        const auto tags = tagsFor(record);
        const bool matchesTags = tagCloud.matches(tags);
        const bool matchesQuery = query.isEmpty()
                || record.name.containsIgnoreCase(query)
                || tags.joinIntoString(" ").containsIgnoreCase(query);
        if (matchesTags && matchesQuery
                && (!tagCloud.favoritesOnly()
                        || (favorites != nullptr && favorites->isPatternFavorite(record.id)))) {
            visible.push_back(record);
        }
    }
    list->setRecords(std::move(visible), selectedId);
    resized();
    viewport.setViewPosition(0, previousScroll);
    updateActions();
}

void PatternBrowser::toggleFavorite(const juce::String& id) {
    if (favorites == nullptr) {
        return;
    }
    favorites->togglePattern(id);
    applyFilter();
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
PatternBrowser::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
    for (const auto& [id, bounds] : toolbar.pointerTargetsForAutomation()) {
        targets.push_back({ id, bounds.translated(
                (float) toolbar.getX(), (float) toolbar.getY()) });
    }
    for (const auto& [id, bounds] : tagCloud.pointerTargetsForAutomation()) {
        targets.push_back({ id, bounds.translated(
                (float) tagCloud.getX(), (float) tagCloud.getY()) });
    }
    for (const auto& target : list->pointerTargetsForAutomation(
            viewport.getBounds(), viewport.getViewPositionY())) {
        targets.push_back(target);
    }
    return targets;
}

void PatternBrowser::editSelected() {
    const auto* record = selectedRecord();
    if (record == nullptr) {
        return;
    }
    if (record->factory) {
        createPattern(record);
    } else if (onEdit) {
        onEdit(record->id);
    }
}

void PatternBrowser::resized() {
    auto bounds = getLocalBounds();
    toolbar.setBounds(bounds.removeFromTop(SidebarLibraryToolbar::height));
    bounds.reduce(6, 0);
    bounds.removeFromBottom(7);
    tagHeading.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(4);
    const int cloudHeight = tagCloud.preferredHeightForWidth(bounds.getWidth());
    tagCloud.setBounds(bounds.removeFromTop(cloudHeight));
    bounds.removeFromTop(8);
    viewport.setBounds(bounds);
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

}
