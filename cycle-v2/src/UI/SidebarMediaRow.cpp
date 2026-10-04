#include "UI/SidebarMediaRow.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarMediaRow {

juce::Rectangle<float> paint(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::String& tag,
        bool selected) {
    const auto card = slot.reduced(10.f, 3.f);
    graphics.setColour(selected
            ? CanvasChromePalette::raisedSurface
            : CanvasChromePalette::surface);
    graphics.fillRoundedRectangle(card, 5.f);
    graphics.setColour(selected
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border.withAlpha(0.55f));
    graphics.drawRoundedRectangle(card, 5.f, selected ? 1.2f : 0.8f);

    auto header = card.withHeight(23.f).reduced(8.f, 0.f);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(juce::FontOptions(13.f, juce::Font::bold));
    graphics.drawFittedText(title, header.withTrimmedRight(74.f).toNearestInt(),
            juce::Justification::centredLeft, 1);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(juce::FontOptions(9.f));
    graphics.drawFittedText(tag.toUpperCase(),
            header.removeFromRight(70.f).toNearestInt(),
            juce::Justification::centredRight, 1);

    const auto preview = card.withTrimmedTop(26.f)
            .withTrimmedBottom(5.f).reduced(8.f, 0.f);
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(preview, 3.f);
    return preview;
}

}
