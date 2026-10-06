#include "UI/SidebarMediaRow.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarMediaRow {

juce::Rectangle<float> favoriteBounds(juce::Rectangle<float> slot) {
    return { slot.getX() + 17.f, slot.getY() + 3.f, 23.f, 23.f };
}

juce::Rectangle<float> patternCardBounds(juce::Rectangle<float> slot) {
    return slot.reduced(2.f, 3.f);
}

juce::Rectangle<float> patternFavoriteBounds(juce::Rectangle<float> slot) {
    const auto card = patternCardBounds(slot);
    return { card.getX() + 4.f, card.getY() + 3.f, 22.f, 22.f };
}

juce::Rectangle<float> patternPreviewBounds(juce::Rectangle<float> slot) {
    const auto card = patternCardBounds(slot);
    return { card.getX() + 5.f, card.getY() + 34.f,
            card.getWidth() - 10.f, card.getHeight() - 39.f };
}

void paintFavorite(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool favorite) {
    paintStar(graphics, favoriteBounds(slot), favorite);
}

void paintStar(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        bool favorite) {
    graphics.setColour(juce::Colour(0xff333333));
    graphics.fillRoundedRectangle(bounds, 3.f);
    juce::Path star;
    star.addStar(bounds.getCentre(), 5, 3.5f, 7.f,
            -juce::MathConstants<float>::halfPi);
    graphics.setColour(favorite
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::mutedText.withAlpha(0.7f));
    if (favorite) {
        graphics.fillPath(star);
    } else {
        graphics.strokePath(star, juce::PathStrokeType(1.2f));
    }
}

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
        const juce::StringArray& tags,
        bool favorite) {
    auto header = slot.reduced(10.f, 3.f).withHeight(23.f).reduced(8.f, 0.f);
    const auto titleBounds = header.withTrimmedLeft(23.f).withTrimmedRight(74.f);
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
    paintFavorite(graphics, slot, favorite);

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

juce::Rectangle<float> paintPatternFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected) {
    const auto card = patternCardBounds(slot);
    graphics.setColour(selected
            ? CanvasChromePalette::raisedSurface
            : CanvasChromePalette::surface);
    graphics.fillRoundedRectangle(card, 5.f);
    graphics.setColour(selected
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border.withAlpha(0.55f));
    graphics.drawRoundedRectangle(card, 5.f, selected ? 1.2f : 0.8f);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.5f));
    graphics.drawHorizontalLine(juce::roundToInt(card.getY() + 31.f),
            card.getX() + 5.f, card.getRight() - 5.f);
    const auto preview = patternPreviewBounds(slot);
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(preview, 3.f);
    return preview;
}

void paintPatternLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::StringArray& tags,
        bool favorite) {
    const auto card = patternCardBounds(slot);
    const auto star = patternFavoriteBounds(slot);
    paintStar(graphics, star, favorite);
    const auto labelLeft = star.getRight() + 5.f;
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(juce::FontOptions(13.f).withStyle("Bold"));
    graphics.drawFittedText(title,
            juce::Rectangle<float>(labelLeft, card.getY() + 3.f,
                    card.getRight() - labelLeft - 5.f, 17.f).toNearestInt(),
            juce::Justification::centredLeft, 1);

    float tagX = labelLeft;
    const juce::Font tagFont(juce::FontOptions(8.5f));
    for (int index = 0; index < juce::jmin(2, tags.size()); ++index) {
        const auto label = tags[index].toUpperCase();
        const float width = tagFont.getStringWidthFloat(label) + 10.f;
        if (tagX + width > card.getRight() - 5.f) {
            break;
        }
        const juce::Rectangle<float> chip(
                tagX, card.getY() + 21.f, width, 10.f);
        graphics.setColour(juce::Colour(0xff333333));
        graphics.fillRoundedRectangle(chip, 2.f);
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.9f));
        graphics.setFont(tagFont);
        graphics.drawFittedText(label, chip.toNearestInt(),
                juce::Justification::centred, 1);
        tagX += width + 3.f;
    }
}

}
