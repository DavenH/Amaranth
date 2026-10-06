#include "UI/SidebarMediaRow.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarMediaRow {

HeaderLabelsLayout headerLabelsLayout(
        juce::Rectangle<float> card,
        juce::Rectangle<float> favorite,
        const juce::StringArray& tags) {
    HeaderLabelsLayout layout;
    const float labelLeft = favorite.getRight() + 5.f;
    const juce::Font tagFont(juce::FontOptions(8.5f));
    float totalTagWidth = 0.f;
    const float tagBudget = card.getRight() - labelLeft - 77.f;
    for (int index = 0; index < juce::jmin(2, tags.size()); ++index) {
        const float width = juce::jmin(70.f,
                tagFont.getStringWidthFloat(tags[index].toUpperCase()) + 11.f);
        const float nextWidth = totalTagWidth + width
                + (layout.tagCount == 0 ? 0.f : 3.f);
        if (nextWidth > tagBudget) {
            break;
        }
        layout.tags[(size_t) layout.tagCount].setWidth(width);
        ++layout.tagCount;
        totalTagWidth = nextWidth;
    }
    const float tagStart = card.getRight() - totalTagWidth - 5.f;
    layout.title = { labelLeft, card.getY() + 3.f,
            juce::jmax(1.f, tagStart - labelLeft - 5.f), 20.f };
    float tagX = tagStart;
    for (int index = 0; index < layout.tagCount; ++index) {
        auto& chip = layout.tags[(size_t) index];
        chip.setPosition(tagX, card.getY() + 6.f);
        chip.setHeight(15.f);
        tagX += chip.getWidth() + 3.f;
    }
    return layout;
}

namespace {

void paintHeaderTags(
        juce::Graphics& graphics,
        const juce::StringArray& tags,
        const HeaderLabelsLayout& layout) {
    const juce::Font tagFont(juce::FontOptions(8.5f));
    graphics.setFont(tagFont);
    for (int index = 0; index < layout.tagCount; ++index) {
        const auto chip = layout.tags[(size_t) index];
        graphics.setColour(juce::Colour(0xff333333));
        graphics.fillRoundedRectangle(chip, 3.f);
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.9f));
        graphics.drawFittedText(tags[index].toUpperCase(), chip.toNearestInt(),
                juce::Justification::centred, 1);
    }
}

}

juce::Rectangle<float> favoriteBounds(juce::Rectangle<float> slot) {
    return patternFavoriteBounds(slot);
}

juce::Rectangle<float> cardBounds(juce::Rectangle<float> slot) {
    return slot.reduced(2.f, 3.f);
}

juce::Rectangle<float> patternFavoriteBounds(juce::Rectangle<float> slot) {
    const auto card = cardBounds(slot);
    return { card.getX() + 4.f, card.getY() + 3.f, 22.f, 22.f };
}

juce::Rectangle<float> patternPreviewBounds(juce::Rectangle<float> slot) {
    const auto card = cardBounds(slot);
    return { card.getX() + 5.f, card.getY() + 28.f,
            card.getWidth() - 10.f, card.getHeight() - 33.f };
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
    const auto card = cardBounds(slot);
    graphics.setColour(selected
            ? CanvasChromePalette::raisedSurface
            : CanvasChromePalette::surface);
    graphics.fillRoundedRectangle(card, 5.f);
    graphics.setColour(selected
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border.withAlpha(0.55f));
    graphics.drawRoundedRectangle(card, 5.f, selected ? 1.2f : 0.8f);

    const auto preview = card.reduced(5.f, 5.f);
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
    const auto card = cardBounds(slot);
    const auto layout = headerLabelsLayout(card, favoriteBounds(slot), tags);
    const juce::Font titleFont(juce::FontOptions(13.f, juce::Font::bold));
    graphics.setColour(juce::Colour(0xff333333));
    const float titleWidth = juce::jmin(
            layout.title.getWidth() + 8.f,
            titleFont.getStringWidthFloat(title) + 12.f);
    graphics.fillRoundedRectangle(
            layout.title.getX() - 4.f, layout.title.getY(),
            titleWidth, layout.title.getHeight(), 3.f);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(titleFont);
    graphics.drawFittedText(title, layout.title.toNearestInt(),
            juce::Justification::centredLeft, 1);
    paintFavorite(graphics, slot, favorite);
    paintHeaderTags(graphics, tags, layout);
}

juce::Rectangle<float> paintPatternFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected) {
    const auto card = cardBounds(slot);
    graphics.setColour(selected
            ? CanvasChromePalette::raisedSurface
            : CanvasChromePalette::surface);
    graphics.fillRoundedRectangle(card, 5.f);
    graphics.setColour(selected
            ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border.withAlpha(0.55f));
    graphics.drawRoundedRectangle(card, 5.f, selected ? 1.2f : 0.8f);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.5f));
    graphics.drawHorizontalLine(juce::roundToInt(card.getY() + 26.f),
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
    const auto card = cardBounds(slot);
    const auto star = patternFavoriteBounds(slot);
    const auto layout = headerLabelsLayout(card, star, tags);
    paintStar(graphics, star, favorite);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(juce::FontOptions(13.f).withStyle("Bold"));
    graphics.drawFittedText(title, layout.title.toNearestInt(),
            juce::Justification::centredLeft, 1);
    paintHeaderTags(graphics, tags, layout);
}

}
