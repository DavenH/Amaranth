#include "UI/SidebarTagCloud.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

constexpr int chipHeight = 21;
constexpr int chipGap = 4;
constexpr int horizontalInset = 1;

}

void SidebarTagCloud::styleHeading(juce::Label& heading) {
    heading.setText("FILTER TAGS", juce::dontSendNotification);
    heading.setFont(juce::FontOptions(10.f, juce::Font::bold));
    heading.setColour(juce::Label::textColourId,
            CanvasChromePalette::mutedText);
}

void SidebarTagCloud::setTags(juce::StringArray tags) {
    tags.removeEmptyStrings();
    tags.removeDuplicates(true);
    tags.sortNatural();
    available = std::move(tags);
    for (int index = selected.size(); --index >= 0;) {
        if (!available.contains(selected[index], true)) {
            selected.remove(index);
        }
    }
    resized();
    repaint();
}

bool SidebarTagCloud::matches(const juce::StringArray& recordTags) const {
    if (selected.isEmpty()) {
        return true;
    }
    for (const auto& tag : recordTags) {
        if (selected.contains(tag, true)) {
            return true;
        }
    }
    return false;
}

int SidebarTagCloud::preferredHeightForWidth(int width) const {
    const auto layout = layoutForWidth(width);
    return layout.empty() ? chipHeight
            : layout.back().bounds.getBottom();
}

void SidebarTagCloud::setChangeCallback(ChangeCallback callback) {
    onChange = std::move(callback);
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
SidebarTagCloud::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
    for (const auto& chip : chips) {
        targets.push_back({ "workspace.sidebar.tag." + chip.tag.toLowerCase(),
                chip.bounds.toFloat() });
    }
    return targets;
}

void SidebarTagCloud::paint(juce::Graphics& graphics) {
    const juce::Font chipFont(juce::FontOptions(10.f));
    graphics.setFont(chipFont);
    for (int index = 0; index < (int) chips.size(); ++index) {
        const auto& chip = chips[(size_t) index];
        const bool active = selected.contains(chip.tag, true);
        const auto bounds = chip.bounds.toFloat();
        graphics.setColour(active
                ? CanvasChromePalette::navigationAccent.withAlpha(0.25f)
                : index == hovered
                        ? CanvasChromePalette::raisedSurface
                        : CanvasChromePalette::restingControlSurface);
        graphics.fillRoundedRectangle(bounds, 4.f);
        graphics.setColour(active
                ? CanvasChromePalette::navigationAccent
                : CanvasChromePalette::border);
        graphics.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);
        graphics.setColour(active
                ? CanvasChromePalette::text
                : CanvasChromePalette::mutedText);
        graphics.drawFittedText(chip.tag, chip.bounds.reduced(7, 0),
                juce::Justification::centred, 1);
    }
}

void SidebarTagCloud::resized() {
    chips = layoutForWidth(getWidth());
}

void SidebarTagCloud::mouseMove(const juce::MouseEvent& event) {
    int nextHover = -1;
    for (int index = 0; index < (int) chips.size(); ++index) {
        if (chips[(size_t) index].bounds.contains(event.getPosition())) {
            nextHover = index;
            break;
        }
    }
    if (nextHover != hovered) {
        hovered = nextHover;
        setMouseCursor(hovered >= 0
                ? juce::MouseCursor::PointingHandCursor
                : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SidebarTagCloud::mouseExit(const juce::MouseEvent&) {
    hovered = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void SidebarTagCloud::mouseUp(const juce::MouseEvent& event) {
    for (const auto& chip : chips) {
        if (!chip.bounds.contains(event.getPosition())) {
            continue;
        }
        const int selectedIndex = selected.indexOf(chip.tag, true);
        if (selectedIndex >= 0) {
            selected.remove(selectedIndex);
        } else {
            selected.add(chip.tag);
        }
        repaint();
        if (onChange) {
            onChange();
        }
        return;
    }
}

std::vector<SidebarTagCloud::Chip> SidebarTagCloud::layoutForWidth(int width) const {
    const juce::Font chipFont(juce::FontOptions(10.f));
    std::vector<Chip> result;
    int x = horizontalInset;
    int y = 0;
    for (const auto& tag : available) {
        const int chipWidth = juce::jmin(juce::jmax(36,
                (int) chipFont.getStringWidthFloat(tag) + 16),
                juce::jmax(36, width - 2 * horizontalInset));
        if (x > horizontalInset && x + chipWidth > width - horizontalInset) {
            x = horizontalInset;
            y += chipHeight + chipGap;
        }
        result.push_back({ tag, { x, y, chipWidth, chipHeight } });
        x += chipWidth + chipGap;
    }
    return result;
}

}
