#include "UI/InlinePresetBrowser.h"
#include "UI/PresetCardList.h"

#include "UI/CanvasChromePalette.h"
#include "UI/CanvasChromeIcons.h"
#include "UI/PresetBrowserComponents.h"
#include "UI/PresetMetadataEditor.h"
#include "UI/SidebarMediaRow.h"
#include "UI/ReferenceAssetBrowser.h"
#include "Graph/PresetMetadataStore.h"

namespace CycleV2 {

std::optional<WorkspaceSidebarTab> workspaceSidebarTabForShortcut(
        const juce::KeyPress& key) {
    if (key.getModifiers().isAnyModifierKeyDown()) {
        return std::nullopt;
    }
    const int keyCode = juce::CharacterFunctions::toLowerCase(key.getKeyCode());
    if (keyCode == 'c') {
        return WorkspaceSidebarTab::Curves;
    }
    if (keyCode == 'p') {
        return WorkspaceSidebarTab::Presets;
    }
    return std::nullopt;
}

juce::String workspaceSidebarTabName(WorkspaceSidebarTab tab) {
    switch (tab) {
        case WorkspaceSidebarTab::Curves:
            return "Curves";
        case WorkspaceSidebarTab::Presets:
            return "Presets";
        case WorkspaceSidebarTab::Patterns:
            return "Patterns";
        case WorkspaceSidebarTab::Assets:
            return "Assets";
        case WorkspaceSidebarTab::Nodes:
            return "Nodes";
    }

    jassertfalse;
    return {};
}


InlinePresetBrowser::InlinePresetBrowser(
        std::vector<juce::File> directories,
        OpenCallback openCallback,
        ActionCallback browseCallback,
        TabCallback tabCallback,
        DeleteCallback deleteCallback,
        ConfirmDeleteCallback confirmDeleteCallback,
        ActionCallback createCallback,
        LibraryFavorites* favoriteStore) :
        onOpen(std::move(openCallback))
    ,   onBrowse(std::move(browseCallback))
    ,   onCreate(std::move(createCallback))
    ,   onTabChanged(std::move(tabCallback))
    ,   onDelete(std::move(deleteCallback))
    ,   onConfirmDelete(std::move(confirmDeleteCallback))
    ,   favorites(favoriteStore)
    ,   list(std::make_unique<PresetCardList>(thumbnails)) {
    setComponentID("workspace.presetSidebar");
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    styleTabButton(curves, LibraryIcon::Curves, "Curves");
    styleTabButton(presets, LibraryIcon::Presets, "Presets");
    styleTabButton(patterns, LibraryIcon::Patterns, "Patterns");
    styleTabButton(nodes, LibraryIcon::Nodes, "Nodes");
    styleTabButton(assets, LibraryIcon::Assets, "WAV references");
    curves.setComponentID("workspace.sidebar.curves");
    presets.setComponentID("workspace.sidebar.presets");
    patterns.setComponentID("workspace.sidebar.patterns");
    assets.setComponentID("workspace.sidebar.assets");
    nodes.setComponentID("workspace.sidebar.nodes");
    curves.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Curves); };
    presets.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Presets); };
    patterns.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Patterns); };
    assets.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Assets); };
    nodes.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Nodes); };
    addAndMakeVisible(curves);
    addAndMakeVisible(presets);
    addAndMakeVisible(patterns);
    addAndMakeVisible(nodes);
    addAndMakeVisible(assets);

    auto& search = toolbar.searchField();
    auto& create = toolbar.newButton();
    auto& edit = toolbar.editButton();
    auto& rename = toolbar.renameButton();
    auto& remove = toolbar.deleteButton();
    search.setComponentID("workspace.sidebar.search");
    search.addListener(this);
    search.addKeyListener(this);
    create.setComponentID("workspace.sidebar.presetNew");
    create.setTooltip("Save the current sound as a new preset");
    create.onClick = [this] {
        if (onCreate) {
            onCreate();
        }
    };
    edit.setButtonText("TAGS...");
    edit.setComponentID("workspace.sidebar.presetEdit");
    edit.setTooltip("Edit the selected preset's tags");
    edit.onClick = [this] { editSelectedTags(); };
    rename.setComponentID("workspace.sidebar.presetRename");
    rename.setTooltip("Rename the selected preset's display title");
    rename.onClick = [this] { renameSelected(); };
    tagHeading.setComponentID("workspace.sidebar.presetFilterToggle");
    tagHeading.setClickingTogglesState(true);
    tagHeading.getProperties().set("disclosure", true);
    tagHeading.getProperties().set("quietAction", true);
    tagHeading.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::mutedText);
    tagHeading.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    tagHeading.setTooltip("Expand or collapse preset tag filters");
    tagHeading.onClick = [this] { updateVisibility(); };
    addAndMakeVisible(tagHeading);
    tagCloud.setComponentID("workspace.sidebar.presetTags");
    tagCloud.setFavoritesAvailable(favorites != nullptr);
    tagCloud.setChangeCallback([this] { applyTagFilter(); });
    tagCloud.setEditCallback([this](const juce::String& tag) { toggleSelectedTag(tag); });
    addChildComponent(tagCloud);
    remove.setVisible(false);
    remove.setComponentID("workspace.sidebar.delete");
    remove.setTooltip("Move the selected preset to Trash");
    remove.onClick = [this] { requestDeleteSelected(); };
    addAndMakeVisible(toolbar);

    list->onDelete = [this](const juce::File& file, const juce::String& name) {
        requestDelete(file, name);
    };
    list->setFavorites(favorites);
    list->onSelectionChanged = [this] { updateSelectedTags(); };
    list->setCallbacks([this] { openSelected(); },
            [this](const juce::File& file) { toggleFavorite(file); });
    viewport.setComponentID("workspace.sidebar.viewport");
    viewport.setViewedComponent(list.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(juce::ScrollBar::thumbColourId,
            CanvasChromePalette::strongBorder.withAlpha(0.72f));
    addAndMakeVisible(viewport);
    browse.setComponentID("workspace.sidebar.browse");
    browse.getProperties().set("quietAction", true);
    browse.onClick = [this] { onBrowse(); };
    browse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    browse.setColour(juce::TextButton::buttonOnColourId,
            CanvasChromePalette::strongBorder);
    browse.setColour(juce::TextButton::textColourOffId,
            CanvasChromePalette::text);
    addAndMakeVisible(browse);

    thumbnails.setReadyCallback([safeThis = juce::Component::SafePointer<InlinePresetBrowser>(this)] {
        if (safeThis != nullptr) {
            safeThis->list->repaint();
        }
    });
    index = std::make_unique<PresetLibraryIndex>(
            std::move(directories),
            [safeThis = juce::Component::SafePointer<InlinePresetBrowser>(this)](
                    const auto& records,
                    const auto& indices) {
                if (safeThis != nullptr) {
                    safeThis->receiveResults(records, indices);
                }
            });
    index->start();
    updateVisibility();
}

