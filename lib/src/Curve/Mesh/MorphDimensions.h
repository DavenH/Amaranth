#pragma once

#include <array>
#include <vector>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

class Mesh;
class GuideCurveProvider;
class VertCube;

// Morph coordinates are independent of the sampled curve's phase/playback axis.
class MorphDimensions {
public:
    explicit MorphDimensions(int countToUse = 3) : count(juce::jlimit(0, 3, countToUse)) {}

    bool active(int dimension) const;
    int axisCount() const { return count; }
    int primaryAxis() const { return count == 2 ? Vertex::Red : Vertex::Time; }
    int cornerMask() const;
    int logicalCorner(int physicalCorner) const { return physicalCorner & cornerMask(); }
    MorphPosition constrain(const MorphPosition& position, const MorphPosition& fallback) const;

    static constexpr std::array<int, 3> axes { Vertex::Time, Vertex::Red, Vertex::Blue };
    static constexpr std::array<int, 3> poleBits { 4, 2, 1 };

    // Called only at a durable dimensionality boundary, never during movement/audio.
    static void project(Mesh& mesh, MorphDimensions retained, const MorphPosition& position,
            GuideCurveProvider* guides = nullptr, std::vector<int>* retainedCubeIndices = nullptr);

private:
    int count;
};
