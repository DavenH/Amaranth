#include "UI/InlinePresetBrowser.h"

#include "UI/CanvasChromePalette.h"
#include "UI/PresetBrowserComponents.h"
#include "UI/SidebarMediaRow.h"

namespace CycleV2 {

namespace {

constexpr int contentInset = 10;

}

class InlinePresetBrowser::CompactList final : public juce::Component {
public:
    using Callback = std::function<void()>;

    explicit CompactList(PresetThumbnailCache& thumbnailsToUse) :
            thumbnails(thumbnailsToUse) {
        setComponentID("workspace.sidebar.list");
    }

    void setResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices,
            std::vector<juce::StringArray> visibleTags) {
        juce::File selectedFile;
        if (const auto* current = selectedRecord()) {
            selectedFile = current->file;
        }
        library = records;
        indices = visibleIndices;
        tags = std::move(visibleTags);
        selected = indices.empty() ? -1 : 0;
        for (int index = 0; index < (int) indices.size(); ++index) {
            if (library[(size_t) indices[(size_t) index]].file == selectedFile) {
                selected = index;
                break;
            }
        }
        updateHeight();
        repaint();
    }

    void setCallbacks(Callback openCallback,
            std::function<void(const juce::File&)> favoriteCallback) {
        onOpen = std::move(openCallback);
        onFavorite = std::move(favoriteCallback);
    }

    void setFavorites(LibraryFavorites* store) {
        favorites = store;
        repaint();
    }

    const PresetLibraryRecord* selectedRecord() const {
        if (!juce::isPositiveAndBelow(selected, (int) indices.size())) {
            return nullptr;
        }
        const int recordIndex = indices[(size_t) selected];
        return juce::isPositiveAndBelow(recordIndex, (int) library.size())
                ? &library[(size_t) recordIndex]
                : nullptr;
    }

    int count() const { return (int) indices.size(); }

    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation(
                    const juce::Rectangle<int>& viewportBounds,
                    int viewPositionY) const {
        std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
        for (int index = 0; index < (int) indices.size(); ++index) {
            const auto row = rowBounds(index).toFloat().translated(
                    (float) viewportBounds.getX(),
                    (float) viewportBounds.getY() - viewPositionY);
            if (row.intersects(viewportBounds.toFloat())) {
                const auto& record = library[(size_t) indices[(size_t) index]];
                targets.push_back({ "workspace.sidebar.preset."
                                + record.file.getFileNameWithoutExtension(),
                        row.getIntersection(viewportBounds.toFloat()) });
                targets.push_back({ "workspace.sidebar.presetFavorite."
                                + record.file.getFileNameWithoutExtension(),
                        SidebarMediaRow::favoriteBounds(row) });
            }
        }
        return targets;
    }

    void moveSelection(int delta) {
        if (indices.empty()) {
            return;
        }
        select(juce::jlimit(0, (int) indices.size() - 1, selected + delta));
    }

