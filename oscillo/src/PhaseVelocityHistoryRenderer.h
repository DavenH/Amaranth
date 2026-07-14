#pragma once

#include <JuceHeader.h>
#include <array>

class GradientColourMap;

struct PhaseVelocityHistory {
    static constexpr int kNumPartials = 8;
    static constexpr int kNumTracks = 13;
    static constexpr int kNumFrames = 512;

    int midiNote = 60;
    int writeIndex = 0;
    int length = 0;
    int sequence = 0;
    bool active = false;
    std::array<std::array<float, kNumFrames>, kNumPartials> values{};
    std::array<std::array<float, kNumFrames>, kNumPartials> magnitudes{};
    std::array<float, kNumFrames> weightedValues{};
    std::array<float, kNumFrames> weightedMagnitudes{};
};

class PhaseVelocityHistoryRenderer {
public:
    static void draw(
        juce::Graphics& g,
        const juce::Rectangle<int>& area,
        int harmonicIndex,
        const juce::String& labelText,
        const std::array<PhaseVelocityHistory, PhaseVelocityHistory::kNumTracks>& histories,
        int latestSequence,
        const GradientColourMap& colourMap,
        juce::Colour background,
        juce::Colour grid,
        juce::Colour label,
        float maxVelocity);
};
