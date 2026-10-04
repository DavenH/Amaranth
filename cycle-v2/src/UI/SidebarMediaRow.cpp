#include "UI/SidebarMediaRow.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarMediaRow {

juce::Rectangle<float> paintFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
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

    const auto preview = card.reduced(8.f, 5.f);
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(preview, 3.f);
    return preview;
}

void paintLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::StringArray& tags) {
    auto header = slot.reduced(10.f, 3.f).withHeight(23.f).reduced(8.f, 0.f);
    const auto titleBounds = header.withTrimmedRight(74.f);
    const auto tagBounds = header.removeFromRight(70.f);
    const juce::Font titleFont(juce::FontOptions(13.f, juce::Font::bold));
    const juce::Font tagFont(juce::FontOptions(9.f));
    graphics.setColour(juce::Colour(0xff333333));
    const float titleWidth = juce::jmin(
            titleBounds.getWidth() + 8.f,
            titleFont.getStringWidthFloat(title) + 12.f);
    graphics.fillRoundedRectangle(
            titleBounds.getX() - 4.f, header.getY() + 2.f,
            titleWidth, 19.f, 3.f);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(titleFont);
    graphics.drawFittedText(title, titleBounds.toNearestInt(),
            juce::Justification::centredLeft, 1);

    for (int index = 0; index < juce::jmin(2, tags.size()); ++index) {
        const auto label = tags[index].toUpperCase();
        const float width = juce::jmin(tagBounds.getWidth(),
                tagFont.getStringWidthFloat(label) + 12.f);
        const juce::Rectangle<float> chip(
                tagBounds.getRight() - width,
                index == 0 ? header.getY() + 2.f : slot.getBottom() - 28.f,
                width, 19.f);
        graphics.setColour(juce::Colour(0xff333333));
        graphics.fillRoundedRectangle(chip, 3.f);
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.9f));
        graphics.setFont(tagFont);
        graphics.drawFittedText(label, chip.toNearestInt().reduced(5, 0),
                juce::Justification::centred, 1);
    }
}

}
