#include "UI/InlinePresetBrowser.h"

#include "UI/CanvasChromePalette.h"
#include "UI/PresetBrowserComponents.h"

namespace CycleV2 {

namespace {

constexpr int heroHeight = 204;
constexpr int heroMetadataHeight = 52;
constexpr int rowHeight = 76;
constexpr int rowGap = 2;
constexpr int contentInset = 10;

void drawTag(
        juce::Graphics& graphics,
        const juce::String& tag,
        juce::Rectangle<float> bounds) {
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRoundedRectangle(bounds, 4.f);
    graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.86f));
    graphics.drawRoundedRectangle(bounds, 4.f, 1.f);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.84f));
    graphics.setFont(juce::FontOptions(10.f));
    graphics.drawFittedText(
            tag,
            bounds.toNearestInt().reduced(7, 0),
            juce::Justification::centred,
            1);
}

class TrashButton final : public juce::Button {
public:
    TrashButton() : juce::Button("Delete preset") {
        setComponentID("workspace.sidebar.delete");
        setTooltip("Move preset to Trash");
    }

    void paintButton(
            juce::Graphics& graphics,
            bool isMouseOverButton,
            bool isButtonDown) override {
        auto bounds = getLocalBounds().toFloat().reduced(4.f);
        if (isMouseOverButton || isButtonDown) {
            graphics.setColour(CanvasChromePalette::raisedSurface.withAlpha(
                    isButtonDown ? 0.96f : 0.78f));
            graphics.fillEllipse(bounds);
        }

        graphics.setColour(isMouseOverButton
                ? CanvasChromePalette::destructive
                : CanvasChromePalette::text.withAlpha(0.78f));
        const auto bin = bounds.reduced(5.f, 4.f);
        juce::Path body;
        body.startNewSubPath(bin.getX() + 2.f, bin.getY() + 5.f);
        body.lineTo(bin.getX() + 3.f, bin.getBottom());
        body.lineTo(bin.getRight() - 3.f, bin.getBottom());
        body.lineTo(bin.getRight() - 2.f, bin.getY() + 5.f);
        graphics.strokePath(body, juce::PathStrokeType(
                1.5f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        graphics.drawLine(bin.getX(), bin.getY() + 3.f,
                bin.getRight(), bin.getY() + 3.f, 1.5f);
        graphics.drawLine(bin.getCentreX() - 2.f, bin.getY(),
                bin.getCentreX() + 2.f, bin.getY(), 1.5f);
    }
};

}

class InlinePresetBrowser::SelectedPresetCard final : public juce::Component {
public:
    SelectedPresetCard(
            PresetThumbnailCache& thumbnailsToUse,
            std::function<void()> deleteCallback) :
            thumbnails(thumbnailsToUse) {
        setComponentID("workspace.sidebar.hero");
        trash.onClick = std::move(deleteCallback);
        addAndMakeVisible(trash);
    }

    void setRecord(const PresetLibraryRecord* nextRecord) {
        hasRecord = nextRecord != nullptr;
        if (hasRecord) {
            record = *nextRecord;
        }
        setName(hasRecord ? "Selected preset: " + record.name : juce::String());
        trash.setVisible(hasRecord);
        repaint();
    }

