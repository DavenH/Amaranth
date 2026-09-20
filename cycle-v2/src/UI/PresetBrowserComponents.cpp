#include "UI/PresetBrowserComponents.h"

#include <algorithm>

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

constexpr int cardWidth = 218;
constexpr int cardHeight = 164;
constexpr int cardGap = 14;
constexpr int cardInset = 10;

juce::Image decodePreview(const PresetLibraryRecord& record) {
    if (!record.presentation.preview.has_value()) {
        return {};
    }
    juce::MemoryInputStream input(record.presentation.preview->jpegData, false);
    juce::JPEGImageFormat jpeg;
    return jpeg.decodeImage(input);
}

void drawStars(juce::Graphics& graphics, int rating, juce::Rectangle<float> bounds) {
    for (int star = 0; star < 5; ++star) {
        graphics.setColour(star < rating
                ? CanvasChromePalette::navigationAccent
                : CanvasChromePalette::mutedText.withAlpha(0.34f));
        const auto mark = bounds.removeFromLeft(13.f)
                .withSizeKeepingCentre(6.f, 6.f);
        if (star < rating) {
            graphics.fillEllipse(mark);
        } else {
            graphics.drawEllipse(mark, 1.f);
        }
    }
}

void drawPreviewPlaceholder(juce::Graphics& graphics, juce::Rectangle<float> bounds) {
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRect(bounds);
    graphics.setColour(CanvasChromePalette::gridMinor.withAlpha(0.7f));
    for (int column = 1; column < 8; ++column) {
        const float x = bounds.getX() + bounds.getWidth() * (float) column / 8.f;
        graphics.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());
    }
    for (int row = 1; row < 4; ++row) {
        const float y = bounds.getY() + bounds.getHeight() * (float) row / 4.f;
        graphics.drawHorizontalLine(juce::roundToInt(y), bounds.getX(), bounds.getRight());
    }
    graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.28f));
    const float quarterWidth = bounds.getWidth() * 0.25f;
    const float amplitude = bounds.getHeight() * 0.17f;
    juce::Path wave;
    wave.startNewSubPath(bounds.getX(), bounds.getCentreY());
    wave.cubicTo(
            bounds.getX() + quarterWidth * 0.5f, bounds.getCentreY() - amplitude,
            bounds.getX() + quarterWidth * 0.5f, bounds.getCentreY() - amplitude,
            bounds.getX() + quarterWidth, bounds.getCentreY());
    wave.cubicTo(
            bounds.getX() + quarterWidth * 1.5f, bounds.getCentreY() + amplitude,
            bounds.getX() + quarterWidth * 1.5f, bounds.getCentreY() + amplitude,
            bounds.getX() + quarterWidth * 2.f, bounds.getCentreY());
    wave.cubicTo(
            bounds.getX() + quarterWidth * 2.5f, bounds.getCentreY() - amplitude,
            bounds.getX() + quarterWidth * 2.5f, bounds.getCentreY() - amplitude,
            bounds.getX() + quarterWidth * 3.f, bounds.getCentreY());
    wave.cubicTo(
            bounds.getX() + quarterWidth * 3.5f, bounds.getCentreY() + amplitude,
            bounds.getX() + quarterWidth * 3.5f, bounds.getCentreY() + amplitude,
            bounds.getRight(), bounds.getCentreY());
    graphics.strokePath(wave, juce::PathStrokeType(1.2f));
}

}

void PresetCardGrid::setResults(
        const std::vector<PresetLibraryRecord>& records,
        const std::vector<int>& visibleIndices) {
    juce::File selectedFile;
    if (const auto* current = selectedRecord()) {
        selectedFile = current->file;
    }
    library = records;
    indices = visibleIndices;
    selected = 0;
    if (selectedFile != juce::File()) {
        for (int index = 0; index < (int) indices.size(); ++index) {
            if (library[(size_t) indices[(size_t) index]].file == selectedFile) {
                selected = index;
                break;
            }
        }
    }
    repaint();
    if (onSelection) {
        onSelection();
    }
}

void PresetCardGrid::setCallbacks(
        SelectionCallback selectionCallback,
        OpenCallback openCallback) {
    onSelection = std::move(selectionCallback);
    onOpen = std::move(openCallback);
}

void PresetCardGrid::moveSelection(int columnDelta, int rowDelta) {
    if (indices.empty()) {
        return;
    }
    const int columns = columnCount(getWidth());
    select(juce::jlimit(0, (int) indices.size() - 1,
            selected + columnDelta + rowDelta * columns));
}

