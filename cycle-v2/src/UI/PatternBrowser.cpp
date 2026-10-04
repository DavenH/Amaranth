#include "UI/PatternBrowser.h"

#include "UI/CanvasChromePalette.h"
#include "UI/MidiPatternMiniMap.h"
#include "UI/SidebarMediaRow.h"
#include "UI/SidebarTypeFilter.h"

namespace CycleV2 {

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
        setSize(getWidth(), (int) records.size() * SidebarMediaRow::height);
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
                    (float) viewportBounds.getY()
                            + index * SidebarMediaRow::height - viewPositionY,
                    (float) viewportBounds.getWidth(),
                    (float) SidebarMediaRow::height);
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
            const auto row = getLocalBounds()
                    .withY(index * SidebarMediaRow::height)
                    .withHeight(SidebarMediaRow::height).toFloat();
            const bool selected = record.id == selectedId;
            const auto preview = SidebarMediaRow::paintFrame(graphics, row,
                    selected, SidebarMediaRow::PreviewPosition::BelowLabels);
            SidebarMediaRow::paintLabels(graphics, row, record.name,
                    record.tag.isNotEmpty() ? record.tag
                            : (record.factory ? "Factory" : "User"), false);
            MidiPatternMiniMap::paintNotes(graphics, record.sequence,
                    preview,
                    MidiPatternMiniMap::pitchRange(record.sequence, 12, 0));
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        const int index = event.y / SidebarMediaRow::height;
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
    search.setComponentID("workspace.sidebar.patternSearch");
    search.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(search);
    createButton.setTooltip("Save the current MIDI phrase with the selected type");
    typeFilter.setTooltip("Filter patterns and choose the type for a new pattern");
    createButton.onClick = [this] { createPattern(); };
    editButton.onClick = [this] { editSelected(); };
    typeFilter.setComponentID("workspace.sidebar.patternType");
    SidebarTypeFilter::configure(typeFilter);
    typeFilter.onChange = [this] { applyFilter(); };
    addAndMakeVisible(typeFilter);
    for (auto* button : { &createButton, &editButton }) {
        button->setColour(juce::TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button->setColour(juce::TextButton::textColourOffId,
                CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }
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

void PatternBrowser::setPlaybackToggleCallback(std::function<void()> callback) {
    search.setPlaybackToggleCallback(std::move(callback));
}

void PatternBrowser::createPattern() {
    auto* prompt = new juce::AlertWindow(
            "New pattern",
            "Name the MIDI phrase to save in the pattern library.",
            juce::MessageBoxIconType::QuestionIcon,
            getTopLevelComponent());
    prompt->addTextEditor("name", {}, "Pattern name");
    prompt->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
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
                const auto type = SidebarTypeFilter::selectedType(safeThis->typeFilter);
                safeThis->onCreate(name,
                        type.isNotEmpty() ? type : juce::String("Other"));
            }), true);
}

void PatternBrowser::applyFilter() {
    const int previousScroll = viewport.getViewPositionY();
    std::vector<PatternRecord> visible;
    const auto tag = SidebarTypeFilter::selectedType(typeFilter);
    const auto query = search.getText().trim();
    for (const auto& record : allRecords) {
        const bool matchesType = tag.isEmpty() || record.tag.equalsIgnoreCase(tag)
                || (tag == "Other" && record.tag.isEmpty());
        const bool matchesQuery = query.isEmpty()
                || record.name.containsIgnoreCase(query)
                || record.tag.containsIgnoreCase(query);
        if (matchesType && matchesQuery) {
            visible.push_back(record);
        }
    }
    list->setRecords(std::move(visible), selectedId);
    resized();
    viewport.setViewPosition(0, previousScroll);
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
PatternBrowser::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets {
            { "workspace.sidebar.patternSearch", search.getBounds().toFloat() },
            { "workspace.sidebar.patternNew", createButton.getBounds().toFloat() },
            { "workspace.sidebar.patternType", typeFilter.getBounds().toFloat() },
            { "workspace.sidebar.patternEdit", editButton.getBounds().toFloat() }
    };
    for (const auto& target : list->pointerTargetsForAutomation(
            viewport.getBounds(), viewport.getViewPositionY())) {
        targets.push_back(target);
    }
    return targets;
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
    search.setBounds(createRow);
    bounds.removeFromTop(6);
    auto filterRow = bounds.removeFromTop(30);
    editButton.setBounds(filterRow.removeFromRight(69));
    filterRow.removeFromRight(5);
    typeFilter.setBounds(filterRow);
    bounds.removeFromTop(7);
    viewport.setBounds(bounds);
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

}
