#pragma once

#include <JuceHeader.h>

#include <array>

namespace CycleV2 {

struct TrimeshGuideAttachmentTarget {
    int vertexIndex { -1 };
    juce::String field;
    int cubeIndex { -1 };

    bool isValid() const;
    bool isCubeTarget() const { return cubeIndex >= 0; }
    int fieldIndex() const;

    static constexpr int fieldCount = 6;
    static const std::array<juce::String, fieldCount>& fields();
    static TrimeshGuideAttachmentTarget parse(const juce::String& portId);
    static juce::String portIdFor(int vertexIndex, const juce::String& field);
    static juce::String portIdForCube(int cubeIndex, const juce::String& field);
};

}
