#include "UI/PresetBrowserPage.h"

#include "Graph/PresetTagStore.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

PresetBrowserPage::PresetBrowserPage(
        std::vector<File> directories,
        OpenCallback openCallback,
        std::function<void()> browseCallback,
        std::function<void()> closeCallback,
        std::function<void()> playbackToggleCallback,
        std::function<void(const File&, const StringArray&)> tagsChangedCallback) :
        onOpen   (std::move(openCallback))
    ,   onBrowse (std::move(browseCallback))
    ,   onClose  (std::move(closeCallback))
    ,   onTogglePlayback (std::move(playbackToggleCallback))
    ,   onTagsChanged (std::move(tagsChangedCallback))
    ,   grid     (thumbnails)
    ,   detail   (thumbnails) {
    setLookAndFeel(&browserLookAndFeel);
    setComponentID("presetBrowser");
    setWantsKeyboardFocus(true);

    title.setText("PRESET LIBRARY", dontSendNotification);
    title.setFont(FontOptions(19.f).withStyle("Bold"));
    title.setColour(Label::textColourId, CanvasChromePalette::text);
    addAndMakeVisible(title);
    subtitle.setText("Cycle 2", dontSendNotification);
    subtitle.setFont(FontOptions(11.f));
    subtitle.setColour(Label::textColourId, CanvasChromePalette::mutedText);
    addAndMakeVisible(subtitle);

    search.setComponentID("presetBrowser.search");
    search.setPlaybackToggleCallback([this] {
        if (onTogglePlayback) {
            onTogglePlayback();
        }
    });
    search.addListener(this);
    search.addKeyListener(this);
    addAndMakeVisible(search);

    sidebar.setComponentID("presetBrowser.sidebar");
    addAndMakeVisible(sidebar);
    grid.setComponentID("presetBrowser.grid");
    grid.setCallbacks(
            [this] { updateSelection(); },
            [this] { openSelected(); });
    viewport.setComponentID("presetBrowser.viewport");
    viewport.setViewedComponent(&grid, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(ScrollBar::thumbColourId,
            CanvasChromePalette::strongBorder.withAlpha(0.65f));
    addAndMakeVisible(viewport);
    detail.setComponentID("presetBrowser.detail");
    addAndMakeVisible(detail);

    status.setComponentID("presetBrowser.status");
    status.setText("INDEXING PRESETS...", dontSendNotification);
    status.setFont(FontOptions(10.f).withStyle("Bold"));
    status.setColour(Label::textColourId, CanvasChromePalette::mutedText);
    addAndMakeVisible(status);
    browse.setComponentID("presetBrowser.browse");
    browse.onClick = [this] { onBrowse(); };
    addAndMakeVisible(browse);
    open.setComponentID("presetBrowser.open");
    open.onClick = [this] { openSelected(); };
    addAndMakeVisible(open);
    editTags.setComponentID("presetBrowser.editTags");
    editTags.onClick = [this] { editSelectedTags(); };
    addAndMakeVisible(editTags);
    close.setComponentID("presetBrowser.close");
    close.onClick = [this] { onClose(); };
    addAndMakeVisible(close);

    auto styleSecondaryButton = [](TextButton& button) {
        button.setColour(TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button.setColour(TextButton::buttonOnColourId,
                CanvasChromePalette::strongBorder.withAlpha(0.72f));
        button.setColour(TextButton::textColourOffId,
                CanvasChromePalette::text.withAlpha(0.92f));
    };
    styleSecondaryButton(browse);
    styleSecondaryButton(close);
    styleSecondaryButton(editTags);
    open.setColour(TextButton::buttonColourId,
            CanvasChromePalette::navigationAccent);
    open.setColour(TextButton::buttonOnColourId,
            CanvasChromePalette::navigationAccent.brighter(0.16f));
    open.setColour(TextButton::textColourOffId,
            CanvasChromePalette::canvasBackground);
    open.setColour(TextButton::textColourOnId,
            CanvasChromePalette::canvasBackground);

    thumbnails.setReadyCallback([safeThis = SafePointer<PresetBrowserPage>(this)] {
        if (safeThis != nullptr) {
            safeThis->grid.repaint();
            safeThis->detail.repaint();
        }
    });

    index = std::make_unique<PresetLibraryIndex>(
            std::move(directories),
            [safeThis = SafePointer<PresetBrowserPage>(this)](
                    const auto& records, const auto& visibleIndices) {
                if (safeThis != nullptr) {
                    safeThis->receiveResults(records, visibleIndices);
                }
            });
    index->start();
}

PresetBrowserPage::~PresetBrowserPage() {
    thumbnails.setReadyCallback({});
    setLookAndFeel(nullptr);
}

void PresetBrowserPage::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::canvasBackground);
    graphics.setColour(CanvasChromePalette::surface);
    graphics.fillRect(getLocalBounds().removeFromTop(74));
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.75f));
    graphics.drawHorizontalLine(73, 0.f, (float) getWidth());
    graphics.drawHorizontalLine(getHeight() - 53, 0.f, (float) getWidth());
}

