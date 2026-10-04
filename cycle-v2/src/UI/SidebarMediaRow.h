#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CycleV2::SidebarMediaRow {

constexpr int height = 84;

enum class PreviewPosition {
    BelowLabels,
    BehindLabels
};

juce::Rectangle<float> paintFrame(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        bool selected,
        PreviewPosition position);

void paintLabels(
        juce::Graphics& graphics,
        juce::Rectangle<float> slot,
        const juce::String& title,
        const juce::String& tag,
        bool onImage);

}
