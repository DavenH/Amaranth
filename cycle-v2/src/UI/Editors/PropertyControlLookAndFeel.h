#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

juce::Rectangle<float> propertySliderTrackBounds(juce::Rectangle<float> bounds);
void paintPropertySlider(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        double normalizedValue,
        juce::Colour fill,
        bool hovered = false,
        bool focused = false,
        bool enabled = true);
juce::LookAndFeel& propertyControlLookAndFeel();

}
