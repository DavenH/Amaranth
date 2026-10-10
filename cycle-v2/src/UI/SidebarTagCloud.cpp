#include "UI/SidebarTagCloud.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

constexpr int chipHeight = 18;
constexpr int chipGap = 4;
constexpr int horizontalInset = 1;
const juce::Colour filterAccent { 0xffd16fab };
const juce::Colour recordAccent { 0xff6d9ed8 };
const juce::Colour combinedAccent { 0xffad83da };

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

void SidebarTagCloud::setRecordTags(juce::StringArray tags) {
    recordTags = std::move(tags);
    repaint();
}

void SidebarTagCloud::setEditCallback(std::function<void(const juce::String&)> callback) {
    onEdit = std::move(callback);
}

juce::Colour SidebarTagCloud::tagAccent(const juce::String& tag) const {
    const bool filtering = selected.contains(tag, true);
    const bool assigned = recordTags.contains(tag, true);
    if (filtering && assigned) {
        return combinedAccent;
    }
    if (filtering) {
        return filterAccent;
    }
    return assigned ? recordAccent : juce::Colours::transparentBlack;
}

void SidebarTagCloud::setFavoritesAvailable(bool available) {
    showFavorites = available;
    if (!available) {
        favoriteSelected = false;
    }
    resized();
    repaint();
}

bool SidebarTagCloud::matches(const juce::StringArray& recordTags) const {
    if (selected.isEmpty()) {
        return true;
    }
    for (const auto& tag : selected) {
        if (!recordTags.contains(tag, true)) {
            return false;
        }
    }
    return true;
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
        targets.push_back({ chip.favorite ? "workspace.sidebar.favoriteFilter"
                : "workspace.sidebar.tag." + chip.tag.toLowerCase(),
                chip.bounds.toFloat() });
    }
    return targets;
}

void SidebarTagCloud::paint(juce::Graphics& graphics) {
    const juce::Font chipFont(juce::FontOptions(10.f));
    graphics.setFont(chipFont);
    for (int index = 0; index < (int) chips.size(); ++index) {
        const auto& chip = chips[(size_t) index];
        const auto accent = chip.favorite
                ? (favoriteSelected ? filterAccent : juce::Colours::transparentBlack)
                : tagAccent(chip.tag);
        const bool active = !accent.isTransparent();
        const auto bounds = chip.bounds.toFloat();
        graphics.setColour(active
                ? accent.withAlpha(0.25f)
                : index == hovered
                        ? CanvasChromePalette::raisedSurface
                        : CanvasChromePalette::restingControlSurface);
        graphics.fillRoundedRectangle(bounds, 4.f);
        graphics.setColour(active
                ? accent
                : CanvasChromePalette::border);
        graphics.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);
        graphics.setColour(active
                ? CanvasChromePalette::text
                : CanvasChromePalette::mutedText);
        if (chip.favorite) {
            juce::Path star;
            star.addStar({ bounds.getX() + 12.f, bounds.getCentreY() },
                    5, 3.f, 6.f, -juce::MathConstants<float>::halfPi);
            if (active) {
                graphics.fillPath(star);
            } else {
                graphics.strokePath(star, juce::PathStrokeType(1.f));
            }
        }
        graphics.drawFittedText(chip.tag.toLowerCase(),
                chip.favorite ? chip.bounds.withTrimmedLeft(22).withTrimmedRight(6)
                        : chip.bounds.reduced(7, 0),
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
        if (!event.mods.isPopupMenu() && !chip.favorite && onEdit) {
            const auto tag = chip.tag;
            onEdit(tag);
            return;
        }
        if (event.mods.isPopupMenu() && chip.favorite) {
            return;
        }
        if (chip.favorite) {
            favoriteSelected = !favoriteSelected;
        } else {
            const int selectedIndex = selected.indexOf(chip.tag, true);
            if (selectedIndex >= 0) {
                selected.remove(selectedIndex);
            } else {
                selected.add(chip.tag);
            }
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
    juce::StringArray labels = available;
    if (showFavorites) {
        labels.insert(0, "Favorites");
    }
    for (int index = 0; index < labels.size(); ++index) {
        const auto& tag = labels[index];
        const bool favorite = showFavorites && index == 0;
        const int chipWidth = juce::jmin(juce::jmax(36,
                (int) chipFont.getStringWidthFloat(tag.toLowerCase()) + (favorite ? 31 : 16)),
                juce::jmax(36, width - 2 * horizontalInset));
        if (x > horizontalInset && x + chipWidth > width - horizontalInset) {
            x = horizontalInset;
            y += chipHeight + chipGap;
        }
        result.push_back({ tag, { x, y, chipWidth, chipHeight }, favorite });
        x += chipWidth + chipGap;
    }
    return result;
}

}