InlinePresetBrowser::~InlinePresetBrowser() {
    thumbnails.setReadyCallback({});
    setLookAndFeel(nullptr);
}

void InlinePresetBrowser::configurePatterns(
        PatternSelectCallback select,
        PatternEditCallback edit,
        PatternCreateCallback create,
        PatternRenameCallback rename,
        PatternDeleteCallback remove) {
    patternBrowser = std::make_unique<PatternBrowser>(
            std::move(select), std::move(edit), std::move(create), favorites,
            std::move(rename), std::move(remove));
    patternBrowser->setPlaybackToggleCallback(onTogglePlayback);
    addAndMakeVisible(*patternBrowser);
    updateVisibility();
}

void InlinePresetBrowser::setPatterns(
        std::vector<PatternRecord> records,
        const juce::String& selectedId) {
    patternTags.clear();
    for (const auto& record : records) {
        patternTags[record.id.toStdString()] = record.tags.isEmpty()
                ? juce::StringArray { record.tag } : record.tags;
    }
    if (patternBrowser != nullptr) {
        patternBrowser->setRecords(std::move(records), selectedId);
    }
    updateAvailableTags();
    applyTagFilter();
}

void InlinePresetBrowser::setActiveTab(WorkspaceSidebarTab nextTab) {
    if (tab == nextTab) {
        if (tab == WorkspaceSidebarTab::Presets) {
            focusPresetSearch();
        }
        return;
    }
    tab = nextTab;
    updateVisibility();
    if (tab == WorkspaceSidebarTab::Presets) {
        focusPresetSearch();
    }
    onTabChanged(tab);
    repaint();
}

