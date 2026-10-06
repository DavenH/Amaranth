#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CycleV2::SidebarMediaRow {

constexpr int height = 84;

juce::Rectangle<float> favoriteBounds(juce::Rectangle<float> slot);
juce::Rectangle<float> patternFavoriteBounds(juce::Rectangle<float> slot);
juce::Rectangle<float> patternCardBounds(juce::Rectangle<float> slot);
juce::Rectangle<float> patternPreviewBounds(juce::Rectangle<float> slot);
void paintStar(juce::Graphics& graphics, juce::Rectangle<float> bounds, bool favorite);
void paintFavorite(juce::Graphics& graphics, juce::Rectangle<float> slot, bool favorite);

juce::Rectangle<float> paintFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected);

void paintLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::StringArray& tags,
        bool favorite = false);

juce::Rectangle<float> paintPatternFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected);

void paintPatternLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::StringArray& tags,
        bool favorite);

}
