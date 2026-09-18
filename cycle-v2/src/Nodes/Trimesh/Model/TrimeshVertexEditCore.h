#pragma once

#include <optional>
#include <vector>

#include <JuceHeader.h>

class Mesh;

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
    static bool apply(Mesh& mesh, const TrimeshVertexEditDelta& delta);
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
