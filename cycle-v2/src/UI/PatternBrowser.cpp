#include "UI/PatternBrowser.h"

#include "UI/CanvasChromePalette.h"
#include "UI/MidiPatternMiniMap.h"

namespace CycleV2 {

namespace {

constexpr int rowHeight = 88;

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

    int selectedIndex() const {
        for (int index = 0; index < (int) records.size(); ++index) {
            if (records[(size_t) index].id == selectedId) {
                return index;
            }
        }
        return 0;
    }

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
                    : CanvasChromePalette::dockSurface);
            graphics.fillRoundedRectangle(row, 5.f);
            if (selected) {
                graphics.setColour(CanvasChromePalette::navigationAccent);
                graphics.drawRoundedRectangle(row, 5.f, 1.2f);
            }
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(juce::FontOptions(13.f, juce::Font::bold));
            graphics.drawFittedText(record.name,
                    row.withHeight(22.f).reduced(8.f, 0.f).toNearestInt(),
                    juce::Justification::centredLeft, 1);
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(juce::FontOptions(9.f));
            graphics.drawText(record.factory ? "FACTORY" : "USER",
                    row.getRight() - 61.f, row.getY() + 7.f, 51.f, 13.f,
                    juce::Justification::centredRight);
            const auto preview = row.withTrimmedTop(27.f).reduced(8.f, 6.f);
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
    nameEntry.setTextToShowWhenEmpty("New pattern name...",
            CanvasChromePalette::mutedText);
    nameEntry.setComponentID("workspace.sidebar.patternName");
    nameEntry.setColour(juce::TextEditor::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    nameEntry.setColour(juce::TextEditor::textColourId,
            CanvasChromePalette::text);
    addAndMakeVisible(nameEntry);
    createButton.onClick = [this] {
        const auto name = nameEntry.getText().trim();
        if (name.isNotEmpty()) {
            onCreate(name);
            nameEntry.clear();
        } else {
            nameEntry.grabKeyboardFocus();
        }
    };
    editButton.onClick = [this] { editSelected(); };
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
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

PatternBrowser::~PatternBrowser() = default;

void PatternBrowser::setRecords(
        std::vector<PatternRecord> records,
        const juce::String& selectedId) {
    status.setText(juce::String(records.size()) + " PATTERNS  ·  CLICK TO ASSIGN",
            juce::dontSendNotification);
    list->setRecords(std::move(records), selectedId);
    resized();
    viewport.setViewPosition(0, list->selectedIndex() * rowHeight);
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
    auto bounds = getLocalBounds().reduced(10, 8);
    nameEntry.setBounds(bounds.removeFromTop(34));
    bounds.removeFromTop(5);
    auto actions = bounds.removeFromTop(32);
    editButton.setBounds(actions.removeFromRight(70));
    actions.removeFromRight(7);
    createButton.setBounds(actions);
    status.setBounds(bounds.removeFromTop(28));
    viewport.setBounds(bounds);
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

}
