#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class NodeCanvasViewport {
public:
    void setBounds(juce::Rectangle<float> boundsToUse);
    void setTransform(juce::Point<float> panToUse, float zoomToUse);
    void panBy(juce::Point<float> delta);
    void zoomAround(juce::Point<float> screenAnchor, float scaleFactor);

    juce::Point<float> toScreen(juce::Point<float> world) const;
    juce::Point<float> toWorld(juce::Point<float> screen) const;
    juce::Rectangle<float> toScreen(juce::Rectangle<float> world) const;
    juce::Rectangle<float> toWorld(juce::Rectangle<float> screen) const;
    juce::Point<float> centreWorld() const;
    juce::Point<float> snap(juce::Point<float> world, float interval) const;

    juce::Point<float> getPan() const { return pan; }
    float getZoom() const { return zoom; }
    uint64_t getRevision() const { return revision; }

private:
    static constexpr float minimumZoom = 0.18f;
    static constexpr float maximumZoom = 2.4f;

    juce::Rectangle<float> bounds;
    juce::Point<float> pan { 34.f, 38.f };
    float zoom { 0.58f };
    uint64_t revision { 1 };
};

}
