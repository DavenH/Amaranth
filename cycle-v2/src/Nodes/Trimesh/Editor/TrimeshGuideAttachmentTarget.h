#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "Nodes/Guide/GuideAttachmentTarget.h"

namespace CycleV2 {
struct Node;
struct TrimeshCubeComponentGuideTarget;
}

namespace CycleV2 {

enum class GuideCurveField;

class TrimeshGuideAttachmentTarget {
public:
    static constexpr int fieldCount = GuideAttachmentTarget::fieldCount;
    static const std::array<juce::String, fieldCount>& fields();
    static GuideCurveField guideField(const juce::String& field);
    static std::vector<TrimeshCubeComponentGuideTarget> cubeTargetsForVertex(
            const Node& trimeshNode,
            int vertexIndex,
            const juce::String& field);
};

class MeshGuideAttachmentTarget {
public:
    static std::vector<TrimeshCubeComponentGuideTarget> cubeTargetsForSelection(
            const Node& node,
            int selectionIndex,
            const juce::String& field);
};

}