    void paint(juce::Graphics& graphics) override {
        const auto clip = graphics.getClipBounds();
        for (int visibleIndex = 0; visibleIndex < (int) indices.size(); ++visibleIndex) {
            const auto bounds = rowBounds(visibleIndex).toFloat();
            if (clip.intersects(bounds.toNearestInt())) {
                paintRow(graphics, visibleIndex, bounds);
            }
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (event.getNumberOfClicks() > 1) {
            return;
        }
        const int hit = indexAt(event.getPosition());
        if (hit >= 0) {
            if (SidebarMediaRow::favoriteBounds(rowBounds(hit).toFloat())
                    .contains(event.position) && onFavorite) {
                onFavorite(library[(size_t) indices[(size_t) hit]].file);
                return;
            }
            select(hit);
            if (onOpen) {
                onOpen();
            }
        }
    }

private:
    juce::Rectangle<int> rowBounds(int index) const {
        return {
                0,
                contentInset + index * SidebarMediaRow::height,
                getWidth(),
                SidebarMediaRow::height
        };
    }

    int indexAt(juce::Point<int> position) const {
        for (int index = 0; index < (int) indices.size(); ++index) {
            if (rowBounds(index).contains(position)) {
                return index;
            }
        }
        return -1;
    }

    void select(int index) {
        if (!juce::isPositiveAndBelow(index, (int) indices.size()) || selected == index) {
            return;
        }
        selected = index;
        repaint();
    }

    void updateHeight() {
        const int height = contentInset * 2
                + (int) indices.size() * SidebarMediaRow::height;
        setSize(juce::jmax(1, getWidth()), juce::jmax(1, height));
    }

    void paintRow(
            juce::Graphics& graphics,
            int visibleIndex,
            juce::Rectangle<float> bounds) {
        const auto& record = library[(size_t) indices[(size_t) visibleIndex]];
        const bool isSelected = visibleIndex == selected;
        const auto preview = SidebarMediaRow::paintFrame(graphics, bounds,
                isSelected);
        {
            juce::Graphics::ScopedSaveState save(graphics);
            juce::Path clip;
            clip.addRoundedRectangle(preview, 3.f);
            graphics.reduceClipRegion(clip);
            PresetBrowserPainting::drawPreview(
                    graphics, record, thumbnails, preview, false);
        }
        SidebarMediaRow::paintLabels(graphics, bounds, record.name,
                tags[(size_t) visibleIndex],
                favorites != nullptr && favorites->isPresetFavorite(record.file));
    }

    PresetThumbnailCache& thumbnails;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> indices;
    std::vector<juce::StringArray> tags;
    Callback onOpen;
    std::function<void(const juce::File&)> onFavorite;
    LibraryFavorites* favorites {};
    int selected { -1 };
};

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
    ,   list(std::make_unique<CompactList>(thumbnails)) {
    setComponentID("workspace.presetSidebar");
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    styleTabButton(curves);
    styleTabButton(presets);
    styleTabButton(patterns);
    curves.setComponentID("workspace.sidebar.curves");
    presets.setComponentID("workspace.sidebar.presets");
    patterns.setComponentID("workspace.sidebar.patterns");
    curves.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Curves); };
    presets.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Presets); };
    patterns.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Patterns); };
    addAndMakeVisible(curves);
    addAndMakeVisible(presets);
    addAndMakeVisible(patterns);

    search.setComponentID("workspace.sidebar.search");
    search.addListener(this);
    search.addKeyListener(this);
    addAndMakeVisible(search);

    create.setComponentID("workspace.sidebar.presetNew");
    create.setTooltip("Save the current sound as a new preset");
    create.onClick = [this] {
        if (onCreate) {
            onCreate();
        }
    };
    SidebarTagCloud::styleHeading(tagHeading);
    addAndMakeVisible(tagHeading);
    tagCloud.setComponentID("workspace.sidebar.presetTags");
    tagCloud.setChangeCallback([this] { applyTagFilter(); });
    addAndMakeVisible(tagCloud);
    favoritesOnly.setComponentID("workspace.sidebar.presetFavoritesOnly");
    favoritesOnly.setClickingTogglesState(true);
    favoritesOnly.setTooltip("Show only favorite presets");
    favoritesOnly.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::restingControlSurface);
    favoritesOnly.setColour(juce::TextButton::buttonOnColourId,
            CanvasChromePalette::navigationAccent.withAlpha(0.3f));
    favoritesOnly.setColour(juce::TextButton::textColourOffId,
            CanvasChromePalette::text);
    favoritesOnly.onClick = [this] { applyTagFilter(); };
    addAndMakeVisible(favoritesOnly);
    remove.setComponentID("workspace.sidebar.delete");
    remove.setTooltip("Move the selected preset to Trash");
    remove.onClick = [this] { requestDeleteSelected(); };
    for (auto* button : { &create, &remove }) {
        button->setColour(juce::TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button->setColour(juce::TextButton::textColourOffId,
                CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }

    list->setFavorites(favorites);
    list->setCallbacks([this] { openSelected(); },
            [this](const juce::File& file) { toggleFavorite(file); });
    viewport.setComponentID("workspace.sidebar.viewport");
    viewport.setViewedComponent(list.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(juce::ScrollBar::thumbColourId,
            CanvasChromePalette::strongBorder.withAlpha(0.72f));
    addAndMakeVisible(viewport);
    browse.setComponentID("workspace.sidebar.browse");
    browse.onClick = [this] { onBrowse(); };
    browse.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::restingControlSurface);
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
        PatternCreateCallback create) {
    patternBrowser = std::make_unique<PatternBrowser>(
            std::move(select), std::move(edit), std::move(create), favorites);
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
        return;
    }
    tab = nextTab;
    updateVisibility();
    if (tab == WorkspaceSidebarTab::Presets) {
        juce::Timer::callAfterDelay(
                0,
                [safeSearch = juce::Component::SafePointer<juce::TextEditor>(&search)] {
                    if (safeSearch != nullptr && safeSearch->isShowing()) {
                        safeSearch->grabKeyboardFocus();
                    }
                });
    }
    onTabChanged(tab);
    repaint();
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
    search.setPlaybackToggleCallback(onTogglePlayback);
    if (patternBrowser != nullptr) {
        patternBrowser->setPlaybackToggleCallback(onTogglePlayback);
    }
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
            { "workspace.sidebar.patterns", patterns.getBounds().toFloat() }
    };
    if (tab == WorkspaceSidebarTab::Presets) {
        targets.push_back({ "workspace.sidebar.search", search.getBounds().toFloat() });
        targets.push_back({ "workspace.sidebar.presetNew", create.getBounds().toFloat() });
        targets.push_back({ "workspace.sidebar.delete", remove.getBounds().toFloat() });
        targets.push_back({ "workspace.sidebar.presetFavoritesOnly",
                favoritesOnly.getBounds().toFloat() });
        targets.push_back({ "workspace.sidebar.browse", browse.getBounds().toFloat() });
        for (const auto& [id, bounds] : tagCloud.pointerTargetsForAutomation()) {
            targets.push_back({ id, bounds.translated(
                    (float) tagCloud.getX(), (float) tagCloud.getY()) });
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
    return targets;
}

bool InlinePresetBrowser::hitTest(int x, int y) {
    return tab != WorkspaceSidebarTab::Curves
            || juce::Rectangle<int>(0, 0, getWidth(), 46).contains(x, y);
}

void InlinePresetBrowser::paint(juce::Graphics& graphics) {
    const auto bounds = getLocalBounds().toFloat();
    if (tab != WorkspaceSidebarTab::Curves) {
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.fillRect(bounds);
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.78f));
        graphics.drawVerticalLine(0, bounds.getY(), bounds.getBottom());
    }
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRect(bounds.withHeight(46.f));
    const auto selectedTab = tab == WorkspaceSidebarTab::Curves
            ? curves.getBounds().toFloat()
            : tab == WorkspaceSidebarTab::Presets
                    ? presets.getBounds().toFloat()
                    : patterns.getBounds().toFloat();
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.fillRoundedRectangle(
            selectedTab.withY(43.f).withHeight(3.f).reduced(8.f, 0.f),
            1.5f);
}