void InlinePresetBrowser::focusPresetSearch() {
    juce::Timer::callAfterDelay(
            0,
            [safeSearch = juce::Component::SafePointer<juce::TextEditor>(
                    &toolbar.searchField())] {
                if (safeSearch != nullptr && safeSearch->isShowing()) {
                    safeSearch->grabKeyboardFocus();
                }
            });
}

int InlinePresetBrowser::visiblePresetCount() const {
    return list->count();
}

void InlinePresetBrowser::refreshIndex() {
    index->start();
}

void InlinePresetBrowser::refreshRecord(const juce::File& file) {
    index->refreshRecord(file);
}

void InlinePresetBrowser::setPlaybackToggleCallback(ActionCallback callback) {
    onTogglePlayback = std::move(callback);
    toolbar.searchField().setPlaybackToggleCallback(onTogglePlayback);
    if (patternBrowser != nullptr) {
        patternBrowser->setPlaybackToggleCallback(onTogglePlayback);
    }
}

void InlinePresetBrowser::setMetadataChangedCallbacks(
        TitleChangedCallback titleCallback,
        TagsChangedCallback tagsCallback) {
    onTitleChanged = std::move(titleCallback);
    onTagsChanged = std::move(tagsCallback);
}

juce::String InlinePresetBrowser::deleteConfirmationMessage(
        const juce::String& presetName) {
    return "\"" + presetName + "\" will be removed from the preset library.";
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
InlinePresetBrowser::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets {
            { "workspace.sidebar.curves", curves.getBounds().toFloat() },
            { "workspace.sidebar.presets", presets.getBounds().toFloat() },
            { "workspace.sidebar.patterns", patterns.getBounds().toFloat() },
            { "workspace.sidebar.nodes", nodes.getBounds().toFloat() },
            { "workspace.sidebar.assets", assets.getBounds().toFloat() }
    };
    if (tab == WorkspaceSidebarTab::Presets) {
        for (const auto& [id, bounds] : toolbar.pointerTargetsForAutomation()) {
            targets.push_back({ id, bounds.translated(
                    (float) toolbar.getX(), (float) toolbar.getY()) });
        }
        targets.push_back({ "workspace.sidebar.browse", browse.getBounds().toFloat() });
        targets.push_back({ tagHeading.getComponentID(), tagHeading.getBounds().toFloat() });
        if (tagCloud.isVisible()) {
            for (const auto& [id, bounds] : tagCloud.pointerTargetsForAutomation()) {
                targets.push_back({ id, bounds.translated(
                        (float) tagCloud.getX(), (float) tagCloud.getY()) });
            }
        }
        for (const auto& target : list->pointerTargetsForAutomation(
                viewport.getBounds(), viewport.getViewPositionY())) {
            targets.push_back(target);
        }
    } else if (tab == WorkspaceSidebarTab::Patterns && patternBrowser != nullptr) {
        for (const auto& [id, bounds] : patternBrowser->pointerTargetsForAutomation()) {
            targets.push_back({ id, bounds.translated(
                    (float) patternBrowser->getX(), (float) patternBrowser->getY()) });
        }
    }
    if (tab == WorkspaceSidebarTab::Assets && assetBrowser != nullptr) {
        for (const auto& [id, bounds] : assetBrowser->pointerTargetsForAutomation()) {
            targets.push_back({ id, bounds.translated(
                    (float) assetBrowser->getX(), (float) assetBrowser->getY()) });
        }
    }
    return targets;
}

bool InlinePresetBrowser::hitTest(int x, int y) {
    if (tab == WorkspaceSidebarTab::Nodes && nodePaletteHitTest != nullptr
            && nodePaletteHitTest({ x, y })) {
        return false;
    }
    return tab != WorkspaceSidebarTab::Curves
            || juce::Rectangle<int>(0, 0, getWidth(), 46).contains(x, y);
}

