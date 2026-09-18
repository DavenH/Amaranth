#include "UI/PresetBrowserPage.h"

#include <algorithm>

namespace CycleV2 {

using namespace juce;

PresetBrowserPage::PresetBrowserPage(
        std::vector<File> directories,
        OpenCallback openCallback,
        std::function<void()> browseCallback,
        std::function<void()> closeCallback) :
        onOpen   (std::move(openCallback))
    ,   onBrowse (std::move(browseCallback))
    ,   onClose  (std::move(closeCallback)) {
    setComponentID("presetBrowser");
    setWantsKeyboardFocus(true);

    title.setText("PRESETS", dontSendNotification);
    title.setFont(FontOptions(19.f).withStyle("Bold"));
    title.setColour(Label::textColourId, Colour(0xffe2e8ef));
    addAndMakeVisible(title);

    search.setComponentID("presetBrowser.search");
    search.setTextToShowWhenEmpty("Search presets", Colour(0xff8190a1));
    search.addListener(this);
    addAndMakeVisible(search);

    list.setComponentID("presetBrowser.list");
    list.setModel(this);
    list.setRowHeight(42);
    list.setColour(ListBox::backgroundColourId, Colour(0xff171f28));
    list.setColour(ListBox::outlineColourId, Colour(0xff354453));
    list.setOutlineThickness(1);
    addAndMakeVisible(list);
    empty.setJustificationType(Justification::centred);
    empty.setColour(Label::textColourId, Colour(0xffaab8c8));
    addAndMakeVisible(empty);

    browse.setComponentID("presetBrowser.browse");
    browse.onClick = [this] { onBrowse(); };
    addAndMakeVisible(browse);
    open.setComponentID("presetBrowser.open");
    open.onClick = [this] { openSelected(); };
    addAndMakeVisible(open);
    close.setComponentID("presetBrowser.close");
    close.onClick = [this] { onClose(); };
    addAndMakeVisible(close);

    for (const File& directory : directories) {
        if (!directory.isDirectory()) {
            continue;
        }
        Array<File> found;
        directory.findChildFiles(found, File::findFiles, false, "*.cyclegraph");
        for (const File& file : found) {
            if (std::find(files.begin(), files.end(), file) == files.end()) {
                files.push_back(file);
            }
        }
    }
    std::sort(files.begin(), files.end(), [](const File& left, const File& right) {
        return left.getFileNameWithoutExtension().compareIgnoreCase(
                right.getFileNameWithoutExtension()) < 0;
    });
    refreshFilter();
}

void PresetBrowserPage::paint(Graphics& graphics) {
    graphics.fillAll(Colour(0xee0d141c));
    const Rectangle<float> panel = getLocalBounds().toFloat().reduced(28.f);
    graphics.setColour(Colour(0xff111922));
    graphics.fillRoundedRectangle(panel, 10.f);
    graphics.setColour(Colour(0xff3a4b5c));
    graphics.drawRoundedRectangle(panel, 10.f, 1.f);
}

void PresetBrowserPage::resized() {
    Rectangle<int> content = getLocalBounds().reduced(52);
    const int width = jmin(760, content.getWidth());
    content = Rectangle<int>(width, content.getHeight()).withCentre(content.getCentre());
    title.setBounds(content.removeFromTop(42));
    content.removeFromTop(12);
    search.setBounds(content.removeFromTop(34));
    content.removeFromTop(14);
    Rectangle<int> actions = content.removeFromBottom(38);
    close.setBounds(actions.removeFromRight(90));
    actions.removeFromRight(8);
    open.setBounds(actions.removeFromRight(120));
    browse.setBounds(actions.removeFromLeft(126));
    content.removeFromBottom(14);
    list.setBounds(content);
    empty.setBounds(content);
}

bool PresetBrowserPage::keyPressed(const KeyPress& key) {
    if (key == KeyPress::escapeKey) {
        onClose();
        return true;
    }
    if (key == KeyPress::returnKey) {
        openSelected();
        return true;
    }
    return false;
}

int PresetBrowserPage::getNumRows() {
    return (int) visibleIndices.size();
}

void PresetBrowserPage::paintListBoxItem(
        int row, Graphics& graphics, int width, int height, bool selected) {
    if (row < 0 || row >= (int) visibleIndices.size()) {
        return;
    }
    const File& file = files[(size_t) visibleIndices[(size_t) row]];
    if (selected) {
        graphics.fillAll(Colour(0xff285276));
    }
    graphics.setColour(Colour(0xffe2e8ef));
    graphics.setFont(FontOptions(13.f));
    graphics.drawText(file.getFileNameWithoutExtension(), 14, 3, width - 28, 19,
            Justification::centredLeft);
    graphics.setColour(Colour(0xff9caabb));
    graphics.setFont(FontOptions(10.f));
    graphics.drawText("Folder: " + file.getParentDirectory().getFileName(), 14, 23, width - 28,
            height - 26, Justification::centredLeft);
}

void PresetBrowserPage::listBoxItemDoubleClicked(int row, const MouseEvent&) {
    list.selectRow(row);
    openSelected();
}

void PresetBrowserPage::returnKeyPressed(int row) {
    list.selectRow(row);
    openSelected();
}

void PresetBrowserPage::textEditorTextChanged(TextEditor&) {
    refreshFilter();
}

void PresetBrowserPage::refreshFilter() {
    const String query = search.getText().trim();
    visibleIndices.clear();
    for (int index = 0; index < (int) files.size(); ++index) {
        if (query.isEmpty()
                || files[(size_t) index].getFileNameWithoutExtension().containsIgnoreCase(query)
                || files[(size_t) index].getParentDirectory().getFileName().containsIgnoreCase(query)) {
            visibleIndices.push_back(index);
        }
    }
    list.updateContent();
    empty.setText(
            files.empty() ? "No local presets found. Browse to open a file."
                    : "No matching presets.",
            dontSendNotification);
    empty.setVisible(visibleIndices.empty());
    if (!visibleIndices.empty()) {
        list.selectRow(0);
    }
}

void PresetBrowserPage::openSelected() {
    const int row = list.getSelectedRow();
    if (row < 0 || row >= (int) visibleIndices.size()) {
        return;
    }
    if (onOpen(files[(size_t) visibleIndices[(size_t) row]])) {
        onClose();
    } else {
        AlertWindow::showMessageBoxAsync(
                MessageBoxIconType::WarningIcon,
                "Unable to open preset",
                "The selected preset could not be loaded.");
    }
}

}