const PresetLibraryRecord* PresetCardGrid::selectedRecord() const {
    if (selected < 0 || selected >= (int) indices.size()) {
        return nullptr;
    }
    const int recordIndex = indices[(size_t) selected];
    if (recordIndex < 0 || recordIndex >= (int) library.size()) {
        return nullptr;
    }
    return &library[(size_t) recordIndex];
}

int PresetCardGrid::contentHeightForWidth(int width) const {
    const int columns = columnCount(width);
    const int rows = ((int) indices.size() + columns - 1) / columns;
    return juce::jmax(1, rows) * (cardHeight + cardGap) + cardGap;
}

void PresetCardGrid::paint(juce::Graphics& graphics) {
    for (int visibleIndex = 0; visibleIndex < (int) indices.size(); ++visibleIndex) {
        const int recordIndex = indices[(size_t) visibleIndex];
        const auto& record = library[(size_t) recordIndex];
        const auto card = cardBounds(visibleIndex).toFloat();
        const bool isSelected = visibleIndex == selected;

        graphics.setColour(isSelected
                ? CanvasChromePalette::raisedSurface
                : CanvasChromePalette::surface);
        graphics.fillRoundedRectangle(card, 5.f);
        graphics.setColour(isSelected
                ? CanvasChromePalette::navigationAccent
                : CanvasChromePalette::border.withAlpha(0.76f));
        graphics.drawRoundedRectangle(card.reduced(0.5f), 5.f, isSelected ? 1.5f : 1.f);

        auto inner = card.reduced((float) cardInset);
        auto previewBounds = inner.removeFromTop(104.f);
        const auto& preview = previewFor(recordIndex);
        if (preview.isValid()) {
            graphics.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
            graphics.drawImage(preview, previewBounds);
        } else {
            drawPreviewPlaceholder(graphics, previewBounds);
        }
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.8f));
        graphics.drawRect(previewBounds, 1.f);

        inner.removeFromTop(7.f);
        auto nameRow = inner.removeFromTop(17.f);
        graphics.setColour(CanvasChromePalette::text);
        graphics.setFont(juce::FontOptions(13.f).withStyle("Bold"));
        graphics.drawFittedText(record.name, nameRow.toNearestInt(),
                juce::Justification::centredLeft, 1);

        auto metaRow = inner.removeFromTop(15.f);
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(juce::FontOptions(10.f));
        const juce::String meta = record.presentation.author.isNotEmpty()
                ? record.presentation.author + "  /  " + record.presentation.pack
                : record.presentation.pack;
        graphics.drawFittedText(meta, metaRow.toNearestInt(),
                juce::Justification::centredLeft, 1);
        drawStars(graphics, record.presentation.rating, metaRow.removeFromRight(67.f));
    }
}

void PresetCardGrid::mouseDown(const juce::MouseEvent& event) {
    select(indexAt(event.getPosition()));
}

void PresetCardGrid::mouseDoubleClick(const juce::MouseEvent& event) {
    const int hit = indexAt(event.getPosition());
    if (hit < 0) {
        return;
    }
    select(hit);
    if (onOpen) {
        onOpen();
    }
}

int PresetCardGrid::columnCount(int width) const {
    return juce::jmax(1, (width - cardGap) / (cardWidth + cardGap));
}

int PresetCardGrid::indexAt(juce::Point<int> position) const {
    for (int index = 0; index < (int) indices.size(); ++index) {
        if (cardBounds(index).contains(position)) {
            return index;
        }
    }
    return -1;
}

juce::Rectangle<int> PresetCardGrid::cardBounds(int visibleIndex) const {
    const int columns = columnCount(getWidth());
    const int column = visibleIndex % columns;
    const int row = visibleIndex / columns;
    return {
            cardGap + column * (cardWidth + cardGap),
            cardGap + row * (cardHeight + cardGap),
            cardWidth,
            cardHeight
    };
}

const juce::Image& PresetCardGrid::previewFor(int recordIndex) {
    const auto& record = library[(size_t) recordIndex];
    const juce::String key = record.file.getFullPathName();
    auto found = previewCache.find(key);
    if (found == previewCache.end()
            || found->second.modificationTime != record.modificationTime) {
        CachedPreview decoded { record.modificationTime, decodePreview(record) };
        found = previewCache.insert_or_assign(key, std::move(decoded)).first;
    }
    return found->second.image;
}

void PresetCardGrid::select(int visibleIndex) {
    if (visibleIndex < 0 || visibleIndex >= (int) indices.size() || visibleIndex == selected) {
        return;
    }
    selected = visibleIndex;
    repaint();
    if (onSelection) {
        onSelection();
    }
}

