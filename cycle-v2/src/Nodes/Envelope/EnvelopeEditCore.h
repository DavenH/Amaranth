#pragma once

#include <array>
#include <JuceHeader.h>

class EnvelopeMesh;
class EnvRasterizer;
class VertCube;
class Vertex;

namespace CycleV2 {

struct EnvelopeEditScope {
    int dimensionCount { 2 };
    std::array<bool, 3> linkedAxes { true, true, true };
};

class EnvelopeEditCore final {
public:
    static constexpr float maximumPhase = 10.f;

    static int fieldDimension(const juce::String& field);
    static juce::Array<Vertex*> linkedVertices(
            VertCube* cube, Vertex* vertex, const EnvelopeEditScope& scope);
    static bool setValue(VertCube& cube, int corner, int dimension,
            float value, const EnvelopeEditScope& scope, bool* changed = nullptr);
    static bool setGuideGain(VertCube& cube, int dimension,
            float value, int dimensionCount, bool* changed = nullptr);
    static bool markerState(const EnvelopeMesh& mesh, const VertCube& cube, bool loop);
    static bool setMarker(EnvelopeMesh& mesh, VertCube& cube,
            bool loop, bool enabled, EnvRasterizer& rasterizer);
    static bool validateMarkers(EnvelopeMesh& mesh, EnvRasterizer& rasterizer);
    static bool synchronizeLoopSeam(
            EnvelopeMesh& mesh, EnvRasterizer& rasterizer, bool sourceIsLoop);
    static void transferMarkers(
            EnvelopeMesh& mesh, const VertCube& source, VertCube& first, VertCube& second);
};

}
