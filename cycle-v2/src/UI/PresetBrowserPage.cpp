#include "UI/PresetBrowserPage.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

PresetBrowserPage::PresetBrowserPage(
        std::vector<File> directories,
        OpenCallback openCallback,
        std::function<void()> browseCallback,
        std::function<void()> closeCallback) :
        onOpen   (std::move(openCallback))
    ,   onBrowse (std::move(browseCallback))
    ,   onClose  (std::move(closeCallback))
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
    search.setTextToShowWhenEmpty("Search presets, authors, packs, or tags",
            CanvasChromePalette::mutedText);
    search.setColour(TextEditor::backgroundColourId, CanvasChromePalette::restingControlSurface);
    search.setColour(TextEditor::outlineColourId, CanvasChromePalette::border);
    search.setColour(TextEditor::focusedOutlineColourId,
            CanvasChromePalette::navigationAccent);
    search.setColour(TextEditor::textColourId, CanvasChromePalette::text);
    search.setFont(FontOptions(15.f));
    search.setIndents(36, 8);
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
    open.setColour(TextButton::buttonColourId,
            CanvasChromePalette::navigationAccent);
    open.setColour(TextButton::buttonOnColourId,
            CanvasChromePalette::navigationAccent.brighter(0.16f));
    open.setColour(TextButton::textColourOffId,
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
    open.setBounds(detailBounds.reduced(20).removeFromBottom(40));
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

bool PresetBrowserPage::keyPressed(const KeyPress& key, Component*) {
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
    detail.setRecord(grid.selectedRecord());
    open.setEnabled(grid.selectedRecord() != nullptr);
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

}
