#pragma once

#include <functional>

#include "JuceHeader.h"

namespace CycleV2 {

// Reconcile property hover when an OpenGL peer misses a native mouse-enter event.
class PropertyHoverTracker final : private juce::Timer {
public:
    PropertyHoverTracker(juce::Component& owner, std::function<void(juce::Point<float>)> update);

private:
    void timerCallback() override;

    juce::Component& owner;
    std::function<void(juce::Point<float>)> update;
};

}
