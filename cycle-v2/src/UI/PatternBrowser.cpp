#include "UI/PatternBrowser.h"

#include "UI/CanvasChromePalette.h"
#include "UI/MidiPatternMiniMap.h"

namespace CycleV2 {

namespace {

constexpr int rowHeight = 84;

}

class PatternBrowser::List final : public juce::Component {
public:
    List(SelectCallback select, EditCallback edit) :
            onSelect(std::move(select))
        ,   onEdit(std::move(edit)) {
        setComponentID("workspace.sidebar.patternList");
    }

    void setRecords(std::vector<PatternRecord> next, const juce::String& id) {
        records = std::move(next);
        selectedId = id;
        setSize(getWidth(), (int) records.size() * rowHeight);
        repaint();
    }

    const juce::String& selection() const { return selectedId; }

    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation(
                    const juce::Rectangle<int>& viewportBounds,
                    int viewPositionY) const {
        std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
        for (int index = 0; index < (int) records.size(); ++index) {
            const auto row = juce::Rectangle<float>(
                    (float) viewportBounds.getX(),
                    (float) viewportBounds.getY() + index * rowHeight - viewPositionY,
                    (float) viewportBounds.getWidth(), (float) rowHeight);
            if (row.intersects(viewportBounds.toFloat())) {
                targets.push_back({ "workspace.sidebar.pattern." + records[(size_t) index].id,
                        row.getIntersection(viewportBounds.toFloat()) });
            }
        }
        return targets;
    }

