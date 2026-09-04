#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

juce::Rectangle<float> propertySliderTrackBounds(juce::Rectangle<float> bounds);
juce::Rectangle<float> propertySliderIndicatorBounds(juce::Rectangle<float> thumbBounds);
juce::Rectangle<float> morphSliderIndicatorBounds(
        juce::Rectangle<float> bounds,
        double normalizedValue);
juce::Colour propertyControlFocusColour();
void paintPropertySlider(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        double normalizedValue,
        juce::Colour fill,
        bool hovered = false,
        bool focused = false,
        bool enabled = true);
void paintMorphSlider(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        double normalizedValue,
        juce::Colour accent,
        bool hovered = false,
        bool focused = false,
        bool enabled = true);
juce::LookAndFeel& propertyControlLookAndFeel();

}
