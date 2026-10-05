#include "UI/PatternBrowser.h"

#include "UI/CanvasChromePalette.h"
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
                        SidebarMediaRow::favoriteBounds(row) });
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
            const auto preview = SidebarMediaRow::paintFrame(graphics, row, selected);
            {
                juce::Graphics::ScopedSaveState save(graphics);
                juce::Path clip;
                clip.addRoundedRectangle(preview, 3.f);
                graphics.reduceClipRegion(clip);
                MidiPatternMiniMap::paintNotes(graphics, record.sequence,
                        preview,
                        MidiPatternMiniMap::pitchRange(record.sequence, 12, 0));
            }
            SidebarMediaRow::paintLabels(graphics, row, record.name,
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
        if (SidebarMediaRow::favoriteBounds(row).contains(event.position)
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
        LibraryFavorites* favoriteStore) :
        onEdit(std::move(edit))
    ,   onCreate(std::move(create))
    ,   favorites(favoriteStore)
    ,   list(std::make_unique<List>(std::move(select), onEdit,
                [this](const juce::String& id) { toggleFavorite(id); },
                favorites)) {
    setComponentID("workspace.sidebar.patternBrowser");
    createButton.setComponentID("workspace.sidebar.patternNew");
    editButton.setComponentID("workspace.sidebar.patternEdit");
    search.setComponentID("workspace.sidebar.patternSearch");
    search.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(search);
    createButton.setTooltip("Save the current MIDI phrase as a new pattern");
    createButton.onClick = [this] { createPattern(); };
    editButton.onClick = [this] { editSelected(); };
    SidebarTagCloud::styleHeading(tagHeading);
    addAndMakeVisible(tagHeading);
    tagCloud.setComponentID("workspace.sidebar.patternTags");
    tagCloud.setChangeCallback([this] { applyFilter(); });
    addAndMakeVisible(tagCloud);
    favoritesOnly.setComponentID("workspace.sidebar.patternFavoritesOnly");
    favoritesOnly.setClickingTogglesState(true);
    favoritesOnly.setTooltip("Show only favorite patterns");
    favoritesOnly.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::restingControlSurface);
    favoritesOnly.setColour(juce::TextButton::buttonOnColourId,
            CanvasChromePalette::navigationAccent.withAlpha(0.3f));
    favoritesOnly.setColour(juce::TextButton::textColourOffId,
            CanvasChromePalette::text);
    favoritesOnly.onClick = [this] { applyFilter(); };
    addAndMakeVisible(favoritesOnly);
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
    juce::StringArray available;
    for (const auto& record : allRecords) {
        for (const auto& tag : tagsFor(record)) {
            available.addIfNotAlreadyThere(tag);
        }
    }
    tagCloud.setTags(std::move(available));
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
    prompt->addTextEditor("tags",
            tagCloud.selectedTags().isEmpty()
                    ? juce::String("Other")
                    : tagCloud.selectedTags().joinIntoString(", "),
            "Tags (comma separated)");
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

void PatternBrowser::applyFilter() {
    const int previousScroll = viewport.getViewPositionY();
    std::vector<PatternRecord> visible;
    const auto query = search.getText().trim();
    for (const auto& record : allRecords) {
        const auto tags = tagsFor(record);
        const bool matchesTags = tagCloud.matches(tags);
        const bool matchesQuery = query.isEmpty()
                || record.name.containsIgnoreCase(query)
                || tags.joinIntoString(" ").containsIgnoreCase(query);
        if (matchesTags && matchesQuery
                && (!favoritesOnly.getToggleState()
                        || (favorites != nullptr && favorites->isPatternFavorite(record.id)))) {
            visible.push_back(record);
        }
    }
    list->setRecords(std::move(visible), selectedId);
    resized();
    viewport.setViewPosition(0, previousScroll);
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
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets {
            { "workspace.sidebar.patternSearch", search.getBounds().toFloat() },
            { "workspace.sidebar.patternNew", createButton.getBounds().toFloat() },
            { "workspace.sidebar.patternEdit", editButton.getBounds().toFloat() },
            { "workspace.sidebar.patternFavoritesOnly",
                    favoritesOnly.getBounds().toFloat() }
    };
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
    auto headingRow = bounds.removeFromTop(24);
    editButton.setBounds(headingRow.removeFromRight(69));
    headingRow.removeFromRight(4);
    favoritesOnly.setBounds(headingRow.removeFromRight(82));
    tagHeading.setBounds(headingRow);
    bounds.removeFromTop(4);
    const int cloudHeight = tagCloud.preferredHeightForWidth(bounds.getWidth());
    tagCloud.setBounds(bounds.removeFromTop(cloudHeight));
    bounds.removeFromTop(8);
    viewport.setBounds(bounds);
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
            list->getHeight());
}

}
