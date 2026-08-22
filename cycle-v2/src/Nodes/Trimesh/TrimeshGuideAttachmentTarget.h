#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace CycleV2 {
struct Node;
struct TrimeshCubeComponentGuideTarget;
}

namespace CycleV2 {

enum class GuideCurveField;

class TrimeshGuideAttachmentTarget {
public:
    static constexpr int fieldCount = 6;
    static const std::array<juce::String, fieldCount>& fields();
    static GuideCurveField guideField(const juce::String& field);
    static bool isValid(
            const Node& trimeshNode,
            const TrimeshCubeComponentGuideTarget& target);
    static std::vector<TrimeshCubeComponentGuideTarget> cubeTargetsForVertex(
            const Node& trimeshNode,
            int vertexIndex,
            const juce::String& field);
};

}