void InlinePresetBrowser::paint(juce::Graphics& graphics) {
    const auto bounds = getLocalBounds().toFloat();
    if (tab != WorkspaceSidebarTab::Curves && tab != WorkspaceSidebarTab::Nodes) {
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.fillRect(bounds);
    }
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRect(bounds.withHeight(46.f));
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawHorizontalLine(46, 0.f, bounds.getRight());
    graphics.drawVerticalLine(patterns.getRight() + 8, 10.f, 36.f);
    graphics.drawVerticalLine(getWidth() - 1, 0.f, 46.f);
    const auto selectedTab = tab == WorkspaceSidebarTab::Curves
            ? curves.getBounds().toFloat()
            : tab == WorkspaceSidebarTab::Presets
                    ? presets.getBounds().toFloat()
                    : tab == WorkspaceSidebarTab::Patterns
                            ? patterns.getBounds().toFloat()
                            : tab == WorkspaceSidebarTab::Assets
                                    ? assets.getBounds().toFloat() : nodes.getBounds().toFloat();
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.fillRoundedRectangle(
            selectedTab.withY(43.f).withHeight(3.f).reduced(8.f, 0.f),
            1.5f);
    if (tab != WorkspaceSidebarTab::Curves) {
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.78f));
        graphics.drawVerticalLine(0, bounds.getY(), bounds.getBottom());
    }
}

void InlinePresetBrowser::setCurveCount(size_t count) {
    if ((int64) curves.getProperties().getWithDefault("libraryCount", -1) == (int64) count) {
        return;
    }
    curves.getProperties().set("libraryCount", (int64) count);
    curves.setTooltip("Curves (" + String((int64) count) + " in this preset)");
    curves.setTitle(curves.getTooltip());
    curves.repaint();
}