void InlinePresetBrowser::resized() {
    auto bounds = getLocalBounds();
    auto tabs = bounds.removeFromTop(46).reduced(8, 0);
    const int tabWidth = juce::jmin(100, tabs.getWidth() / 3);
    curves.setBounds(tabs.removeFromLeft(tabWidth));
    presets.setBounds(tabs.removeFromLeft(tabWidth));
    patterns.setBounds(tabs.removeFromLeft(tabWidth));
    if (patternBrowser != nullptr) {
        patternBrowser->setBounds(bounds);
    }
    if (tab != WorkspaceSidebarTab::Presets) {
        return;
    }

    auto footer = bounds.removeFromBottom(54).reduced(10, 8);
    browse.setBounds(footer);
    bounds.reduce(10, 7);
    auto searchRow = bounds.removeFromTop(30);
    create.setBounds(searchRow.removeFromRight(69));
    searchRow.removeFromRight(5);
    search.setBounds(searchRow);
    bounds.removeFromTop(6);
    auto headingRow = bounds.removeFromTop(24);
    remove.setBounds(headingRow.removeFromRight(69));
    headingRow.removeFromRight(4);
    favoritesOnly.setBounds(headingRow.removeFromRight(82));
    tagHeading.setBounds(headingRow);
    bounds.removeFromTop(4);
    const int cloudHeight = tagCloud.preferredHeightForWidth(bounds.getWidth());
    tagCloud.setBounds(bounds.removeFromTop(cloudHeight));
    bounds.removeFromTop(8);
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
    if (source == &search && search.handlePlaybackSpace(key)) {
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
    index->setQuery(search.getText());
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
    juce::StringArray available;
    for (const auto& record : library) {
        for (const auto& tag : tagsFor(record)) {
            available.addIfNotAlreadyThere(tag);
        }
    }
    tagCloud.setTags(std::move(available));
    resized();
}

void InlinePresetBrowser::applyTagFilter() {
    std::vector<int> filtered;
    std::vector<juce::StringArray> tags;
    filtered.reserve(searchResults.size());
    tags.reserve(searchResults.size());
    for (const int indexToCheck : searchResults) {
        auto recordTags = tagsFor(library[(size_t) indexToCheck]);
        if (tagCloud.matches(recordTags)
                && (!favoritesOnly.getToggleState()
                        || (favorites != nullptr && favorites->isPresetFavorite(
                                library[(size_t) indexToCheck].file)))) {
            filtered.push_back(indexToCheck);
            tags.push_back(std::move(recordTags));
        }
    }
    list->setResults(library, filtered, std::move(tags));
    remove.setEnabled(!filtered.empty());
    const int width = juce::jmax(1, viewport.getMaximumVisibleWidth());
    list->setSize(width, list->getHeight());
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
    if (!record.presentation.tags.isEmpty()) {
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

void InlinePresetBrowser::requestDeleteSelected() {
    const PresetLibraryRecord* record = list->selectedRecord();
    if (record == nullptr) {
        return;
    }

    const juce::File file = record->file;
    const juce::String name = record->name;
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
    curves.setToggleState(!showingPresets, juce::dontSendNotification);
    presets.setToggleState(showingPresets, juce::dontSendNotification);
    patterns.setToggleState(tab == WorkspaceSidebarTab::Patterns,
            juce::dontSendNotification);
    curves.setColour(
            juce::TextButton::textColourOffId,
            showingPresets ? CanvasChromePalette::mutedText : CanvasChromePalette::text);
    curves.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    presets.setColour(
            juce::TextButton::textColourOffId,
            showingPresets ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    presets.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    patterns.setColour(juce::TextButton::textColourOffId,
            tab == WorkspaceSidebarTab::Patterns
                    ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    patterns.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    search.setVisible(showingPresets);
    create.setVisible(showingPresets);
    tagHeading.setVisible(showingPresets);
    tagCloud.setVisible(showingPresets);
    favoritesOnly.setVisible(showingPresets);
    remove.setVisible(showingPresets);
    viewport.setVisible(showingPresets);
    browse.setVisible(showingPresets);
    if (patternBrowser != nullptr) {
        patternBrowser->setVisible(tab == WorkspaceSidebarTab::Patterns);
    }
    resized();
}

void InlinePresetBrowser::styleTabButton(juce::TextButton& button) {
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::mutedText);
}

}
