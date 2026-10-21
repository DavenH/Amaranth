#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

juce::Rectangle<float> propertySliderTrackBounds(juce::Rectangle<float> bounds);
float propertySliderValuePosition(juce::Slider& slider, double value);
juce::Rectangle<float> propertySliderIndicatorBounds(juce::Rectangle<float> thumbBounds);
juce::Rectangle<float> morphSliderIndicatorBounds(
        juce::Rectangle<float> bounds,
        double normalizedValue);
juce::Rectangle<float> morphKeyboardFocusOutlineBounds(
        juce::Rectangle<float> rowBounds,
        juce::Rectangle<float> trailingControlBounds);
juce::Rectangle<float> keyboardFocusIndicatorBounds(
        juce::Rectangle<float> focusOutlineBounds);
juce::Rectangle<float> keyboardFocusIndicatorHitBounds(
        juce::Rectangle<float> focusOutlineBounds);
juce::Colour propertyControlFocusColour();
void paintKeyboardFocusIndicator(
        juce::Graphics& graphics,
        juce::Rectangle<float> focusOutlineBounds);
void paintMorphKeyboardFocus(
        juce::Graphics& graphics,
        juce::Rectangle<float> focusOutlineBounds);
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
        bool enabled = true);
void paintMorphPreviewVertex(juce::Graphics& graphics, juce::Point<float> point, bool selected);
void paintMorphPreviewCursor(juce::Graphics& graphics, juce::Point<float> point);
juce::LookAndFeel& propertyControlLookAndFeel();

}
