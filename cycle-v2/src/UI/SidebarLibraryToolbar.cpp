#include "UI/SidebarLibraryToolbar.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

SidebarLibraryToolbar::SidebarLibraryToolbar(
        const juce::String& searchPlaceholder) :
        search(searchPlaceholder) {
    addAndMakeVisible(search);
    for (auto* button : { &create, &edit, &rename, &remove }) {
        button->setColour(juce::TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button->setColour(juce::TextButton::buttonOnColourId, CanvasChromePalette::border);
        button->setColour(juce::TextButton::textColourOffId,
                CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }
    create.setColour(juce::TextButton::buttonColourId,
            CanvasChromePalette::navigationAccent.withAlpha(0.18f));
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
SidebarLibraryToolbar::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets;
    for (const auto* component : { static_cast<const juce::Component*>(&search),
            static_cast<const juce::Component*>(&create),
            static_cast<const juce::Component*>(&edit),
            static_cast<const juce::Component*>(&rename),
            static_cast<const juce::Component*>(&remove) }) {
        if (component->isVisible()) {
            targets.push_back({ component->getComponentID(),
                    component->getBounds().toFloat() });
        }
    }
    return targets;
}

void SidebarLibraryToolbar::paint(juce::Graphics& graphics) {
    auto groupBounds = create.getBounds();
    for (const auto* button : { &edit, &rename, &remove }) {
        if (button->isVisible()) {
            groupBounds = groupBounds.getUnion(button->getBounds());
        }
    }
    const auto group = groupBounds.expanded(2, 2).toFloat();
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRoundedRectangle(group, 5.f);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.5f));
    graphics.drawRoundedRectangle(group.reduced(0.5f), 5.f, 1.f);
}

void SidebarLibraryToolbar::resized() {
    auto bounds = getLocalBounds().reduced(6, 0);
    bounds.removeFromTop(7);
    search.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(6);
    auto actions = bounds.removeFromTop(28);
    std::vector<juce::TextButton*> buttons;
    for (auto* button : { &create, &edit, &rename, &remove }) {
        if (button->isVisible()) {
            buttons.push_back(button);
        }
    }
    const bool iconActions = create.getProperties().contains("libraryIcon");
    if (!iconActions) {
        const int width = (actions.getWidth() - 4 * ((int) buttons.size() - 1)) / (int) buttons.size();
        for (auto* button : buttons) {
            button->setBounds(button == buttons.back() ? actions : actions.removeFromLeft(width));
            actions.removeFromLeft(4);
        }
        return;
    }
    const int groupWidth = (int) buttons.size() * 32 + ((int) buttons.size() - 1) * 8;
    actions = actions.withSizeKeepingCentre(groupWidth, 28);
    for (auto* button : buttons) {
        button->setBounds(actions.removeFromLeft(32));
        actions.removeFromLeft(8);
    }
}

}
