#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace CycleV2 { struct Node; }

namespace CycleV2 {

enum class GuideCurveField;

struct TrimeshGuideAttachmentTarget {
    int cubeIndex { -1 };
    juce::String field;

    bool isValid() const;
    bool isCubeTarget() const { return isValid(); }
    int fieldIndex() const;

    static constexpr int fieldCount = 6;
    static const std::array<juce::String, fieldCount>& fields();
    static GuideCurveField guideField(const juce::String& field);
    static TrimeshGuideAttachmentTarget parse(const juce::String& portId);
    static juce::String portIdForCube(int cubeIndex, const juce::String& field);
    static std::vector<juce::String> cubePortIdsForVertex(
            const Node& trimeshNode,
            int vertexIndex,
            const juce::String& field);
};

}