void PresetBrowserPage::resized() {
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(74).reduced(20, 14);
    auto titleArea = header.removeFromLeft(220);
    title.setBounds(titleArea.removeFromTop(25));
    subtitle.setBounds(titleArea);
    search.setBounds(header.withSizeKeepingCentre(jmin(620, header.getWidth()), 36));

    auto footer = bounds.removeFromBottom(54).reduced(18, 9);
    close.setBounds(footer.removeFromRight(86));
    footer.removeFromRight(9);
    browse.setBounds(footer.removeFromRight(120));
    status.setBounds(footer);

    sidebar.setBounds(bounds.removeFromLeft(174));
    const auto detailBounds = bounds.removeFromRight(258);
    detail.setBounds(detailBounds);
    auto detailActions = detailBounds.reduced(20).removeFromBottom(40);
    editTags.setBounds(detailActions.removeFromLeft(100));
    detailActions.removeFromLeft(8);
    open.setBounds(detailActions);
    viewport.setBounds(bounds);
    const int gridWidth = jmax(244, viewport.getMaximumVisibleWidth());
    grid.setSize(gridWidth, grid.contentHeightForWidth(gridWidth));
}

void PresetBrowserPage::visibilityChanged() {
    if (!isVisible()) {
        return;
    }
    Timer::callAfterDelay(0, [safeSearch = SafePointer<TextEditor>(&search)] {
        if (safeSearch != nullptr) {
            safeSearch->grabKeyboardFocus();
        }
    });
}

bool PresetBrowserPage::keyPressed(const KeyPress& key) {
    return keyPressed(key, this);
}

bool PresetBrowserPage::keyPressed(const KeyPress& key, Component* source) {
    if (source == &search && search.handlePlaybackSpace(key)) {
        return true;
    }
    if (key.getKeyCode() == KeyPress::spaceKey
            && !key.getModifiers().isCommandDown()
            && !key.getModifiers().isCtrlDown()
            && !key.getModifiers().isAltDown()
            && onTogglePlayback) {
        if (source == &search || search.hasKeyboardFocus(true)) {
            return false;
        }
        onTogglePlayback();
        return true;
    }
    if (key == KeyPress::escapeKey) {
        onClose();
        return true;
    }
    if (key == KeyPress::returnKey) {
        openSelected();
        return true;
    }
    if (key == KeyPress::leftKey) {
        grid.moveSelection(-1, 0);
        return true;
    }
    if (key == KeyPress::rightKey) {
        grid.moveSelection(1, 0);
        return true;
    }
    if (key == KeyPress::upKey) {
        grid.moveSelection(0, -1);
        return true;
    }
    if (key == KeyPress::downKey) {
        grid.moveSelection(0, 1);
        return true;
    }
    return false;
}

void PresetBrowserPage::textEditorTextChanged(TextEditor&) {
    status.setText("FILTERING...", dontSendNotification);
    index->setQuery(search.getText());
}

void PresetBrowserPage::textEditorReturnKeyPressed(TextEditor&) {
    openSelected();
}

void PresetBrowserPage::receiveResults(
        const std::vector<PresetLibraryRecord>& records,
        const std::vector<int>& visibleIndices) {
    sidebar.setRecords(records);
    grid.setResults(records, visibleIndices);
    const int width = jmax(244, viewport.getMaximumVisibleWidth());
    grid.setSize(width, grid.contentHeightForWidth(width));
    status.setText(String(visibleIndices.size()) + " OF " + String(records.size()) + " PRESETS",
            dontSendNotification);
    if (!records.empty() && !records.front().metadataReady) {
        status.setText(String(records.size()) + " PRESETS  ·  LOADING DETAILS",
                dontSendNotification);
    }
    updateSelection();
}

void PresetBrowserPage::updateSelection() {
    const auto* record = grid.selectedRecord();
    detail.setRecord(record);
    open.setEnabled(record != nullptr);
    editTags.setEnabled(record != nullptr && record->metadataReady);
}

void PresetBrowserPage::openSelected() {
    const auto* record = grid.selectedRecord();
    if (record == nullptr) {
        return;
    }
    if (onOpen(record->file)) {
        onClose();
        return;
    }
    AlertWindow::showMessageBoxAsync(
            MessageBoxIconType::WarningIcon,
            "Unable to open preset",
            "The selected preset could not be loaded.");
}

void PresetBrowserPage::editSelectedTags() {
    const auto* record = grid.selectedRecord();
    if (record == nullptr) {
        return;
    }
    const auto file = record->file;
    auto* prompt = new AlertWindow(
            "Edit preset tags",
            "Separate tags with commas.",
            MessageBoxIconType::QuestionIcon,
            getTopLevelComponent());
    prompt->addTextEditor("tags",
            record->presentation.tags.joinIntoString(", "), "Tags");
    prompt->addButton("Save", 1, KeyPress(KeyPress::returnKey));
    prompt->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
    prompt->enterModalState(true,
            ModalCallbackFunction::create([
                    safeThis = SafePointer<PresetBrowserPage>(this),
                    prompt, file](int result) {
                if (result != 1 || safeThis == nullptr) {
                    return;
                }
                StringArray tags;
                tags.addTokens(prompt->getTextEditorContents("tags"), ",", "\"");
                tags = PresetTagStore::normalize(std::move(tags));
                String error;
                if (!PresetTagStore::save(file, tags, error)) {
                    AlertWindow::showMessageBoxAsync(
                            MessageBoxIconType::WarningIcon,
                            "Unable to save tags", error);
                    return;
                }
                safeThis->index->refreshRecord(file);
                if (safeThis->onTagsChanged) {
                    safeThis->onTagsChanged(file, tags);
                }
            }), true);
}

}
