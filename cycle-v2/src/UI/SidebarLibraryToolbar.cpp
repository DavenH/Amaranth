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
        targets.push_back({ component->getComponentID(),
                component->getBounds().toFloat() });
    }
    return targets;
}

void SidebarLibraryToolbar::paint(juce::Graphics& graphics) {
    const auto group = create.getBounds().getUnion(remove.getBounds())
            .expanded(2, 2).toFloat();
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
    const int actionWidth = (actions.getWidth() - 12) / 4;
    create.setBounds(actions.removeFromLeft(actionWidth));
    actions.removeFromLeft(4);
    edit.setBounds(actions.removeFromLeft(actionWidth));
    actions.removeFromLeft(4);
    rename.setBounds(actions.removeFromLeft(actionWidth));
    actions.removeFromLeft(4);
    remove.setBounds(actions);
}

}
