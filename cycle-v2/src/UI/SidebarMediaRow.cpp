#include "UI/SidebarMediaRow.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarMediaRow {

juce::Rectangle<float> paintFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected,
        PreviewPosition position) {
    const auto card = slot.reduced(10.f, 3.f);
    graphics.setColour(selected
            ? CanvasChromePalette::raisedSurface
            : CanvasChromePalette::surface);
    graphics.fillRoundedRectangle(card, 5.f);
    graphics.setColour(selected
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border.withAlpha(0.55f));
    graphics.drawRoundedRectangle(card, 5.f, selected ? 1.2f : 0.8f);

    const auto preview = position == PreviewPosition::BehindLabels
            ? card.reduced(8.f, 5.f)
            : card.withTrimmedTop(26.f).withTrimmedBottom(5.f).reduced(8.f, 0.f);
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(preview, 3.f);
    return preview;
}

void paintLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::String& tag,
        bool onImage) {
    auto header = slot.reduced(10.f, 3.f).withHeight(23.f).reduced(8.f, 0.f);
    const auto titleBounds = header.withTrimmedRight(74.f);
    const auto tagBounds = header.removeFromRight(70.f);
    const juce::Font titleFont(juce::FontOptions(13.f, juce::Font::bold));
    const juce::Font tagFont(juce::FontOptions(9.f));
    auto tagTextBounds = tagBounds;
    auto tagJustification = juce::Justification::centredRight;
    if (onImage) {
        graphics.setColour(juce::Colour(0xff333333));
        const float titleWidth = juce::jmin(
                titleBounds.getWidth() + 8.f,
                titleFont.getStringWidthFloat(title) + 12.f);
        graphics.fillRoundedRectangle(
                titleBounds.getX() - 4.f, header.getY() + 2.f,
                titleWidth, 19.f, 3.f);
        if (tag.isNotEmpty()) {
            const float tagWidth = juce::jmin(
                    tagBounds.getWidth(),
                    tagFont.getStringWidthFloat(tag.toUpperCase()) + 12.f);
            const juce::Rectangle<float> tagChip(
                    tagBounds.getRight() - tagWidth,
                    header.getY() + 2.f, tagWidth, 19.f);
            graphics.fillRoundedRectangle(tagChip, 3.f);
            tagTextBounds = tagChip.reduced(5.f, 0.f);
            tagJustification = juce::Justification::centred;
        }
    }
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(titleFont);
    graphics.drawFittedText(title, titleBounds.toNearestInt(),
            juce::Justification::centredLeft, 1);
    graphics.setColour(onImage
            ? CanvasChromePalette::text.withAlpha(0.9f)
            : CanvasChromePalette::mutedText);
    graphics.setFont(tagFont);
    graphics.drawFittedText(tag.toUpperCase(),
            tagTextBounds.toNearestInt(), tagJustification, 1);
}

}