void InlinePresetBrowser::resized() {
    auto bounds = getLocalBounds();
    auto tabs = bounds.removeFromTop(46).reduced(8, 0);
    const int tabWidth = juce::jmin(100, (tabs.getWidth() - 16) / 5);
    assets.setBounds(tabs.removeFromLeft(tabWidth));
    presets.setBounds(tabs.removeFromLeft(tabWidth));
    patterns.setBounds(tabs.removeFromLeft(tabWidth));
    tabs.removeFromLeft(16);
    nodes.setBounds(tabs.removeFromLeft(tabWidth));
    curves.setBounds(tabs.removeFromLeft(tabWidth));
    if (assetBrowser != nullptr) {
        assetBrowser->setBounds(bounds);
    }
    if (patternBrowser != nullptr) {
        patternBrowser->setBounds(bounds);
    }
    if (tab != WorkspaceSidebarTab::Presets) {
        return;
    }

    auto footer = bounds.removeFromBottom(42).reduced(10, 7);
    browse.setBounds(footer.withWidth(juce::jmin(148, footer.getWidth())));
    toolbar.setBounds(bounds.removeFromTop(SidebarLibraryToolbar::height));
    bounds.reduce(6, 0);
    tagHeading.setBounds(bounds.removeFromTop(24));
    if (tagHeading.getToggleState()) {
        bounds.removeFromTop(4);
        const int cloudHeight = tagCloud.preferredHeightForWidth(bounds.getWidth());
        tagCloud.setBounds(bounds.removeFromTop(cloudHeight));
    }
    bounds.removeFromTop(6);
    viewport.setBounds(bounds);
    list->setSize(
            juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

bool InlinePresetBrowser::keyPressed(const juce::KeyPress& key) {
    return keyPressed(key, this);
}

bool InlinePresetBrowser::keyPressed(
        const juce::KeyPress& key,
        juce::Component* source) {
    if (tab != WorkspaceSidebarTab::Presets) {
        return false;
    }
    if (source == &toolbar.searchField()
            && toolbar.searchField().handlePlaybackSpace(key)) {
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        openSelected();
        return true;
    }
    if (key == juce::KeyPress::upKey) {
        list->moveSelection(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey) {
        list->moveSelection(1);
        return true;
    }
    if (key == juce::KeyPress::escapeKey) {
        setActiveTab(WorkspaceSidebarTab::Curves);
        return true;
    }
    return false;
}

void InlinePresetBrowser::textEditorTextChanged(juce::TextEditor&) {
    index->setQuery(toolbar.searchField().getText());
}

void InlinePresetBrowser::textEditorReturnKeyPressed(juce::TextEditor&) {
    openSelected();
}

void InlinePresetBrowser::receiveResults(
        const std::vector<PresetLibraryRecord>& records,
        const std::vector<int>& visibleIndices) {
    library = records;
    searchResults = visibleIndices;
    updateAvailableTags();
    applyTagFilter();
}

void InlinePresetBrowser::updateAvailableTags() {
    for (const auto& record : library) {
        for (const auto& tag : tagsFor(record)) {
            knownTags.addIfNotAlreadyThere(tag, true);
        }
    }
    tagCloud.setTags(knownTags);
    resized();
}

void InlinePresetBrowser::applyTagFilter() {
    const int activeFilters = tagCloud.selectedTags().size() + (tagCloud.favoritesOnly() ? 1 : 0);
    tagHeading.setButtonText(activeFilters > 0
            ? "Filter tags (" + String(activeFilters) + ")" : "Filter tags");
    std::vector<int> filtered;
    std::vector<juce::StringArray> tags;
    filtered.reserve(searchResults.size());
    tags.reserve(searchResults.size());
    for (const int indexToCheck : searchResults) {
        auto recordTags = tagsFor(library[(size_t) indexToCheck]);
        if (tagCloud.matches(recordTags)
                && (!tagCloud.favoritesOnly()
                        || (favorites != nullptr && favorites->isPresetFavorite(
                                library[(size_t) indexToCheck].file)))) {
            filtered.push_back(indexToCheck);
            tags.push_back(std::move(recordTags));
        }
    }
    list->setResults(library, filtered, std::move(tags));
    updateSelectedTags();
    const int width = juce::jmax(1, viewport.getMaximumVisibleWidth());
    list->setSize(width, list->getHeight());
}

void InlinePresetBrowser::updateSelectedTags() {
    const auto* record = list->selectedRecord();
    const bool editable = record != nullptr && record->metadataReady;
    tagCloud.setRecordTags(record != nullptr ? tagsFor(*record) : juce::StringArray {});
    toolbar.editButton().setEnabled(editable);
    toolbar.renameButton().setEnabled(editable);
    toolbar.deleteButton().setEnabled(record != nullptr);
}

void InlinePresetBrowser::toggleSelectedTag(const juce::String& tag) {
    const auto* record = list->selectedRecord();
    if (record == nullptr || !record->metadataReady) {
        return;
    }
    const auto file = record->file;
    auto tags = tagsFor(*record);
    const int indexOfTag = tags.indexOf(tag, true);
    if (indexOfTag >= 0) {
        tags.remove(indexOfTag);
    } else {
        tags.add(tag);
    }
    juce::String error;
    if (!PresetMetadataStore::save(file, tags, error)) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Unable to save tags", error);
        return;
    }
    index->refreshRecord(file);
    if (onTagsChanged) {
        onTagsChanged(file, tags);
    }
}

void InlinePresetBrowser::toggleFavorite(const juce::File& file) {
    if (favorites == nullptr) {
        return;
    }
    favorites->togglePreset(file);
    applyTagFilter();
}

void InlinePresetBrowser::refreshFavorites() {
    applyTagFilter();
}

juce::StringArray InlinePresetBrowser::tagsFor(
        const PresetLibraryRecord& record) const {
    if (record.presentation.tagsSpecified || !record.presentation.tags.isEmpty()) {
        return record.presentation.tags;
    }
    const auto found = patternTags.find(record.presentation.patternId.toStdString());
    if (found != patternTags.end() && !found->second.isEmpty()) {
        return found->second;
    }
    return { "Other" };
}

void InlinePresetBrowser::openSelected() {
    const PresetLibraryRecord* record = list->selectedRecord();
    if (record == nullptr) {
        return;
    }
    if (!onOpen(record->file)) {
        juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Preset not loaded",
                "The selected preset could not be opened.");
    }
}

void InlinePresetBrowser::editSelectedTags() {
    const auto* record = list->selectedRecord();
    if (record == nullptr || !record->metadataReady) {
        return;
    }
    const auto file = record->file;
    PresetMetadataEditor::editTags(*this, file, record->presentation.tags,
            [safeThis = juce::Component::SafePointer<InlinePresetBrowser>(this),
                    file](const juce::StringArray& tags) {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->index->refreshRecord(file);
                if (safeThis->onTagsChanged) {
                    safeThis->onTagsChanged(file, tags);
                }
            });
}

void InlinePresetBrowser::renameSelected() {
    const auto* record = list->selectedRecord();
    if (record == nullptr || !record->metadataReady) {
        return;
    }
    const auto file = record->file;
    PresetMetadataEditor::rename(*this, file, record->name,
            [safeThis = juce::Component::SafePointer<InlinePresetBrowser>(this),
                    file](const juce::String& title) {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->index->refreshRecord(file);
                if (safeThis->onTitleChanged) {
                    safeThis->onTitleChanged(file, title);
                }
            });
}

void InlinePresetBrowser::requestDeleteSelected() {
    const PresetLibraryRecord* record = list->selectedRecord();
    if (record == nullptr) {
        return;
    }

    requestDelete(record->file, record->name);
}

void InlinePresetBrowser::requestDelete(const juce::File& file, const juce::String& name) {
    auto completion = [
            safeThis = juce::Component::SafePointer<InlinePresetBrowser>(this),
            file](bool confirmed) {
        if (confirmed && safeThis != nullptr) {
            safeThis->deletePreset(file);
        }
    };
    if (onConfirmDelete) {
        onConfirmDelete(name, std::move(completion));
        return;
    }

    juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Move preset to Trash?",
            deleteConfirmationMessage(name),
            "Move to Trash",
            "Cancel",
            getTopLevelComponent(),
            juce::ModalCallbackFunction::create([
                    completion = std::move(completion)](int result) mutable {
                completion(result != 0);
            }));
}

