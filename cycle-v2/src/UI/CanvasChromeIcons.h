#pragma once

#include <JuceHeader.h>

namespace CycleV2::CanvasChromeIcons {

inline void paintTrash(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        juce::Colour colour) {
    graphics.setColour(colour);
    const juce::Rectangle<float> bin = bounds.reduced(5.f, 4.f);
    juce::Path body;
    body.startNewSubPath(bin.getX() + 2.f, bin.getY() + 5.f);
    body.lineTo(bin.getX() + 3.f, bin.getBottom());
    body.lineTo(bin.getRight() - 3.f, bin.getBottom());
    body.lineTo(bin.getRight() - 2.f, bin.getY() + 5.f);
    graphics.strokePath(body, juce::PathStrokeType(
            1.5f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    graphics.drawLine(
            bin.getX(),
            bin.getY() + 3.f,
            bin.getRight(),
            bin.getY() + 3.f,
            1.5f);
    graphics.drawLine(
            bin.getCentreX() - 2.f,
            bin.getY(),
            bin.getCentreX() + 2.f,
            bin.getY(),
            1.5f);
}

}
