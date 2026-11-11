#include "UI/PresetCardList.h"

#include "UI/SidebarMediaRow.h"
#include "UI/CanvasChromeIcons.h"
#include "UI/CanvasChromePalette.h"
#include "UI/PresetBrowserComponents.h"

namespace CycleV2 {

namespace {
constexpr int contentInset = 10;
}

PresetCardList::PresetCardList(PresetThumbnailCache& thumbnailsToUse) :
        thumbnails(thumbnailsToUse) {
    setComponentID("workspace.sidebar.list");
}

void PresetCardList::setResults(
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

void PresetCardList::setCallbacks(Callback openCallback,
        std::function<void(const juce::File&)> favoriteCallback) {
    onOpen = std::move(openCallback);
    onFavorite = std::move(favoriteCallback);
}

void PresetCardList::setFavorites(LibraryFavorites* store) {
    favorites = store;
    repaint();
}

const PresetLibraryRecord* PresetCardList::selectedRecord() const {
    if (!juce::isPositiveAndBelow(selected, (int) indices.size())) {
        return nullptr;
    }
    const int recordIndex = indices[(size_t) selected];
    return juce::isPositiveAndBelow(recordIndex, (int) library.size())
            ? &library[(size_t) recordIndex]
            : nullptr;
}

int PresetCardList::count() const { return (int) indices.size(); }

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
        PresetCardList::pointerTargetsForAutomation(
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
            targets.push_back({ "workspace.sidebar.presetDelete."
                            + record.file.getFileNameWithoutExtension(),
                    SidebarMediaRow::deleteBounds(row) });
            targets.push_back({ "workspace.sidebar.presetFavorite."
                            + record.file.getFileNameWithoutExtension(),
                    SidebarMediaRow::favoriteBounds(row) });
        }
    }
    return targets;
}

void PresetCardList::moveSelection(int delta) {
    if (indices.empty()) {
        return;
    }
    select(juce::jlimit(0, (int) indices.size() - 1, selected + delta));
}

void PresetCardList::paint(juce::Graphics& graphics) {
    const auto clip = graphics.getClipBounds();
    for (int visibleIndex = 0; visibleIndex < (int) indices.size(); ++visibleIndex) {
        const auto bounds = rowBounds(visibleIndex).toFloat();
        if (clip.intersects(bounds.toNearestInt())) {
            paintRow(graphics, visibleIndex, bounds);
        }
    }
}

void PresetCardList::mouseMove(const juce::MouseEvent& event) {
    const int next = indexAt(event.getPosition());
    if (hovered != next) {
        hovered = next;
        repaint();
    }
}

void PresetCardList::mouseExit(const juce::MouseEvent&) {
    hovered = -1;
    repaint();
}

void PresetCardList::mouseUp(const juce::MouseEvent& event) {
    if (event.getNumberOfClicks() > 1) {
        return;
    }
    const int hit = indexAt(event.getPosition());
    if (hit >= 0) {
        if (hit == hovered && SidebarMediaRow::deleteBounds(rowBounds(hit).toFloat())
                .contains(event.position) && onDelete) {
            const auto& record = library[(size_t) indices[(size_t) hit]];
            onDelete(record.file, record.name);
            return;
        }
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

juce::Rectangle<int> PresetCardList::rowBounds(int index) const {
    return {
            0,
            contentInset + index * SidebarMediaRow::presetHeight,
            getWidth(),
            SidebarMediaRow::presetHeight
    };
}

int PresetCardList::indexAt(juce::Point<int> position) const {
    for (int index = 0; index < (int) indices.size(); ++index) {
        if (rowBounds(index).contains(position)) {
            return index;
        }
    }
    return -1;
}

void PresetCardList::select(int index) {
    if (!juce::isPositiveAndBelow(index, (int) indices.size()) || selected == index) {
        return;
    }
    selected = index;
    repaint();
    if (onSelectionChanged) {
        onSelectionChanged();
    }
}

void PresetCardList::updateHeight() {
    const int height = contentInset * 2
            + (int) indices.size() * SidebarMediaRow::presetHeight;
    setSize(juce::jmax(1, getWidth()), juce::jmax(1, height));
}

void PresetCardList::paintRow(
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
    if (hovered == visibleIndex) {
        const auto trash = SidebarMediaRow::deleteBounds(bounds);
        graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.95f));
        graphics.fillRoundedRectangle(trash, 3.f);
        CanvasChromeIcons::paintTrash(graphics, trash, CanvasChromePalette::text);
    }
}

}
