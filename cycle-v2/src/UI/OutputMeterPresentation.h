#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct OutputMeterLayout {
    juce::Rectangle<float> left;
    juce::Rectangle<float> right;
    juce::Rectangle<float> faderHitTarget;
    juce::Rectangle<float> faderTrack;
    juce::Rectangle<float> gainLabel;
};

struct OutputMeterLevels {
    float left {};
    float right {};

    bool operator==(const OutputMeterLevels& other) const {
        return left == other.left && right == other.right;
    }

    bool operator!=(const OutputMeterLevels& other) const {
        return !(*this == other);
    }
};

class OutputMeterBallistics {
public:
    bool update(OutputMeterLevels measured);
    void reset();
    OutputMeterLevels levels() const { return currentLevels; }

private:
    static float nextLevel(float current, float measured);

    OutputMeterLevels currentLevels;
};

class OutputMeterPresentation {
public:
    static OutputMeterLayout layout(juce::Rectangle<float> area);
    static float displayLevelForAmplitude(float amplitude);
    static juce::Rectangle<float> fillBounds(
            juce::Rectangle<float> channelBounds,
            float level);
    static float gainUnitValueAt(
            juce::Rectangle<float> area,
            float y);
    static juce::Rectangle<float> gainThumbBounds(
            juce::Rectangle<float> area,
            float gainUnitValue);
    static juce::String gainLabel(float gainUnitValue);
    static void paint(
            juce::Graphics& graphics,
            juce::Rectangle<float> area,
            float leftLevel,
            float rightLevel,
            juce::Colour colour,
            float gainUnitValue = 0.5f);
};

}