void InlinePresetBrowser::deletePreset(const juce::File& file) {
    const bool deleted = onDelete ? onDelete(file) : file.moveToTrash();
    if (!deleted) {
        juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Preset not deleted",
                "The selected preset could not be moved to Trash.");
        return;
    }
    index->start();
}

void InlinePresetBrowser::updateVisibility() {
    const bool showingPresets = tab == WorkspaceSidebarTab::Presets;
    curves.setToggleState(tab == WorkspaceSidebarTab::Curves, juce::dontSendNotification);
    presets.setToggleState(showingPresets, juce::dontSendNotification);
    patterns.setToggleState(tab == WorkspaceSidebarTab::Patterns,
            juce::dontSendNotification);
    curves.setColour(
            juce::TextButton::textColourOffId,
            tab == WorkspaceSidebarTab::Curves ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    curves.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    presets.setColour(
            juce::TextButton::textColourOffId,
            showingPresets ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    presets.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    patterns.setColour(juce::TextButton::textColourOffId,
            tab == WorkspaceSidebarTab::Patterns
                    ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    patterns.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    nodes.setToggleState(tab == WorkspaceSidebarTab::Nodes, juce::dontSendNotification);
    nodes.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::mutedText);
    nodes.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    assets.setToggleState(tab == WorkspaceSidebarTab::Assets, juce::dontSendNotification);
    if (assetBrowser != nullptr) {
        assetBrowser->setVisible(tab == WorkspaceSidebarTab::Assets);
    }
    toolbar.setVisible(showingPresets);
    tagHeading.setVisible(showingPresets);
    tagCloud.setVisible(showingPresets && tagHeading.getToggleState());
    viewport.setVisible(showingPresets);
    browse.setVisible(showingPresets);
    if (patternBrowser != nullptr) {
        patternBrowser->setVisible(tab == WorkspaceSidebarTab::Patterns);
    }
    resized();
}

void InlinePresetBrowser::configureAssets(ReferenceAssetBrowser& browser) {
    assetBrowser = &browser;
    addChildComponent(browser);
    updateVisibility();
}

void InlinePresetBrowser::showAssets() {
    setActiveTab(WorkspaceSidebarTab::Assets);
}

void InlinePresetBrowser::styleTabButton(juce::TextButton& button,
        LibraryIcon icon, const juce::String& name) {
    LibraryIconRenderer::configure(button, icon, name);
    button.getProperties().set("sidebarTab", true);
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::mutedText);
}

}