    void paint(juce::Graphics& graphics) override {
        if (!hasRecord) {
            return;
        }

        const auto bounds = cardBounds();
        auto content = bounds;
        PresetBrowserPainting::drawPreview(
                graphics, record, thumbnails, content, false);

        const auto metadata = content.removeFromBottom((float) heroMetadataHeight);
        juce::ColourGradient scrim(
                CanvasChromePalette::canvasBackground.withAlpha(0.30f),
                metadata.getX(),
                metadata.getY(),
                CanvasChromePalette::canvasBackground.withAlpha(0.94f),
                metadata.getX(),
                metadata.getBottom(),
                false);
        graphics.setGradientFill(scrim);
        graphics.fillRect(metadata);

        auto overlay = metadata.reduced(10.f, 4.f);
        auto name = overlay.removeFromTop(22.f);
        graphics.setColour(CanvasChromePalette::text);
        graphics.setFont(juce::FontOptions(15.f).withStyle("Bold"));
        graphics.drawFittedText(
                record.name,
                name.withTrimmedRight(32.f).toNearestInt(),
                juce::Justification::centredLeft,
                1);

        auto tags = overlay.removeFromTop(22.f);
        for (int tag = 0; tag < juce::jmin(3, record.presentation.tags.size()); ++tag) {
            const float width = juce::jlimit(
                    42.f, 78.f, 18.f + (float) record.presentation.tags[tag].length() * 6.f);
            drawTag(graphics, record.presentation.tags[tag], tags.removeFromLeft(width));
            tags.removeFromLeft(6.f);
        }
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.82f));
        graphics.drawLine(
                bounds.getX(),
                bounds.getBottom() - 1.f,
                bounds.getRight(),
                bounds.getBottom() - 1.f,
                2.f);
    }

    void resized() override {
        const auto bounds = cardBounds().toNearestInt();
        const auto metadata = bounds.withTop(bounds.getBottom() - heroMetadataHeight);
        trash.setBounds(metadata.getRight() - 39, metadata.getY() + 3, 34, 34);
    }

private:
    juce::Rectangle<float> cardBounds() const {
        return getLocalBounds().toFloat()
                .withTrimmedLeft((float) contentInset)
                .withTrimmedRight((float) contentInset)
                .withTrimmedTop((float) contentInset)
                .withHeight((float) heroHeight);
    }

    PresetThumbnailCache& thumbnails;
    PresetLibraryRecord record;
    TrashButton trash;
    bool hasRecord {};
};

class InlinePresetBrowser::CompactList final : public juce::Component {
public:
    using Callback = std::function<void()>;

    explicit CompactList(PresetThumbnailCache& thumbnailsToUse) :
            thumbnails(thumbnailsToUse) {
        setComponentID("workspace.sidebar.list");
    }