void PresetDetailPanel::setRecord(const PresetLibraryRecord* recordToUse) {
    hasRecord = recordToUse != nullptr;
    record = hasRecord ? *recordToUse : PresetLibraryRecord {};
    preview = hasRecord ? decodePreview(record) : juce::Image {};
    repaint();
}

void PresetDetailPanel::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::surface);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.8f));
    graphics.drawLine(0.f, 0.f, 0.f, (float) getHeight());
    if (!hasRecord) {
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText("No matching presets", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto bounds = getLocalBounds().reduced(20);
    auto previewBounds = bounds.removeFromTop(132).toFloat();
    if (preview.isValid()) {
        graphics.drawImage(preview, previewBounds);
    } else {
        drawPreviewPlaceholder(graphics, previewBounds);
    }
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRect(previewBounds, 1.f);
    bounds.removeFromTop(19);

    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(juce::FontOptions(19.f).withStyle("Bold"));
    graphics.drawFittedText(record.name, bounds.removeFromTop(27),
            juce::Justification::centredLeft, 1);
    drawStars(graphics, record.presentation.rating, bounds.removeFromTop(20).toFloat());
    bounds.removeFromTop(13);

    auto drawField = [&](const juce::String& label, const juce::String& value) {
        graphics.setFont(juce::FontOptions(10.f).withStyle("Bold"));
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText(label, bounds.removeFromTop(17), juce::Justification::centredLeft);
        graphics.setFont(juce::FontOptions(12.f));
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.9f));
        graphics.drawText(value, bounds.removeFromTop(22), juce::Justification::centredLeft);
        bounds.removeFromTop(8);
    };
    drawField("AUTHOR", record.presentation.author.isNotEmpty()
            ? record.presentation.author : "Unknown");
    drawField("PACK", record.presentation.pack);

    if (record.presentation.tags.size() > 0) {
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.15f));
        auto tagBounds = bounds.removeFromTop(25);
        graphics.fillRoundedRectangle(tagBounds.toFloat(), 4.f);
        graphics.setColour(CanvasChromePalette::navigationAccent);
        graphics.setFont(juce::FontOptions(10.f).withStyle("Bold"));
        graphics.drawFittedText(record.presentation.tags.joinIntoString("  ").toUpperCase(),
                tagBounds.reduced(8, 0), juce::Justification::centredLeft, 1);
    }
    bounds.removeFromTop(16);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(juce::FontOptions(11.f));
    graphics.drawFittedText(record.presentation.description, bounds,
            juce::Justification::topLeft, 8);
}

void PresetBrowserSidebar::setRecords(const std::vector<PresetLibraryRecord>& records) {
    factoryCount = 0;
    userCount = 0;
    tags.clear();
    for (const auto& record : records) {
        if (record.presentation.pack.equalsIgnoreCase("Factory")) {
            ++factoryCount;
        } else {
            ++userCount;
        }
        for (const auto& tag : record.presentation.tags) {
            tags.addIfNotAlreadyThere(tag);
        }
    }
    tags.sortNatural();
    repaint();
}

void PresetBrowserSidebar::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::insetBackground);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.7f));
    graphics.drawLine((float) getWidth() - 1.f, 0.f,
            (float) getWidth() - 1.f, (float) getHeight());
    auto bounds = getLocalBounds().reduced(18, 20);
    graphics.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.drawText("LIBRARY", bounds.removeFromTop(20), juce::Justification::centredLeft);

    auto drawItem = [&](const juce::String& name, int count, bool selected) {
        auto row = bounds.removeFromTop(31);
        if (selected) {
            graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.12f));
            graphics.fillRoundedRectangle(row.toFloat(), 4.f);
        }
        graphics.setColour(selected
                ? CanvasChromePalette::navigationAccent
                : CanvasChromePalette::text.withAlpha(0.82f));
        graphics.setFont(juce::FontOptions(12.f));
        graphics.drawText(name, row.reduced(8, 0), juce::Justification::centredLeft);
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText(juce::String(count), row.reduced(8, 0), juce::Justification::centredRight);
    };
    drawItem("All presets", factoryCount + userCount, true);
    drawItem("Factory", factoryCount, false);
    drawItem("User", userCount, false);
    bounds.removeFromTop(22);

    graphics.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.drawText("TAGS", bounds.removeFromTop(22), juce::Justification::centredLeft);
    graphics.setFont(juce::FontOptions(11.f));
    for (const auto& tag : tags) {
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.7f));
        graphics.drawText(tag, bounds.removeFromTop(26).reduced(8, 0),
                juce::Justification::centredLeft);
    }
}

}
