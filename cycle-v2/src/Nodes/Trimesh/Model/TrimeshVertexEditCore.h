#pragma once

#include <array>
#include <optional>
#include <vector>

#include <JuceHeader.h>

class Mesh;
class VertCube;

namespace CycleV2 {

enum class TrimeshVertexEditTarget {
    VertexValue,
    GuideGain
};

struct TrimeshVertexValueChange {
    int ownerOrdinal { -1 };
    float before {};
    float after {};
};

struct TrimeshVertexEditDelta {
    TrimeshVertexEditTarget target { TrimeshVertexEditTarget::VertexValue };
    int vertexIndex { -1 };
    int valueIndex { -1 };
    std::vector<TrimeshVertexValueChange> changes;

    bool changed() const { return !changes.empty(); }
    TrimeshVertexEditDelta inverse() const;
};

struct TrimeshCubeCurveEdit {
    std::array<float, 8> before {};
    std::array<float, 8> after {};

    bool changed() const { return before != after; }
    TrimeshCubeCurveEdit inverse() const;
};

class TrimeshVertexEditCore final {
public:
    static std::optional<TrimeshVertexEditDelta> prepareVertexValue(
            const Mesh& mesh,
            int vertexIndex,
            const juce::String& parameterId,
            float value);
    static std::optional<TrimeshVertexEditDelta> prepareGuideGain(
            const Mesh& mesh,
            int vertexIndex,
            const juce::String& parameterId,
            float value);
    static std::optional<TrimeshCubeCurveEdit> prepareCubeCurve(
            const VertCube& cube,
            float averageValue);
    static bool apply(Mesh& mesh, const TrimeshVertexEditDelta& delta);
    static bool apply(VertCube& cube, const TrimeshCubeCurveEdit& edit);
    static bool canApply(const Mesh& mesh, const TrimeshVertexEditDelta& delta);
    static std::optional<TrimeshVertexEditDelta> compose(
            const TrimeshVertexEditDelta& accumulated,
            const TrimeshVertexEditDelta& movement);
    static float guideGain(
            const Mesh& mesh,
            int vertexIndex,
            const juce::String& parameterId);

private:
    static int valueIndex(const juce::String& parameterId);
};

}