    void setResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices) {
        juce::File selectedFile;
        if (const auto* current = selectedRecord()) {
            selectedFile = current->file;
        }
        library = records;
        indices = visibleIndices;
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

    void setCallbacks(
            Callback selectionCallback,
            Callback openCallback) {
        onSelection = std::move(selectionCallback);
        onOpen = std::move(openCallback);
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

    void mouseDown(const juce::MouseEvent& event) override {
        const int hit = indexAt(event.getPosition());
        if (hit >= 0) {
            select(hit);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override {
        const int hit = indexAt(event.getPosition());
        if (hit < 0) {
            return;
        }
        select(hit);
        if (onOpen) {
            onOpen();
        }
    }

private:
    juce::Rectangle<int> rowBounds(int index) const {
        return {
                contentInset,
                contentInset + index * (rowHeight + rowGap),
                getWidth() - contentInset * 2,
                rowHeight
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
        if (onSelection) {
            onSelection();
        }
    }

    void updateHeight() {
        const int height = contentInset * 2
                + (int) indices.size() * (rowHeight + rowGap);
        setSize(juce::jmax(1, getWidth()), juce::jmax(1, height));
    }

    void paintRow(
            juce::Graphics& graphics,
            int visibleIndex,
            juce::Rectangle<float> bounds) {
        const auto& record = library[(size_t) indices[(size_t) visibleIndex]];
        const bool isSelected = visibleIndex == selected;
        graphics.setColour(isSelected
                ? CanvasChromePalette::raisedSurface
                : CanvasChromePalette::dockSurface.withAlpha(0.7f));
        graphics.fillRoundedRectangle(bounds, 5.f);
        if (isSelected) {
            graphics.setColour(CanvasChromePalette::navigationAccent);
            graphics.drawRoundedRectangle(bounds.reduced(1.f), 5.f, 2.f);
        }

        auto content = bounds.reduced(6.f);
        auto preview = content.removeFromLeft(124.f);
        PresetBrowserPainting::drawPreview(graphics, record, thumbnails, preview);
        content.removeFromLeft(9.f);
        auto name = content.removeFromTop(23.f);
        graphics.setColour(CanvasChromePalette::text);
        graphics.setFont(juce::FontOptions(13.f).withStyle("Bold"));
        graphics.drawFittedText(
                record.name,
                name.toNearestInt(),
                juce::Justification::centredLeft,
                1);

        auto tags = content.removeFromTop(21.f);
        for (int tag = 0; tag < juce::jmin(2, record.presentation.tags.size()); ++tag) {
            const float width = juce::jlimit(
                    40.f, 70.f, 16.f + (float) record.presentation.tags[tag].length() * 5.5f);
            drawTag(graphics, record.presentation.tags[tag], tags.removeFromLeft(width));
            tags.removeFromLeft(5.f);
        }
    }

    PresetThumbnailCache& thumbnails;
    std::vector<PresetLibraryRecord> library;
    std::vector<int> indices;
    Callback onSelection;
    Callback onOpen;
    int selected { -1 };
};

InlinePresetBrowser::InlinePresetBrowser(
        std::vector<juce::File> directories,
        OpenCallback openCallback,
        ActionCallback browseCallback,
        ActionCallback newGuideCallback,
        TabCallback tabCallback,
        DeleteCallback deleteCallback,
        ConfirmDeleteCallback confirmDeleteCallback) :
        onOpen(std::move(openCallback))
    ,   onBrowse(std::move(browseCallback))
    ,   onNewGuide(std::move(newGuideCallback))
    ,   onTabChanged(std::move(tabCallback))
    ,   onDelete(std::move(deleteCallback))
    ,   onConfirmDelete(std::move(confirmDeleteCallback))
    ,   selectedPreview(std::make_unique<SelectedPresetCard>(
                thumbnails,
                [this] { requestDeleteSelected(); }))
    ,   list(std::make_unique<CompactList>(thumbnails)) {
    setComponentID("workspace.presetSidebar");
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    styleTabButton(curves);
    styleTabButton(presets);
    curves.setComponentID("workspace.sidebar.curves");
    presets.setComponentID("workspace.sidebar.presets");
    curves.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Curves); };
    presets.onClick = [this] { setActiveTab(WorkspaceSidebarTab::Presets); };
    addAndMakeVisible(curves);
    addAndMakeVisible(presets);

    addGuide.setComponentID("workspace.sidebar.addGuide");
    addGuide.onClick = [this] { onNewGuide(); };
    addGuide.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::restingControlSurface);
    addGuide.setColour(juce::TextButton::buttonOnColourId,
            CanvasChromePalette::border);
    addGuide.setColour(juce::TextButton::textColourOffId,
            CanvasChromePalette::text);
    addAndMakeVisible(addGuide);

    search.setComponentID("workspace.sidebar.search");
    search.setTextToShowWhenEmpty("Search presets...", CanvasChromePalette::mutedText);
    search.setColour(juce::TextEditor::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    search.setColour(juce::TextEditor::outlineColourId, CanvasChromePalette::border);
    search.setColour(juce::TextEditor::focusedOutlineColourId,
            CanvasChromePalette::navigationAccent);
    search.setColour(juce::TextEditor::textColourId, CanvasChromePalette::text);
    search.setFont(juce::FontOptions(15.f));
    search.setIndents(36, 11);
    search.addListener(this);
    search.addKeyListener(this);
    addAndMakeVisible(search);

    styleFilterButton(all);
    styleFilterButton(factory);
    styleFilterButton(user);
    all.setComponentID("workspace.sidebar.all");
    factory.setComponentID("workspace.sidebar.factory");
    user.setComponentID("workspace.sidebar.user");
    all.onClick = [this] { setPackFilter(PackFilter::All); };
    factory.onClick = [this] { setPackFilter(PackFilter::Factory); };
    user.onClick = [this] { setPackFilter(PackFilter::User); };
    addAndMakeVisible(all);
    addAndMakeVisible(factory);
    addAndMakeVisible(user);

    list->setCallbacks(
            [this] { updateSelectedPreview(); },
            [this] { openSelected(); });
    viewport.setComponentID("workspace.sidebar.viewport");
    viewport.setViewedComponent(list.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(juce::ScrollBar::thumbColourId,
            CanvasChromePalette::strongBorder.withAlpha(0.72f));
    addAndMakeVisible(*selectedPreview);
    addAndMakeVisible(viewport);

    status.setFont(juce::FontOptions(11.f).withStyle("Bold"));
    status.setColour(juce::Label::textColourId, CanvasChromePalette::mutedText);
    addAndMakeVisible(status);
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
            safeThis->selectedPreview->repaint();
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
    setPackFilter(PackFilter::All);
    updateVisibility();
}

InlinePresetBrowser::~InlinePresetBrowser() {
    thumbnails.setReadyCallback({});
    setLookAndFeel(nullptr);
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

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
InlinePresetBrowser::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets {
            { "workspace.sidebar.curves", curves.getBounds().toFloat() },
            { "workspace.sidebar.presets", presets.getBounds().toFloat() }
    };
    if (tab == WorkspaceSidebarTab::Curves) {
        targets.push_back({ "workspace.sidebar.addGuide", addGuide.getBounds().toFloat() });
    } else {
        targets.push_back({ "workspace.sidebar.search", search.getBounds().toFloat() });
        targets.push_back({ "workspace.sidebar.browse", browse.getBounds().toFloat() });
    }
    return targets;
}

bool InlinePresetBrowser::hitTest(int x, int y) {
    return tab == WorkspaceSidebarTab::Presets
            || juce::Rectangle<int>(0, 0, getWidth(), 46).contains(x, y);
}

void InlinePresetBrowser::paint(juce::Graphics& graphics) {
    const auto bounds = getLocalBounds().toFloat();
    if (tab == WorkspaceSidebarTab::Presets) {
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.fillRect(bounds);
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.78f));
        graphics.drawVerticalLine(0, bounds.getY(), bounds.getBottom());
    }
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRect(bounds.withHeight(46.f));
    const auto selectedTab = tab == WorkspaceSidebarTab::Curves
            ? curves.getBounds().toFloat()
            : presets.getBounds().toFloat();
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.fillRoundedRectangle(
            selectedTab.withY(43.f).withHeight(3.f).reduced(8.f, 0.f),
            1.5f);
}

void InlinePresetBrowser::resized() {
    auto bounds = getLocalBounds();
    auto tabs = bounds.removeFromTop(46).reduced(8, 0);
    addGuide.setBounds(tabs.removeFromRight(34).reduced(2, 7));
    const int tabWidth = juce::jmin(100, tabs.getWidth() / 2);
    curves.setBounds(tabs.removeFromLeft(tabWidth));
    presets.setBounds(tabs.removeFromLeft(tabWidth));
    if (tab != WorkspaceSidebarTab::Presets) {
        return;
    }

    auto footer = bounds.removeFromBottom(54).reduced(10, 8);
    browse.setBounds(footer.removeFromRight(142));
    status.setBounds(footer);
    bounds.reduce(10, 10);
    search.setBounds(bounds.removeFromTop(42));
    bounds.removeFromTop(9);
    auto filters = bounds.removeFromTop(34);
    all.setBounds(filters.removeFromLeft(64));
    filters.removeFromLeft(7);
    factory.setBounds(filters.removeFromLeft(82));
    filters.removeFromLeft(7);
    user.setBounds(filters.removeFromLeft(70));
    bounds.removeFromTop(9);
    selectedPreview->setBounds(bounds.removeFromTop(heroHeight + contentInset));
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
        juce::Component*) {
    if (tab != WorkspaceSidebarTab::Presets) {
        return false;
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
    status.setText("FILTERING...", juce::dontSendNotification);
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
    applyPackFilter();
}

void InlinePresetBrowser::applyPackFilter() {
    std::vector<int> filtered;
    filtered.reserve(searchResults.size());
    for (const int indexToCheck : searchResults) {
        const bool factoryPreset = library[(size_t) indexToCheck]
                .presentation.pack.equalsIgnoreCase("Factory");
        if (packFilter == PackFilter::All
                || (packFilter == PackFilter::Factory && factoryPreset)
                || (packFilter == PackFilter::User && !factoryPreset)) {
            filtered.push_back(indexToCheck);
        }
    }
    list->setResults(library, filtered);
    updateSelectedPreview();
    const int width = juce::jmax(1, viewport.getMaximumVisibleWidth());
    list->setSize(width, list->getHeight());
    status.setText(
            juce::String(filtered.size()) + " PRESETS",
            juce::dontSendNotification);
    if (!library.empty() && !library.front().metadataReady) {
        status.setText("LOADING " + juce::String(library.size()) + " PRESETS...",
                juce::dontSendNotification);
    }
}

void InlinePresetBrowser::updateSelectedPreview() {
    selectedPreview->setRecord(list->selectedRecord());
}

void InlinePresetBrowser::setPackFilter(PackFilter nextFilter) {
    packFilter = nextFilter;
    all.setToggleState(packFilter == PackFilter::All, juce::dontSendNotification);
    factory.setToggleState(packFilter == PackFilter::Factory, juce::dontSendNotification);
    user.setToggleState(packFilter == PackFilter::User, juce::dontSendNotification);
    applyPackFilter();
}

void InlinePresetBrowser::openSelected() {
    const PresetLibraryRecord* record = list->selectedRecord();
    if (record == nullptr) {
        return;
    }
    if (!onOpen(record->file)) {
        status.setText("UNABLE TO LOAD PRESET", juce::dontSendNotification);
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
            "“" + name + "” will be removed from the preset library and moved to Trash.",
            "Move to Trash",
            "Cancel",
            this,
            juce::ModalCallbackFunction::create([
                    completion = std::move(completion)](int result) mutable {
                completion(result != 0);
            }));
}

void InlinePresetBrowser::deletePreset(const juce::File& file) {
    const bool deleted = onDelete ? onDelete(file) : file.moveToTrash();
    if (!deleted) {
        status.setText("UNABLE TO DELETE PRESET", juce::dontSendNotification);
        return;
    }
    status.setText("PRESET MOVED TO TRASH", juce::dontSendNotification);
    index->start();
}

void InlinePresetBrowser::updateVisibility() {
    const bool showingPresets = tab == WorkspaceSidebarTab::Presets;
    curves.setToggleState(!showingPresets, juce::dontSendNotification);
    presets.setToggleState(showingPresets, juce::dontSendNotification);
    curves.setColour(
            juce::TextButton::textColourOffId,
            showingPresets ? CanvasChromePalette::mutedText : CanvasChromePalette::text);
    curves.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    presets.setColour(
            juce::TextButton::textColourOffId,
            showingPresets ? CanvasChromePalette::text : CanvasChromePalette::mutedText);
    presets.setColour(juce::TextButton::textColourOnId, CanvasChromePalette::text);
    addGuide.setVisible(!showingPresets);
    search.setVisible(showingPresets);
    all.setVisible(showingPresets);
    factory.setVisible(showingPresets);
    user.setVisible(showingPresets);
    selectedPreview->setVisible(showingPresets);
    viewport.setVisible(showingPresets);
    status.setVisible(showingPresets);
    browse.setVisible(showingPresets);
    resized();
}

void InlinePresetBrowser::styleTabButton(juce::TextButton& button) {
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::mutedText);
}

void InlinePresetBrowser::styleFilterButton(juce::TextButton& button) {
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::restingControlSurface);
    button.setColour(juce::TextButton::buttonOnColourId,
            CanvasChromePalette::navigationAccent);
    button.setColour(juce::TextButton::textColourOffId, CanvasChromePalette::text);
    button.setColour(juce::TextButton::textColourOnId,
            CanvasChromePalette::canvasBackground);
}

}