    void paint(juce::Graphics& graphics) override {
        for (int index = 0; index < (int) records.size(); ++index) {
            const auto& record = records[(size_t) index];
            const auto row = getLocalBounds().withY(index * rowHeight)
                    .withHeight(rowHeight).reduced(7, 3).toFloat();
            const bool selected = record.id == selectedId;
            graphics.setColour(selected
                    ? CanvasChromePalette::raisedSurface
                    : CanvasChromePalette::surface);
            graphics.fillRoundedRectangle(row, 5.f);
            graphics.setColour(selected
                    ? CanvasChromePalette::navigationAccent
                    : CanvasChromePalette::border.withAlpha(0.55f));
            graphics.drawRoundedRectangle(row, 5.f, selected ? 1.2f : 0.8f);
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(juce::FontOptions(13.f, juce::Font::bold));
            graphics.drawFittedText(record.name,
                    row.withHeight(22.f).withTrimmedRight(78.f)
                            .reduced(8.f, 0.f).toNearestInt(),
                    juce::Justification::centredLeft, 1);
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(juce::FontOptions(9.f));
            graphics.drawText(record.tag.isNotEmpty() ? record.tag.toUpperCase()
                            : (record.factory ? "FACTORY" : "USER"),
                    row.getRight() - 75.f, row.getY() + 7.f, 65.f, 13.f,
                    juce::Justification::centredRight);
            const auto preview = row.withTrimmedTop(20.f)
                    .withTrimmedBottom(4.f).reduced(8.f, 0.f);
            graphics.setColour(CanvasChromePalette::insetBackground);
            graphics.fillRoundedRectangle(preview, 3.f);
            MidiPatternMiniMap::paintNotes(graphics, record.sequence,
                    preview.withTrimmedBottom(10.f),
                    MidiPatternMiniMap::pitchRange(record.sequence, 12, 0));
            MidiPatternMiniMap::paintControls(graphics, record.sequence,
                    preview.withTrimmedTop(preview.getHeight() - 10.f));
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        const int index = event.y / rowHeight;
        if (index < 0 || index >= (int) records.size()) {
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
    std::vector<PatternRecord> records;
    juce::String selectedId;
};

PatternBrowser::PatternBrowser(
        SelectCallback select,
        EditCallback edit,
        CreateCallback create) :
        onEdit(std::move(edit))
    ,   onCreate(std::move(create))
    ,   list(std::make_unique<List>(std::move(select), onEdit)) {
    setComponentID("workspace.sidebar.patternBrowser");
    createButton.setComponentID("workspace.sidebar.patternNew");
    editButton.setComponentID("workspace.sidebar.patternEdit");
    nameEntry.setTextToShowWhenEmpty("Pattern name",
            CanvasChromePalette::mutedText);
    nameEntry.setComponentID("workspace.sidebar.patternName");
    nameEntry.setColour(juce::TextEditor::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    nameEntry.setColour(juce::TextEditor::textColourId,
            CanvasChromePalette::text);
    nameEntry.setColour(juce::TextEditor::outlineColourId,
            CanvasChromePalette::border);
    nameEntry.setColour(juce::TextEditor::focusedOutlineColourId,
            CanvasChromePalette::focus);
    nameEntry.setFont(juce::FontOptions(13.f));
    nameEntry.setIndents(10, 6);
    nameEntry.onReturnKey = [this] { createButton.triggerClick(); };
    addAndMakeVisible(nameEntry);
    createButton.setTooltip("Save the current MIDI phrase with the selected type");
    typeFilter.setTooltip("Filter patterns and choose the type for a new pattern");
    createButton.onClick = [this] {
        const auto name = nameEntry.getText().trim();
        if (name.isNotEmpty()) {
            onCreate(name, typeFilter.getSelectedId() > 1
                    ? typeFilter.getText() : juce::String("Other"));
            nameEntry.clear();
        } else {
            nameEntry.grabKeyboardFocus();
        }
    };
    editButton.onClick = [this] { editSelected(); };
    typeFilter.setComponentID("workspace.sidebar.patternType");
    typeFilter.addItem("All types", 1);
    for (const auto& tag : { "Bass", "Lead", "Pad", "Keys", "Sustained", "Rhythm", "Other" }) {
        typeFilter.addItem(tag, typeFilter.getNumItems() + 2);
    }
    typeFilter.setSelectedId(1, juce::dontSendNotification);
    typeFilter.onChange = [this] { applyFilter(); };
    typeFilter.setColour(juce::ComboBox::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    typeFilter.setColour(juce::ComboBox::textColourId,
            CanvasChromePalette::text);
    typeFilter.setColour(juce::ComboBox::outlineColourId,
            CanvasChromePalette::border);
    addAndMakeVisible(typeFilter);
    for (auto* button : { &createButton, &editButton }) {
        button->setColour(juce::TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button->setColour(juce::TextButton::textColourOffId,
                CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }
    status.setColour(juce::Label::textColourId,
            CanvasChromePalette::mutedText);
    status.setFont(juce::FontOptions(11.f));
    addAndMakeVisible(status);
    viewport.setViewedComponent(list.get(), false);
    viewport.setComponentID("workspace.sidebar.patternViewport");
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

PatternBrowser::~PatternBrowser() = default;

void PatternBrowser::setRecords(
        std::vector<PatternRecord> records,
        const juce::String& selectedId) {
    allRecords = std::move(records);
    this->selectedId = selectedId;
    applyFilter();
}

void PatternBrowser::applyFilter() {
    const int previousScroll = viewport.getViewPositionY();
    std::vector<PatternRecord> visible;
    const auto tag = typeFilter.getSelectedId() > 1
            ? typeFilter.getText() : juce::String();
    for (const auto& record : allRecords) {
        if (tag.isEmpty() || record.tag.equalsIgnoreCase(tag)
                || (tag == "Other" && record.tag.isEmpty())) {
            visible.push_back(record);
        }
    }
    status.setText(juce::String(visible.size()) + " PATTERNS",
            juce::dontSendNotification);
    list->setRecords(std::move(visible), selectedId);
    resized();
    viewport.setViewPosition(0, previousScroll);
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
PatternBrowser::pointerTargetsForAutomation() const {
    return list->pointerTargetsForAutomation(
            viewport.getBounds(), viewport.getViewPositionY());
}

void PatternBrowser::editSelected() {
    if (list->selection().isNotEmpty()) {
        onEdit(list->selection());
    }
}

void PatternBrowser::resized() {
    auto bounds = getLocalBounds().reduced(10, 7);
    auto createRow = bounds.removeFromTop(30);
    createButton.setBounds(createRow.removeFromRight(69));
    createRow.removeFromRight(5);
    nameEntry.setBounds(createRow);
    bounds.removeFromTop(6);
    auto filterRow = bounds.removeFromTop(30);
    editButton.setBounds(filterRow.removeFromRight(69));
    filterRow.removeFromRight(5);
    typeFilter.setBounds(filterRow);
    status.setBounds(bounds.removeFromTop(24));
    viewport.setBounds(bounds);
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

}
